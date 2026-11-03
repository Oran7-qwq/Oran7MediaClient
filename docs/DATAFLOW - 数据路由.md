---
status: ✅ 已核对
updated: 2026-10-27
---

# 数据路由（DATAFLOW）

> 本文回答：一次用户操作，数据从 QML 出发经过哪些层、在哪些状态上停留、什么时候回到界面。
> 每条链路按"入口 → 决策 → 执行 → 回传"排列，括号里是权威位置。

## 1. 播放本地音乐

```
QML 点击播放/双击行
└─ Client.musicQueue.activate(model,index) → Oran7PlaybackViewModel
   ├─ 记录模型/稳定 trackId 归属，通知 UI 音乐焦点
   ├─ Client.selectMusicTrack(...) → currentTrack 整体快照
   └─ Client.playNetworkSong(0, localPath, false)
      └─ Resolver 本地路径直传 → Client.startMusicPlayback（持久化）
         └─ Controller.prepare(path, musicPlayback)
            ├─ 同路径已有实例 → 播放/暂停；其他新目标 100ms 节流
            ├─ 更新 Controller 源路径和 PlaybackState 打开快照
            ├─ mediaAccepted → Client 加载歌词；视频则停止歌词
            ├─ 内部切歌 stopRequested → Controller.stopCurrent
            │  └─ 失效旧消息/帧，移动 Native unique_ptr 到后台释放
            └─ 300ms 单次定时器 → Controller.playOrPauseRequested → Controller.toggle（不经过 Client 信号）
               ├─ unique_ptr 创建 Oran7MediaPlayer
               ├─ create 注册静态消息循环（只捕获邮箱/固定代际）
               ├─ 渲染桥帧邮箱 + Qt D3D11 设备
               ├─ set_data_source(path) → prepare_async
               └─ Native 消息线程读取 PREPARED → 邮箱排入 GUI → start/pause
```

回传：

- 进度：Controller 100ms 定时器读取当前实例，过滤未知时长/非法位置/Seek 中采样；更新 PlaybackState 并转发旧 playProgressUpdated 信号。
- 时长：消息线程读取所属 Native 时长快照，经邮箱回 GUI 校验代际，再更新状态和 totalDurationUpdated。
- 结束：EOF 经邮箱回 GUI → Ended → stopIconUpdated/stopRequested → playNextTriggered。同步通知若触发新请求，旧后续动作失效。
- 错误：旧消息丢弃；本轮错误更新状态后发 openFailed，由 Client 委托 Resolver 执行网络补救。
- 视频帧：Native 回调只持有渲染桥共享帧邮箱；GUI 取最新帧提交渲染项。
- 删除文件：Client 匹配实际 Native 地址/待播放地址，主动停止后等待 Controller 全部退役实例释放，再调用曲库删除。

## 2. 播放网易云歌曲（VIP/非 VIP 分叉）

```
Client.musicQueue.activate(model,index) → Oran7PlaybackViewModel
├─ Client.selectMusicTrack(...) → currentTrack 整体快照
└─ Client.playNetworkSong(songId, fallbackUrl, isVip)
   ├─ 当前媒体相同 → 暂停/继续；撤销其他歌曲的待处理解析
   └─ Oran7NetworkSongResolver::resolve
      ├─ 新鲜缓存（20min）→ urlReady → Client.startMusicPlayback
      ├─ 非 VIP → 立即交付稳定外链；后台解析只更新缓存
      ├─ VIP → 等待真实地址；旧网络 URL 可提取 id，本地/无 id 记录直传
      └─ NcmSongApi::resolveUrls(ids, attemptToken)
         └─ CurlHttpClient（系统 curl.exe，Schannel，带登录 Cookie）
            └─ POST /api/song/enhance/player/url（ids + br=320000）
               ├─ urlsResolved(requestId=attemptToken, map)
               │  └─ Resolver 校验本轮尝试编号 → 缓存 → 按分支交付或只缓存
               └─ 解析失败 → 3 秒后重试一次 → 仍失败发 failed
                  → Client 设置错误、转发 networkPlayResolveFailed → QML 下一首
```

请求代际与 HTTP 尝试编号分开：播放和普通查询均通过 NcmRequest::nextId() 分配唯一编号，各 API 只消费自己的请求一次，旧/重复回调不会起播；主动停止、切本地/视频和退出使旧请求失效并停止重试。打开网络媒体失败时，Resolver 独立预算允许用缓存地址补救一次，否则沿用自动下一首通知。

歌词由 `Oran7LyricsService` 接收媒体路径和元数据快照，委托 `Oran7OnlineLyricsFetcher` 级联 LRCLIB/网易云；本地 LRC 持久保存，网络歌词在每轮独立 QTemporaryDir，旧结果按代际丢弃。

## 3. 播放 B 站直播

```
VideoPlayerStack：用户输入房间号
├─ BilibiliRoomAddressCatch.getRoomInfo(roomId)              （BilibliLiveRoomAddressCatch.cpp:5）
│  ├─ 新建 QNetworkAccessManager（每次调用一个，见债务）
│  ├─ GET xlive/web-room/v2/index/getRoomPlayInfo
│  │    protocol=0,1 format=0,1,2 codec=0,1 qn=10000 ptype=8
│  ├─ live_status!=1 → urlsError()
│  └─ getAvailableStreams() 打分（hevc+200/avc+100，fmp4+30/ts+20/flv+10）→ urlsReady()
├─ Client.requestPlayVideo(url[0])                            （:717）
└─ preparePlayingMedia → 同上；avformat 注入 B站 Cookie（ffmpegCookieString）

弹幕（并行）：
BilibiliDanmakuClient.start(roomId)                           （BilibiliDanmakuClient.cpp:238）
├─ room_init（短号 → 真实 room_id，必须）
├─ warmUp：GET www.bilibili.com 收 buvid3/4 到共享 cookie jar（防 -352）
├─ nav → wbi_img.img_url/sub_url → wbiMixinKey() → wbiSign()
├─ getDanmuInfo（-352 时降级 Danmu/getConf，再失败报错不重试）
├─ WebSocket /sub → sendAuth(uid=0 匿名 或 dedeUserId, protover=3)
├─ heartbeat 30s + watchdog 15s
└─ 收包：op=3 人气值 / op=5 通知（brotli 解压 → 递归解析 → DANMU_MSG）
   → danmakuReceived(text,uname,color,ts) → Oran7DanmakuLayer 多轨滚动
```

## 4. 录屏 / 推流

```
Oran7ScreenCaptureComponent.qml onCompleted
├─ Client.attachVideoItem(ScreenCaptureRender, host)          （Oran7MediaClient.cpp:1442）
│   ├─ createVideoItem → new D3D11VideoItem + 挂到 host + 绑定 D3D11DeviceProvider
│   └─ m_screenCap->setVideoItem(item)
├─ Client.screenCapture().start()                             （Oran7ScreenCapture.cpp:48）
│   └─ startWorkerIfPossible()                                （:612）
│       ├─ new QThread + DdaGrabWorker(monitor, drawMouse, fps)
│       ├─ worker.started → captureApplyFinished(true)（仅应用参数时）
│       └─ worker.initGraph()：av_hwdevice_ctx_create(D3D11VA)
│           ddagrab(output_idx,framerate,draw_mouse,output_fmt=bgra) → buffersink
└─ grabLoop：
    ├─ 共享纹理池（3×MISC_SHARED BGRA）← VideoProcessorBlt
    ├─ emit newFrameReady(idx)  ──Direct──▶ onNewFrameIndex → item->submitFrame
    └─ 若 recordingEnabled：
        ├─ downloadD3d11AndConvert（GPU→CPU BGRA→NV12，sws 缓存）
        ├─ pts = 墙钟 − 起始，强制单调
        └─ VideoEncoder.pushFrame → 编码线程 → _pktQueue
            ├─ Mp4Muxer 线程（RecordMode::MuxerToMp4，当前唯一可用）
            └─ WhipPublisher 线程（RecordMode::WhipPush，setRecordMode 无人调用 → 死路）

声音：WASAPI loopback → PCM 队列 → 音频桥线程 → AudioEncoder → 封装/推流
```

参数：`screen_capture.*` → `loadCaptureSettings()`（`main.cpp:365`）→ 应用时先落盘再重启 worker，失败 `rollbackCaptureSettings()`。

## 5. 网易云登录

```
Oran7NetEaseLoginPopup.start
└─ NcmWebLogin.start()                                        （WebLoginController.cpp:96）
   ├─ 找 Edge/Chrome → 随机调试端口 → 临时 profile
   ├─ 启动浏览器 → 轮询 http://127.0.0.1:<p>/json → 连 CDP WebSocket（必须 NoProxy）
   ├─ Network.enable + 每 1.5s Network.getAllCookies
   ├─ 抓 MUSIC_U / __csrf → CookieStore::setAuthCookie + save()（写 config.json）
   ├─ NcmUserApi.fetchAccount()（curl）→ nickname
   └─ loginSucceeded → QML 关闭弹窗、刷新收藏列表
```

B 站登录：`BilibiliAuthManager.startLogin()` → 二维码（外部 `api.qrserver.com` 生成图片）→ 轮询 86303 → 从 `data.url` 查询串 + `Set-Cookie` 双路抓 Cookie（需 URL 解码）→ nav 校验拿 `mid/uname`。

## 6. 主题切换/编辑

设置窗口 → saveComponentToken → profile v2 差量原子保存 → 默认源 + 当前覆盖 + 临时调整 → Oran7ThemeEvaluator 计算组件快照 → 比较键/值/类型 → 仅实际变化时发该组 Changed → 依赖该 Map 的 QML 绑定重新求值。

全局 token、深色模式和 reload 会计算 Index + 全部组件；所有结果先提交，再通知变化的分组。同值整套重载不广播。darkMode 是唯一权威，System 由 Qt 配色信号驱动。切换 profile 清理旧覆盖/临时值；旧全量 profile 首次先备份、迁移为差量。详见 [[Theme - 主题系统]]。

## 6.1 P1-6 录制请求与结果

QML → Client.screenCapture().startRecording() → Oran7RecordingSession（Preparing）→ 独立线程 Oran7RecordingBackend。任务读取配置值和共享 Feed，准备编码器/MP4 或 WHIP，输出就绪经带代际的回调进入 Recording；统计每约 500ms 回 GUI。stopRecording 置取消标记并进入 Stopping，后台关入口/音频/编码/输出，资源回收完才回 Idle/Error。预览被停止时关闭 Feed，旧任务不能绑定新采集实例。

## 7. 配置加载/保存

见 [[ARCHITECTURE - 架构]] §3/§4 与 [[Config - 配置与持久化]]。
P1-5：Manager 首次访问即加载并迁移为 schemaVersion=2；后续 loadConfig 返回既有状态，不覆盖待保存修改。模块通过 Oran7AppConfig 分组快照读取/提交，default.json 深合并提供唯一默认值。退出仍按原顺序采集窗口/播放状态，保存先备份已有原件，再用 QSaveFile 原子提交；损坏/未来版本保护见 [[Config - 配置与持久化]]。

## 8. 异步任务（QML 侧）

```
BasicConfig.fetchNeteaseLikedSongs()
└─ Client.cloudMusic.fetchLiked() → Oran7CloudMusicService → Oran7Async::run(Network)
   ├─ 服务在 GUI 线程快照 cookie/csrf，30 秒 TTL/并发/账号代际检查
   ├─ worker：blockingCurl × N（account → playlist → detail → 分批 song/detail）
   └─ finished（GUI）→ 成功/失败值，检查账号与取消代际
      ├─ success → Oran7TrackLibrary::replaceLikedTracks → C++ 模型整批替换（拉取中点赞置脏则不覆盖）
      └─ failed  → CloudMusicService 保持 likedDirty=true，下次重试
```

## 9. 全局输入（快捷键）

`Oran7GlobalEventFilter`（`Oran7GlobalEventFilter.h:13`）同时是 `QObject` 事件过滤器与 `QAbstractNativeEventFilter`：

- 普通键 → `eventFilter` → 信号（`up/down/left/rightKeyPressed` 等）。
- 已注册组合键 → `RegisterHotKey` → `WM_HOTKEY` 在原生消息泵里只记 `m_pendingHotkey`，由 `m_hotkeyDispatchTimer` 在事件循环安全阶段发出 `keyCombinationTriggered(combo)`，避免在原生消息处理中途触发 QML 重逻辑。

## 10. 链路速查（谁触发谁）

| 用户动作 | QML 入口 | C++ 门面 | 内核 |
|---|---|---|---|
| 播本地歌 | `LocalMusicStack.dealClicked` | `Client.playNetworkSong` | `Oran7MediaPlayer` |
| 播网络歌 | `SearchMusicStack.dealClicked` | `Client.playNetworkSong` | `NcmSongApi` + `Oran7MediaPlayer` |
| 播直播 | `VideoPlayerStack` | `Client.requestPlayVideo` | `BilibiliRoomAddressCatch` + `Oran7MediaPlayer` |
| 弹幕 | `VideoPlayerStack` | `BilibiliDanmakuClient`（直接 QML 类型） | WebSocket + brotli |
| 开始录制 | `ScreenCaptureStack` | `Client.screenCapture().startRecording()` | 采集→编码→封装/推流 |
| 改抓屏参数 | 设置窗 | `Client.applyScreenCaptureSettings` | 控制器 worker 重启 |
| 登录网易云 | 登录弹窗 | `NcmWebLogin.start` | CDP + curl |
| 登录 B 站 | `BilibiliLoginPopup` | `BilibiliAuthManager.startLogin` | QNAM + 轮询 |
| 改主题 | 设置窗 | `Oran7Theme.save*Token` | 主题引擎 |
| 搜索 | `SearchMusicStack` | `Oran7AsyncManager.run("musicSearch")` | 线程池 + curl |

## P1-3 列表路由更新（2026-10-24）

- 本地：后台文件任务 → GUI tracksReady(QVariantList) → TrackLibrary.local.replaceTracks → 模型通知 → QML 列表。
- 搜索/收藏：现有异步 handler → QML 请求代际/事件处理 → 集合批量命令 → C++ 模型；不再逐条 append。
- 整合：QML 操作意图 → TrackLibrary 判重/批量增删 → integrated 模型 → integratedMusicChanged → ConfigBinding 原键落盘。
- 拖拽：模型 moveTrack → begin/endMoveRows → 本地顺序直接同步曲库，整合顺序自动持久化；云端顺序提交仍由原网络流程处理。
- 成员标记：liked/integrated 身份集合变化 → 四个模型 dataChanged(isLiked/isIntegrated) → delegate 直接绑定。

## 漫游预缓存过滤（2026-10-27）

下拉设置「音乐播放器设置 → 网易云播放设置 → 漫游过滤已喜欢歌曲」绑定 `Client.musicQueue.roamFilterLiked`，由 ConfigBinding 保存到 `player.roam_filter_liked`，默认关闭以保留旧行为；与桌面歌词的 `floatingLocked` 主题配置无关。

`Oran7PlaybackViewModel` 独占 `Oran7RoamFilterStrategy`，在保留全部歌曲与排除已喜欢歌曲两种策略之间切换。排除策略调用 `Oran7TrackLibrary::isLiked(songId)`，复用喜欢模型已有的 `QSet<QString>` 身份索引，不逐曲遍历喜欢列表，也不维护第二份喜欢列表缓存。单曲平均 O(1)，过滤一批 M 首推荐平均 O(M)，不随喜欢列表 N 做 M×N 扫描。

- 新推荐发布前、消费缓存前、开启开关及喜欢模型变化时清理尚未消费的队列；当前曲目与上一首历史保留，操作不会立即切断当前歌曲。
- 开启过滤且喜欢列表尚未加载时复用 CloudMusicService 的异步加载及并发去重；成功后继续请求，失败不把未知喜欢状态当作空列表放行。
- 连续三批推荐都被过滤后停止补取；预取失败不打断当前歌曲，队列耗尽时停止。本轮预算在收到可用歌曲、切换策略或清空漫游会话后重置。
- 账号切换、取消、退出仍沿用原有会话代际；在途结果按收到时的策略过滤，旧请求结果不能复活已取消的队列。
- 过滤范围是已成功加载到喜欢模型的歌曲；云端无法获取详情、未进入模型的歌曲 ID 尚不单独建立索引。取消喜欢不会恢复此前已丢弃的推荐，后续推荐可再次接收。

项目当前推荐链路仍使用 `/weapi/playmode/intelligence/list`（心动/智能播放），本次不修改推荐协议。第三方适配项目另有 `/api/v1/radio/get` 的 `mode/subMode/limit` 控制，但不属于有稳定承诺的网易云官方开放 API；未据此假设服务端提供排除全部喜欢歌曲的开关。参考适配源码：[智能播放](https://github.com/neteasecloudmusicapienhanced/api-enhanced/blob/main/module/playmode_intelligence_list.js)、[私人 FM 模式](https://github.com/neteasecloudmusicapienhanced/api-enhanced/blob/main/module/personal_fm_mode.js)。

离线验证：`tests/playlist` 覆盖一万首喜欢歌曲、64 位 ID、缓存及喜欢变化、整批过滤补取上限、旧预取取消、喜欢预加载成功/失败；`tests/config` 覆盖配置持久化；`tests/localization` 使用实际音乐面板验证开关点击、反向同步及歌词主题隔离。均不访问真实账号或运行主程序。

