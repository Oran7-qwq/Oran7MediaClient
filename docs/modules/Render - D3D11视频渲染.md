---
status: ✅ P2-7 代码实施完成，真实设备待验收
updated: 2026-11-11
---

# Render：QRhi 视频渲染与采集同步

源码 `src/cpp/Oran7VideoGPURender/` 编译进 EXE。保留 D3D11VideoItem 的 QML 类型、Client 渲染接口、Direct3D11 后端、basic 渲染循环和透明窗口约束。2026-11 已补充高位深、PQ/HLG 与主窗口 scRGB HDR 输出；旧 P2-7 的同步和所有权边界继续保留。

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
  → NV12/P010/P016 平面视图、共享 BGRA 复制或 CPU 高精度上传
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

截图与构建/CTest 记录放在检查目录的 tests/artifacts/render_sync_checks 和 build/refactor-work/P2-7。软件检查不能替代真实 D3D11 播放、Seek、缩放、翻页、切屏/分辨率、连续录制及退出验收，也不能据此承诺实际吞吐或无撕裂。WHIP 与录屏输出格式不扩展；2026-11 的 HDR 播放新增部分见下文。

## API 依据

- [Qt QSGRenderNode：prepare 与 render 的阶段边界](https://doc.qt.io/qt-6/qsgrendernode.html)
- [Qt QRhi 自定义节点示例](https://doc.qt.io/qt-6/qtquick-scenegraph-customrendernode-example.html)
- [Microsoft keyed mutex 与共享表面](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/surface-sharing-between-windows-graphics-apis)
- [AcquireSync：返回值与零超时语义](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-acquiresync)

实际 API 同时对照本地 Qt 6.7.3 头文件及软件渲染检查，不据在线新版本示例升级项目 Qt。

## 2026-11：高位深与 HDR 播放

原路径将所有 D3D11 NV12/P010 都按 BT.709 limited 转成 BGRA8，不区分 BT.2020、PQ/HLG；软件路径同样固定 BGRA8。位深截断、矩阵错误和缺少 HDR 映射会共同造成失真，不能仅用提高饱和度修复。

- Oran7VideoColorSpace 从 AVFrame 的位深、范围、矩阵、原色、传递函数和 Mastering/MaxCLL 取快照。解码器只补齐帧中缺省的流色彩字段，不覆盖逐帧值；不支持硬解配置时选用 FFmpeg 提供的软解格式。保留 Main10/高位深，不指定 8-bit 中转。
- D3D11 NV12/P010/P016 复制正确的数组 slice 到节点独占的单层纹理，QRhi R8/RG8 或 R16/RG16 导入 Y/UV 平面；可见范围与对齐编码尺寸分开。原共享 BGRA/keyed mutex/租约路径不变。硬件平面导入失败才下载原位深并走 FP16 RGB 回退，不将 PQ 直接解释成 SDR。
- CPU YUV 用 swscale 整理为 NV12/P016，RGB 高位深经 RGBA64 转 FP16，范围和矩阵按帧设置；HDR 的传递函数与色域变换留给 GPU。常用 BT.601/709/2020 NCL、full/limited 分别处理；PQ 解为绝对 nit，HLG 以 1000 nit、gamma 1.2 的参考显示处理。
- HDR 输出采用线性 BT.709/scRGB 浮点空间，保留超出 1 和表示广色域的负值。SDR 输出使用亮度驱动的扩展 Reinhard 与色域压缩，峰值优先 MaxCLL、其次母版峰值、缺省 1000 nit。8-bit ShaderEffectSource 单独映射 SDR，不能令主 pass 一起失去 HDR。

Oran7HdrWindowOutput 是 main.qml 的主窗口局部对象，不是单例。只给该窗口请求 Qt 6.7.3 的 `_qt_sg_hdr_format=scrgb`，不设置全局 QSG_RHI_HDR；实际表面必须为 FP16 才启用。SDR 屏幕、Windows HDR 关闭或后端不支持时使用 SDR。

Qt 6.7 的普通 QML 颜色仍以 sRGB 数值合成。原生 HDR 模式下，视频先以扩展 sRGB 值写入 FP16 场景，Oran7HdrCompositor 在 afterRendering 对最终画面线性化一次、保持预乘透明度，然后由 QRhi 提交 scRGB 输出，避免界面灰色变白。Windows 的 SDR 内容亮度通过 DisplayConfig 查询，界面白色随之调整，视频绝对亮度不随此滑块改变。该步骤增加一份窗口大小的 FP16 缓冲和一次全屏合成。

该输出兼容固定 Direct3D11/basic/透明窗口。Qt 6.7.3 的私有 D3D11 结构只用于取得实际表面格式、当前 RTV 和交换链重建登记字段；媒体帧仍只在节点 prepare 消费，最终绘制仍由 QRhi 完成。原生复制使用 beginExternal/endExternal 包围，先执行 Qt 已记录的绘制命令；资源创建与更新不放进 render。显示设置变化/跨屏移动防抖后，在完整帧结束处重建同一交换链，保留 HWND/播放器/几何；失败有界回退 SDR，不循环重试。此兼容层升级 Qt 时必须重新核对。

新增 `hdr_video_checks` 使用 WARP、真实生产节点和离屏 FP16 目标检查 PQ/HLG、1000 nit 高光、10-bit 相邻色阶、广色域负值、CPU/P010 数组 slice 一致、8-bit 离屏和主 HDR 多 pass、SDR gamma/透明度、Windows 白色比例及清屏。与原 render_sync_checks 一起运行：

```powershell
cmake --build --preset checks --target render_sync_checks hdr_video_checks
ctest --preset checks -R render-sync
```

真实屏幕峰值、DWM/驱动、跨屏切换和高分辨率吞吐由用户验收。当前不解析 Dolby Vision、HDR10+ 动态元数据，也不把录屏/WHIP 改成 HDR；BT.2020 constant-luminance、ICC 任意显示配置不属于此版支持范围。

## 2026-11：停止后的缓冲归还

清屏的本帧仍只改变 black 状态，避免破坏尚未提交的多 pass 绘制。Oran7VideoFrameMailbox 用独立 releaseSerial 标记清屏，节点在 afterFrameEnd 归还旧 GPU 纹理、pass/SRB/pipeline、CPU 转换帧和 swscale 缓冲；页面隐藏但主窗口仍绘制时也能执行，不依赖节点继续 prepare。清屏后若同帧已成功准备新图像，视为新会话资源，不能误释放。再次绘制纯黑只在 prepare 创建 1×1 纹理。主窗口完全不提交帧时释放延迟到下一帧；beforeRendering 仍只推进帧号，render 仍不创建或销毁资源。

更多取证和验证见 `../PERF - 内存增长排查.md`。本项针对停用后保留上一段大尺寸资源，不代表所有驱动/GUI 缓存的长期增长已归因。
