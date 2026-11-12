---
status: ✅ 已核对
updated: 2026-10-24
---

# Player：播放内核

> 源码 `src/cpp/Oran7MediaPlayer/`　产物：编进 EXE
> FFmpeg 6.0（+SDL2）实现的 ffplay 派生内核，加上 B 站直播地址解析与弹幕客户端。

## 职责与边界

- 解封装、解码、A/V 同步、音频输出、seek、变速变调、硬件解码协商。
- 把解码后的视频帧交给渲染层（`D3D11VideoItem`），把播放事件交给 `Oran7MediaClient`。
- **不承担**：界面状态（在 `Oran7MediaClient`/QML）、播放列表决策（在 QML）、渲染（在 `Oran7VideoGPURender`）。

## 类清单

| 类 | 文件 | 职责 |
|---|---|---|
| `Oran7MediaPlayer` | `.h:105-215` | 门面：状态、消息线程、`data_source`、C 风格 `oran7mp_*` 接口 |
| `Oran7FFplayerCore` | `Oran7FFplayerCore.h:22` | 引擎：解封装线程、解码器编排、A/V 同步、SDL 音频、seek、硬解、消息投递 |
| `Oran7FFmpegDecoderCore` | `.h:14` | 单流解码：解码线程 + 音/视频线程函数 + `queue_picture` |
| `MessageQueue` / `AVMessage` | `FFMsgQueue.h:22/:9` | 无界链表队列（未满阻塞不存在），节点回收 |
| `PacketQueue` | `FFPlay_Def.h:80` | 包队列 + `serial`（seek 世代） |
| `FrameQueue` / `Frame` | `FFPlay_Def.h:122/:110` | 环形帧缓冲（视频 4、音频 9，`keep_last=1`） |
| `Clock` | `FFPlay_Def.h:136` | pts/drift 主时钟 |
| `Sonic` | `Sonic.h:67` | 变速变调（时间伸缩） |
| `BilibiliDanmakuClient` | `BilibiliDanmakuClient.h:25` | B 站直播弹幕 WS 客户端（QML 类型） |
| `BilibiliRoomAddressCatch` | `BilibliLiveRoomAddressCatch.h:14` | 直播间 → 可播放流地址（QML 类型） |

P1 首批：播放领域状态通过 Client.playback 暴露，消息状态/时长以值快照回 GUI 后按代际校验，消息循环不再固定 sleep。FFmpeg 解码和帧队列逻辑未重写。

## 对外接口

### `Oran7MediaPlayer`（C++ 门面，`Oran7MediaPlayer.h:105-215`）

| 方法 | 说明 |
|---|---|
| `oran7mp_create(std::function<int(void*)> msg_loop)` | 建 core + 初始化消息队列 + 存回调（**重复调用会泄漏旧 core**） |
| `oran7mp_destroy()` | 幂等（`destroying_` CAS）；停流 → 断队列 → join 消息线程 → 释放 |
| `oran7mp_set_data_source(url)` | 先复制成功，再释放旧数据源（P0 已修复） |
| `oran7mp_prepare_async()` | 置 `ASYNC_PREPARING`、起消息线程、同步 `stream_open` |
| `oran7mp_start()/pause()/seek_to(msec)` | 投递 `FFP_REQ_*`（先删同类再入队 = last-writer-wins） |
| `oran7mp_get_state()` | 返回原子 mp_state_；事件携带发生时的状态值 |
| `oran7mp_get_current_position(_double)()` | 主时钟秒 |
| `oran7mp_get_duration()` | `ic->duration` 毫秒 |
| `oran7mp_get_msg(AVMessage*, block)` | 取消息并**拦截执行** REQ_* |
| `oran7mp_set_playback_volume(percent)` | 缓存 + `ffp_update_reqVolum` |
| `oran7mp_set_playback_rate(double)` | 变速（**当前无调用方**） |
| `AddVideoRefreshCallback(fn)` | 注入帧回调 |
| `oran7mp_setD3D11Device(dev)` | 注入 Qt 的 D3D11 设备供硬解复用 |
| `oran7mp_get_playable_duration/set_loop/get_loop` | **声明但未实现** |

### QML 类型

`BilibiliDanmakuClient`（`main.cpp:186`）

| 成员 | 说明 |
|---|---|
| `isConnected` | 认证握手完成 |
| `active` | 用户希望保持连接（start/stop 语义） |
| `viewerCount` | op=3 心跳回包的人气值 |
| `start(roomId)` / `stop()` | 可重复调用，内部 `m_session` 世代号让旧请求失效 |
| `danmakuReceived(text, uname, color, timestamp)` | 普通弹幕 |
| `superChatReceived(text, uname, price, color)` | 醒目留言 |
| `connectionChanged/activeChanged/viewerCountChanged/errorOccurred` | 状态 |

`BilibiliRoomAddressCatch`（`main.cpp:185`）

| 成员 | 说明 |
|---|---|
| `avliStrAdr` | 候选地址列表（`[0]` 为最优，**按引用返回**） |
| `selectedCodecName` / `selectedFormatName` | 选中流的编码/格式名 |
| `getRoomInfo(roomId)` | 异步；**立即返回空 QVariant**，结果靠信号 |
| `urlsReady()` / `urlsError()` / `avliStrAdrChanged()` | 结果通知 |

## 线程模型

| 线程 | 创建处 | 工作 |
|---|---|---|
| `msg_thread_` | `Oran7MediaPlayer.cpp:136` | `oran7mp_get_msg` → `Oran7PlaybackController::messageLoop` 静态循环；所属实例快照经共享邮箱排入 GUI，线程不捕获 Client/Controller |
| `read_thread_` | `Oran7FFplayerCore.cpp:846` | `open_input`/`find_stream_info`/开组件/读包/seek/EOF |
| `auddec.decoder_thread_` / `viddec.decoder_thread_` | `Oran7FFmpegDecoderCore.cpp:53-55` | 解码入 `sampq`/`pictq` |
| `video_refresh_thread_` | `Oran7FFplayerCore.cpp:357` | ≤10ms 轮询做同步、取帧、回调 `OutputVideo` |
| SDL 音频回调 | `SDL_OpenAudioDevice` | 重采样、Sonic、写设备、推进音频时钟 |
| 后台销毁 | `Oran7Async` 池 | Controller 移交 Native unique_ptr 至后台闭包，暂停/短暂排空后释放；文件任务等退役计数归零 |

跨线程只通过：消息队列、包队列、帧队列、原子标志、`QMetaObject::invokeMethod`。**没有**互斥保护的共享字段见 [[ASYNC - 异步与线程]] §5。

## 状态机

```
IDLE ──prepare_async──▶ ASYNC_PREPARING ──(FFP_MSG_PREPARED)──▶ 等 REQ_START
                                   └─失败──▶ ERROR
REQ_START ─▶ STARTED(isPlaying=true)      REQ_PAUSE ─▶ PAUSED(isPlaying=false)
```

未赋值的状态：`PREPARED / STOPPED / COMPLETED / END / IDLE`（`MP_STATE_*` 宏只在头文件注释里存在）。
`FFP_MSG_PLAYBACK_STATE_CHANGED` 携带发生时状态，Controller 在 GUI 线程校验代际后写入 Oran7PlaybackState；QML 从 Client.playback 只读绑定。

EOF 判定：`eof=1` + `Video_EOF`（`pictq` 空） + `Audio_EOF`（音频回调静音路径）→ `check_play_finished()` → `FFP_MSG_PLAY_FNISH`。
⚠️ EOF 后解码线程已退出，**seek 无法恢复播放**，单曲循环是 QML 里 `seek(0)` 特判实现的。

## 关键策略

| 策略 | 实现 |
|---|---|
| 主时钟 | `AV_SYNC_AUDIO_MASTER`，无音频时退化视频；`get_clock` 带 drift 修正，seek 后 serial 不匹配返回 `NAN` |
| 音频时钟 | 回调内 `pts + nb_samples/rate`，再减去两个硬件缓冲（双缓冲 SDL） |
| 视频同步 | ffplay 的 ±40/70ms 阈值 + 重复帧抑制 + 迟到丢帧 + serial 不匹配丢帧 |
| 刷新节拍 | `REFRESH_RATE 0.01`，每轮 sleep 剩余时间 |
| seek | 异步世代：`REQ_SEEK` → `avformat_seek_file`（失败再用 `BACKWARD` + 10s 容差）→ flush 双队列 + `flush_pkt`（serial++）→ `SEEK_COMPLETE`；直播流拒绝 |
| 变速变调 | Sonic（`sonicSetSpeed`，pitch=1），仅 S16/FLT、实际立体声；视频 pts 除以 rate 补偿；**当前未接入 UI** |
| 硬件解码 | 启发式（4K / 1080p≥59fps / HEVC·AV1·VP9 / >14Mbps）→ `d3d11va` 优先，复用 Qt 的 `ID3D11Device`；`dxva2` 分支是空实现（实际软解） |
| 硬解帧传递 | 不下载到 CPU：`AVFrame.data[0]` 就是 D3D11 纹理，直接给渲染层做 `VideoProcessorBlt` |
| 网络选项 | UA/Referer/`reconnect=1`/`stimeout=5s`；按域名注入 B 站/网易云 Cookie |
| 音视频不齐 | `compute_target_delay` 用帧时长与同步偏差修正；`max_frame_duration` 防跳变 |

## 数据流（播放一首歌）

见 [[DATAFLOW - 数据路由]] §1。关键节点：`prepare_async` → `stream_open`（**同步在 GUI 线程**）→ `read_thread` → `PREPARED` → `REQ_START` → SDL 出声；视频经 `video_refresh` → `OutputVideo` → `D3D11VideoItem`。

## 易踩坑

1. P0 已修复：数据源/消息线程指针默认 nullptr，状态默认 IDLE、音量默认 25；重复设置数据源时释放旧地址。
2. `stream_close` 先关 `ic->pb` 再 join 读线程（`Oran7FFplayerCore.cpp:871-877`）——**故意**，否则 `av_read_frame` 阻塞导致切歌卡死。
3. `read_thread` 的异常必须投递 `FFP_MSG_ERROR`，否则 `message_loop` 永远阻塞在 `msg_queue_get`（"既不切歌也不报错，静默挂死"）。
4. 旧播放器消息线程会继续投递：Controller 按创建时固定代际丢弃旧事件；邮箱在 Controller 析构前失效，投递不依赖 Client 生命周期。
5. `static` 计数器留在热循环里（`submitCount`、`put_video_hw_frame_num`、`run_times`）。
6. `hw_device_ctx` 的 `av_buffer_ref` 没有对应 unref；`video_refresh_thread_` join 后没 `delete`。
7. `FFP_REQ_SEEK` 参数被 `(int)` 截断，而内核其余部分用 `int64_t` 微秒。
8. 两处 `#define FFP_MSG_COMPONENT_OPEN` 重复（`ffmsg.h:17-18`）。
9. 弹幕文本提取有 4 套历史布局 + 递归兜底（`BilibiliDanmakuClient.cpp:793-821`），是维护热点。
10. 弹幕匿名连接被服务端**抽样**（实测 0-4 条/30s），不是 bug。

## 待办

- [ ] 把 "Oran7MediaClient 成员跑在内核线程" 改为 `PlaybackEngine : QObject`（queued signal 下发、`Q_PROPERTY` 上报）。
- [ ] 补齐或删除 `get_playable_duration` / `set_loop` / `get_loop`。
- [ ] 让 `mp_state_` 与内核标志成为**一个**带信号的状态机，并把状态发到 QML。
- [ ] 接入 `set_playback_rate`（UI 已有 Sonic 支持但无入口）。
- P2-1 已删除无调用的 ImageScaler 和解码器残留指针；计数器、重复宏等其他事项需独立核查。
- [ ] `prepare_async` 里的 `stream_open` 移到工作线程。

## 高帧率与 VFR 播放调度修复（2026-11-12）

样本 `Oran7-20261112-091211-PpIzJA.mp4` 标称 240fps，实际 2309 帧 / 20.962056 秒，平均约 110.15fps；视频平均码率约 3.58Mbps，音频长度 20.965 秒。只读检查了全部视频包，PTS 全部递增，间隔中位数约 8.645ms、最长约 23.488ms，不能仅凭 r_frame_rate=240 判断持续输出 240 帧。

`Frame Drop. in RunTime:511` 中的旧数字实际为 video_refresh 调用次数，不是 frame_drops_late；静态计数还会跨播放器实例共享。逐帧丢帧及空队列日志已移除，改为实例独立、每 3 秒汇总 `[PlaybackPerf] late_drop_interval / late_drop_total`。2026-11-12 改为由通用基础设置中的开发者模式实时控制，普通播放不输出，不再依赖编译宏或重启。关闭后重置区间采样基线，重新开启不将关闭期间的丢帧计入开启后的区间；累计数仍保留本次播放器实例的真实丢帧。汇总报告真实同步迟到丢帧，不代表采集丢帧或 QRhi 邮箱合并帧数。

旧刷新线程显示一帧后默认等待 10ms，下一次即使应在 4～9ms 内显示也不能及时运行，提前返回时又强制至少等待 1ms。现在按相邻真实 PTS 计算下一次截止时间，队列缺少下一帧时才参考解码帧时长；删除 1ms 强制下限，10ms 仅为空闲/长帧最大轮询间隔。新增线程独占 `Oran7VideoRefreshScheduler`，优先 Windows 高精度一次性等待计时器，不忙等、不请求全局 timer period；旧系统回退普通计时器，失败时保留 av_usleep。退出每次等待最长请求为 10ms。

同时纠正 framedrop=0 仍进入丢帧分支的条件；默认自动追帧仍然启用，不靠关闭音画同步丢帧来隐藏问题。max_frame_duration 在创建刷新线程前初始化。媒体帧仍走现有 QRhi 邮箱，不改变渲染线程与 HDR 路径。

新增 `video_pacing_checks`：临时目录生成 160×96、2 秒 240fps 合成视频，由真实 Native/FFmpeg 软解，回调消费帧，SDL 使用 dummy，不启动主程序、不打开硬件 GPU/音频设备。当前结果：纯视频 480/480、带音频 479/480；另有约 110fps 的非均匀 VFR 流 220/220。验证 PTS 单调、真实播放时长、亚毫秒截止时间，以及不被旧 10ms 轮询限制。native_io_checks 与 native_memory_checks 通过；Release 构建通过。

上述数字是软件隔离调度验证，不代表真实 1080p/4K 硬解、100Hz 显示器或 GUI 最终呈现全部 240 帧。真实样本的播放体验由用户重启后验收；采集端未达到 240fps 的问题仍独立跟进，未实施独立 CFR 补帧。
