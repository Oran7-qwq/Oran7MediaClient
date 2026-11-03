---
status: ✅ 已核对
updated: 2026-10-24
---

# 异步与线程（ASYNC）

> 本文回答：哪些代码在哪个线程跑、跨线程怎么传数据、线程池怎么用、退出时怎么收尾、哪里现在还会踩坑。
> 线程清单的"模块视角"见各模块文档；这里是横向规则与契约。

## 1. 两个门面，共享一个线程池

| 门面 | 所在位置 | 执行资源 | 返回值 | 完成回调 |
|---|---|---|---|---|
| Oran7Async::run(context, work, done, group) | Oran7Core | 唯一的 Oran7Async::pool() | Oran7TaskHandle | context 所在线程 |
| Oran7AsyncManager::run(op, args, completed, failed) | EXE / QML 单例 | 委托 Oran7Async | 整数 taskId | GUI 线程的 QJSValue 回调 |

General 组用于扫描、删除与播放器销毁；Network 组用于 QML 的 musicSearch，以及 C++ CloudMusicService 的 fetchLikedSongs/roamSongs，最多两个并发。网络工作先排队，再由池内最多两个执行器消费，不用信号量阻塞额外工作线程。池大小为 max(4, idealThreadCount())，为通用工作保留执行空间。

C++ 可取消 Oran7TaskHandle；QML 可 cancel(taskId)/cancelAll()。取消跳过尚未开始的工作，运行中工作自行返回后丢弃回调，不强杀 curl。runDetached 用于无需回 GUI 的普通工作；P2-2 日志已迁至 Oran7LogSink 专用线程，不占用业务池。

池、分组队列和退出标志都由 Oran7Core 唯一持有。原有 QML 私有线程池已删除。

## 2. 线程与数据通路

### 2.1 播放内核

```
GUI ──put req(REQ_START/PAUSE/SEEK)──▶ MessageQueue ◀──put event── read/refresh/audio 线程
                                          │
                                          ▼
                                    msg_thread_（Oran7MediaPlayer 内部）
                                          │ 拦截 REQ_* 自己执行
                                          └─其余→ Controller 静态 messageLoop → 共享值邮箱 → GUI
```

- 队列**无界**，不阻塞生产者；`abort_request=1` 时新消息被丢弃（`FFMsgQueue.cpp:15-50`）。
- START/PAUSE 采用"先删同类消息再入队"= last-writer-wins（`Oran7MediaPlayer.cpp:152-178`）。
- Oran7PlaybackController::messageLoop 只持有所属 Native、固定代际和共享邮箱，读取状态/时长快照回 GUI。析构先失效邮箱 receiver，旧事件按代际丢弃；不执行 Client/Controller 实例成员。
- 视频帧出核：`video_refresh` → 渲染桥共享最新帧邮箱 → GUI 取帧 → `D3D11VideoItem::submitFrame` → `update()`；回调只捕获邮箱/代际，不访问 Client。
- 音频：SDL 回调线程里做重采样 + Sonic + 时钟推进。
- 音频设备由 `Oran7SdlAudioDevice` 独占，内部控制锁串行 open/pause/close；回调不进入该控制锁。关闭时先清空所属句柄，再直接调用 SDL 的关闭接口，不持有 SDL 回调锁，不在关闭后操作旧 ID。普通切歌仍后台销毁播放器，GUI 不再逐次调用 `HeapCompact`。
- 快速切歌只保留最新打开目标；旧 Native 实例后台完全释放后才创建新实例，300ms 定时器到期不代表退役已完成。GUI 不等待后台 join；停止/退出会取消待打开目标。FFmpeg 网络读取通过原子退出标志和 `interrupt_callback` 取消，禁止在读线程 join 前跨线程 `avio_closep`；`avformat_close_input` 在 join 后统一释放输入与 I/O。

**线程安全规则（现状）**：`abort_request`、`audio_volume`、`destroying_`、`mp_state_` 和播放器 isPlaying 是原子；`seek_req/seek_pos`、`paused`、`eof`、`Audio_EOF/Video_EOF`、`All_finished_`等**不是**，靠"读写恰好都在同一线程"或"容忍脏读"维持。新增字段必须显式选择：原子、加锁、或限定单线程。

### 2.2 录屏/推流

| 生产者 | 消费者 | 机制 | 背压 |
|---|---|---|---|
| 采集线程 `DdaGrabWorker` | 渲染项 | `newFrameReady(idx)` **DirectConnection**（故意：串行化，避免 GUI 一次吃掉两帧） | 共享纹理池 3 张，**无 fence/keyed mutex** |
| 采集线程 | 视频编码器 | `_frameQueue` 容量 30 | 丢最旧帧 |
| 编码线程 | 封装/推流 | `_pktQueue` 容量 15 | 视频**阻塞**生产者；音频丢包（容量 10） |
| WASAPI 线程 | 音频桥线程 | `std::queue` 容量 10 | 丢最旧 |
| 音频桥 | 音频编码器 | 容量 3 | 丢最旧 |

P1-6：startRecording/stopRecording 只提交请求，首帧/编码器/WHIP 等待和 join/flush 在独立 Oran7RecordingSession 任务线程执行；ICE 回调只入队，由发布线程串行 HTTP，信令 stop 不再嵌套事件循环。Feed mutex 保护硬件设备引用、尺寸与编码入口。仍保留预览 teardownWorker 的同步 wait、显示器枚举和抓屏参数重启/回滚；退出时也同步等待，详见 [[Capture - 录屏与推流]]。

### 2.3 渲染

- 生产者：解码线程 / 采集线程 → `submitFrame`（`QMutex m_frameMutex` + 队列容量 2）。
- 渲染线程：`beforeRendering`（DirectConnection）与 `updatePaintNode` 双路驱动，同一个 `setFrame`；`QSGRenderNode` 里做 NV12/P010→BGRA 的 `VideoProcessorBlt`，原生纹理通过 `QRhiTexture::createFrom(NativeTexture)` 包装，不拷贝。
- 跨线程传递的只有 `AVFrame*`（所有权转移）、`std::atomic<AVFrame*> m_latestFrame`、若干原子标志；`m_contentRect` 等由 GUI 写、渲染线程读，依赖 `updatePaintNode` 在线程边界上的同步语义。
- `Oran7MediaClient::m_d3d11Slots`（`QHash`）会被渲染线程的 `OutputVideo` 读到 → 未加锁的容器跨线程读，属于潜在 UB。

### 2.4 网络

- B 站认证、弹幕、歌词的 `QNetworkAccessManager` 仍在所属 GUI 线程。ncm API/Session 也由 GUI 线程持有；但 `CurlHttpClient` 的 `Oran7CurlHttpWorker` 在专用 QThread 拥有 QProcess/临时文件，进程创建、完成解析与清理均在后台，结果显式排队回前端。`QProcess::start` 在 Windows 的 CreateProcess 阶段可能同步停顿，禁止再放回 GUI。服务析构时才 quit/wait，先后台终止子进程再释放临时文件。
- 唯一的例外：`ncm::blockingCurl`（线程池里同步跑 `curl.exe`）与 WHIP 的 `onLocalCandidate → whipPatchIce()`（在 libdatachannel 线程里做阻塞 HTTP）。
- 线程池任务拿不到任何 QObject：Cookie/CSRF 由 GUI 线程快照为值传入；搜索仍由 QML 提交，P1-7 后收藏/漫游由 Oran7CloudMusicService 提交。

## 3. 契约

### 3.1 `Oran7Async::run`

```cpp
Oran7Async::run(this,
    []() -> Result { /* 后台：禁止碰 QObject/QML/Oran7MediaClient */ },
    [this](Result r) { /* GUI：可以 emit / 改 QML */ });
```

- `work` 的返回值必须可拷贝/可移动；`done` 通过 `QFutureWatcher` 回到 `context` 线程。
- run 返回 Oran7TaskHandle；取消/上下文销毁后不调用 done，退出后返回无效句柄。已接受的必要清理工作仍会排空。
- work 异常会经 QPromise 传回并记录，不调用成功回调；需业务失败回调的操作应像 QML 门面一样返回明确结果。
- 不要捕获裸 `this` 到 `work` 里；`Oran7MediaClient` 用 `m_destroyedFlag`（`shared_ptr<atomic<bool>>`）做守卫。

### 3.2 `Oran7AsyncManager::run`

```qml
Oran7AsyncManager.run("musicSearch",
    { keywords: kw, cookie: NcmSession.cookieString, isLoggedIn: NcmSession.isLoggedIn },
    result => { /* GUI */ }, error => { /* GUI */ })
```

- handler 返回业务 QVariant；门面包装 success/result/error，异常转换为 failed(error)。
- run 返回 taskId，cancel(taskId) 在任务仍在跟踪时返回 true；取消不执行 completed/failed。
- 未知 operation → 立即 `failed("未知后台操作：...")` 并返回 `-1`。
- 参数必须是**纯值**（`QVariantMap`），不能传 QObject/QJSValue 进去。
- 现有 handler：`test` / `deleteMusic` / `musicSearch` / `fetchLikedSongs` / `roamSongs`（`Oran7AsyncManager.cpp:20-43`）。

## 4. 退出时序（唯一正确顺序）

`main.cpp:301-338`：

```
stopPlayerRunning()            // 先停渲染回调，防 UAF
Oran7Async::beginShutdown()    // 冻结业务池，日志继续独立接收
Oran7AsyncManager::beginShutdown()  // 取消 QML 任务，共享池稍后统一排空
Oran7BilibiliAuthManager::saveCookies(); cleanup()
Oran7NetworkMonitor::cleanup()  // 必须先于 NcmSession
NcmSession::cleanup()
Config_AppConfigManager_Save()
Oran7Async::waitForDone()
旧 TCP 模块默认停用（不遍历 Client 子树操作 Socket）
（post routine）Oran7Theme::cleanup()
引擎/应用销毁后 scope guard → Oran7LogSink::shutdown() // 拒绝新日志、排空、join
```

任何新增后台线程都必须在这里有明确的"停 + join"位置，否则退出会崩在静态析构。

## 5. 已知陷阱清单

| 陷阱 | 位置 | 现象 |
|---|---|---|
| 已修复：持有音频回调锁关闭设备 | 原 `Oran7FFplayerCore::audio_close` | 音频线程等待锁、关闭线程等待音频线程，切歌时可能死锁；关闭后再 unlock 还可能误用已复用 ID。`tests/playback/sdl-audio.cpp` 使用真实 SDL/dummy 驱动在受控子进程重现旧死锁，并检查新设备生命周期与并发控制。 |
| GUI 线程做阻塞打开 | `oran7mp_prepare_async()` 里同步 `stream_open` | 打开网络流时界面卡顿（"异步"名不副实） |
| 历史：消息回调 sleep 10ms | P1 首批已移除 | 状态消息返回 GUI 后按代际更新 |
| 后台线程读 GUI 容器 | `Oran7MediaClient::OutputVideo` 读 `m_d3d11Slots` | 低概率崩溃/哈希表损坏 |
| 历史：采集 static 跨 worker 代际 | P0 已改为 worker 成员 | cleanup 释放 SwsContext 并复位诊断/时间戳 |
| 已修复：libdatachannel 回调中阻塞 HTTP | P1-6 共享事件队列 | 发布线程串行发送候选者；回调不捕获 publisher 裸 this |
| 已修复：录制首帧无界等待 | P1-6 独立任务线程有界等待/取消 | GUI 不再轮询；失败在资源清理后回 Error |
| 阻塞 curl 的取消 | Network 组并发已限 2 | 运行中仍需等待既有超时，不强制终止 |
| 日志在析构里再投递任务 | `Oran7Logger::~Oran7Logger` | 已用 `isShuttingDown()` + 同步回退兜住，改动时别破坏 |
| 主题清理 | P0 已统一到插件 DLL 的唯一实现 | QML 引擎销毁后由 post routine 清理 |

## 6. 目标形态（重构建议）

1. **已落地共享池与分组**：后续按实际负载再增加优先级，不新增第二个池。
2. **播放器改为 `Oran7PlaybackEngine : QObject`**：请求通过 queued signal 下发，状态用 `Q_PROPERTY + NOTIFY` 回到 GUI；P1-2 第五批已用 Oran7PlaybackController + 共享值邮箱移除 Client 成员跨线程执行；更深层内核状态统一仍待后续。
3. **状态原子化**：所有跨线程可见字段统一 `std::atomic` 或 `QMutex` 保护，并在头文件注释里标注归属线程。
4. **已落地 Oran7TaskHandle**：排队任务可跳过，运行中任务抑制回调；底层 I/O 中断另行设计。
5. **P1-6 录制已异步化**：开始/停止、WHIP 建连和资源收尾通过请求与状态通知执行；预览/参数重启的剩余同步等待另行处理。
6. **共享内存同步**：采集纹理池加 keyed mutex（或改为 3 缓冲 + fence），消除撕裂。
