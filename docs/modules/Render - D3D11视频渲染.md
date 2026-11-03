---
status: ✅ P2-7 代码实施完成，真实设备待验收
updated: 2026-10-26
---

# Render：QRhi 视频渲染与采集同步

源码 `src/cpp/Oran7VideoGPURender/` 编译进 EXE。保留 D3D11VideoItem 的 QML 类型、Client 渲染接口、Direct3D11 后端、basic 渲染循环和透明窗口约束。HDR 为后续功能，本阶段继续使用 BGRA8 SDR 输出。

## 职责和所有权

| 对象 | 职责 |
|---|---|
| D3D11VideoItem | GUI 几何、源尺寸/信息信号、场景图更新；不持有 GPU 设备或转换器 |
| Oran7VideoFrameMailbox | 受 mutex 保护的最新帧、清屏代际、帧率与完整信息快照；通知合并为一个 GUI 投递 |
| D3D11VideoRenderNode | 渲染线程拥有原生转换资源与 QRhi 纹理、顶点、采样器及每个 pass 的 uniform/SRB/pipeline |
| Oran7CaptureTextureSlot | 池与 AVFrame 共同持有的纹理租约，含纹理及 keyed mutex |
| D3D11DeviceProvider | Qt 设备的兼容发布入口；更换宿主断开旧连接，同宿主场景图重建保留订阅 |
| Oran7RenderBridge | Client 持有的 GUI 协调层，继续负责播放请求代际、宿主挂载、Fit/Fill、信息来源标记 |

原有 D3D11 类型名称保持兼容，不新增全局单例。新增自有类型统一 Oran7 前缀。

## 唯一帧消费路径

```text
播放器 → RenderBridge 请求代际邮箱 → GUI → Item 邮箱
采集线程 → 捕获于 GUI 的 frameSubmitter 值回调 → Item 邮箱
                         ↓ 合并通知
GUI：update() → updatePaintNode（只同步几何、创建/标脏节点）
                         ↓
QRhi 节点 prepare()：每个 Qt 帧最多取一次最新媒体帧
  → D3D11 NV12/P010 转换、共享 BGRA 复制或 CPU BGRA 上传
  → 导入 QRhiTexture，准备顶点及各 pass 的 uniform/SRB/pipeline
  → commandBuffer()->resourceUpdate（渲染 pass 开始前）
                         ↓
render()：使用已准备资源，设置 viewport/scissor/stencil，draw
```

beforeRendering 仅推进渲染周期编号，不读取或消费媒体帧。Qt 一帧中即使因 ShaderEffectSource 多次调用 prepare/render，也只消费一次，期间新到的帧留给下一 Qt 帧。不会因为主画面和离屏 pass 先后运行而抢走不同帧。

停止会丢弃邮箱内旧帧、递增代际并提交清屏状态；旧转换结果不能重新发布元数据。清屏由 QRhi 着色器状态完成，保持各 pass 一致。Item 析构在邮箱锁内先失效接收者；晚到回调只持共享邮箱，直接释放帧。

## QRhi 资源与布局

- 所有资源创建、纹理导入和数据更新在 prepare；render 不再分配资源或发出复制/更新命令。
- 每个目标独立持有 uniform/SRB/pipeline，保存兼容 render-pass descriptor 并比较 sampleCount，避免主画面与离屏矩阵互相覆盖。历史目标缓存超过上限时在下一 Qt 帧首个 pass 前清理。
- 使用 Qt 提供的 projectionMatrix × matrix，不再依据目标尺寸/scissor 猜测离屏投影；Fit/Fill 像素对齐仍由 RenderBridge 计算 contentRect。
- uniform 明确使用 float[16] 矩阵、opacity 与 black 字段，编译期核对偏移/大小；不能 memcpy 整个 QMatrix4x4（其内部还有标志）。着色器输出按 Qt 继承透明度预乘合成。
- 节点析构与 releaseResources 都在场景图线程释放 GPU 资源；Item 的 releaseResources 不释放 GPU 对象。每帧转换完成便释放输入 AVFrame，不长期占住解码器或采集池。

## 采集纹理同步

原来的“共享句柄通知 GUI → GUI 打开纹理池 → worker 直接调用 Item”链路已移除。worker 在 GUI 创建时取得只持共享邮箱的提交回调，运行期不访问 Item/Controller 的 QObject 状态。

池保持三张 BGRA8 纹理，使用 D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX：

1. 生产者只选择 use_count == 1 的槽位，确保没有未消费或克隆的 AVFrame 租约；AcquireSync(0, 0) 成功后复制，Flush 后 ReleaseSync(0)。
2. 发布的 AVFrame 通过 AVBufferRef 拥有槽位共享引用，覆盖、取消、尺寸变化或退出均依靠 RAII 释放；重建池不销毁仍被旧帧持有的纹理。
3. QRhi 节点在 Qt 设备上 OpenSharedResource，AcquireSync(0, 0) 后复制到自己的纹理，再 ReleaseSync(0)，最后释放输入帧。GPU 读写顺序由 keyed mutex 保证，CPU 槽位复用由租约保证。
4. WAIT_TIMEOUT 是正常忙状态，零超时跳过预览帧；不能仅用 SUCCEEDED 判断 AcquireSync。其他同步失败转入既有有限采集恢复；不使用 INFINITE。
5. 预览池忙不跳过原始帧的录制转换/编码路径。预览启停原有 wait/join 和参数回滚仍保留，不宣称整个采集模块已无 GUI 等待。

同设备 FFmpeg 硬解帧遵守 AVD3D11VADeviceContext 的递归上下文锁，跨设备共享只接受可同步的纹理。以上不是跨适配器传输方案，真实双显卡/外接屏仍需用户验收。

## 信息发布

节点仅在成功准备图像后构造 Oran7VideoInfo 完整值，连同清屏代际提交邮箱。源尺寸变化立即通知，其余信息按 1 Hz 节流；GUI 排空后发出原有信号，RenderBridge 再补充 fillModeName/sourceTag。GUI 不读取节点的 QString、格式字段或 GPU 对象，也不把复合值拆成多个原子量。FPS 仍表示输入提交统计，不是录像文件的标称帧率。

## 验证与边界

新增 tests/render-sync，使用两个 WARP 软件 D3D11 设备、实际 QQuickRenderControl/QRhi/生产节点和合成帧，不显示主程序窗口、不使用硬件 GPU、不采集桌面/声音、不访问真实配置或凭据。

覆盖：有界最新帧、清屏旧代际拒绝、完整信息、克隆租约、接收者销毁后的晚到提交、并发帧序；真实像素的连续更新、透明度、Fit、清屏/恢复、尺寸变化、页面重挂载、scissor、ShaderEffectSource 多 pass，以及 pass 间注入新帧仍保持单帧一致；跨设备共享纹理、零超时忙槽、设备宿主切换与重建订阅。

截图与构建/CTest 记录放在检查目录的 tests/artifacts/render_sync_checks 和 build/refactor-work/P2-7。软件检查不能替代真实 D3D11 播放、Seek、缩放、翻页、切屏/分辨率、连续录制及退出验收，也不能据此承诺实际吞吐或无撕裂。WHIP 不扩展，HDR/PQ/HLG、10-bit/浮点输出和色调映射另行实施。

## API 依据

- [Qt QSGRenderNode：prepare 与 render 的阶段边界](https://doc.qt.io/qt-6/qsgrendernode.html)
- [Qt QRhi 自定义节点示例](https://doc.qt.io/qt-6/qtquick-scenegraph-customrendernode-example.html)
- [Microsoft keyed mutex 与共享表面](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/surface-sharing-between-windows-graphics-apis)
- [AcquireSync：返回值与零超时语义](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-acquiresync)

实际 API 同时对照本地 Qt 6.7.3 头文件及软件渲染检查，不据在线新版本示例升级项目 Qt。
