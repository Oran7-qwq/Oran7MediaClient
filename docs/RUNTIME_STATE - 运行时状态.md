---
status: ✅ 已核对
updated: 2026-10-27
---

# 运行时状态（RUNTIME_STATE）

> 本文回答：程序运行时的"事实"分别存在哪里、谁是权威、谁在镜像、什么时候落盘、QML 与 C++ 怎么同步。
> 这是本项目最需要先统一的部分——目前同一个事实经常有 2~4 个副本，改一处漏一处就会出"界面不刷新/重启后回退"的 bug。

## 1. 三层状态模型

| 层 | 权威方 | 生命周期 | 同步方式 | 现存问题 |
|---|---|---|---|---|
| **领域状态** Domain | C++ | 进程 | `Q_PROPERTY` + `NOTIFY`，或 `Q_INVOKABLE` 拉取 | Client.playback 已提供主要播放快照；音乐元信息已纳入 currentTrack；列表与队列策略仍待迁移 |
| **界面瞬时状态** UI | QML | 页面/组件 | QML 绑定 | 业务决策（播放哪首、缓存多久、焦点归谁）被写进 QML |
| **持久化状态** Persist | 文件 | 跨进程 | `Oran7AppConfigManager` / 主题 profile | 字符串 key 散落各处，无版本号、无迁移、无加密 |

判定规则：

- **能被界面直接显示、且必须跨页面一致** → 领域状态，放 C++，只读暴露。
- **只影响本控件外观/动画/临时焦点** → UI 状态，留 QML。
- **重启后还要恢复** → 持久化状态，且只允许在明确的保存点写盘（启动加载、退出保存、用户显式操作）。

## 2. 事实所有权总表

| 事实 | 权威存储 | 镜像/副本 | 落盘 | 备注 |
|---|---|---|---|---|
| 本地录制容器格式 | 当前 `Oran7RecordingConfig.format`，会话启动取值快照 | 工作台 outputDraft 编辑草稿；保存后刷新 outputSaved | `recording.format`，默认 mp4，可选 flv | 格式独立于本地/WHIP mode；沿现有配置原子提交，不改变运行中的会话 |
| 开发者模式 | Oran7Core 中的原子开关，经 `Oran7GlobalHelper::developerMode()` 跨线程读取 | `Oran7UiPreferences.developerMode` 只读投影；设置开关不缓存业务状态 | `general.developerMode`，默认 false | 仅 preferences 在启动恢复及保存成功后发布；保存失败不改运行状态；不再使用编译宏 |
| 启动阶段/就绪门槛/错误与取消 | main 独占的 `Oran7StartupCoordinator` | Oran7Startup 上下文只读投影；QML 提交取消 | 不落盘；交接成功前不保存部分几何 |
| 主窗预热透明度/渐变与原生呈现门槛 | `Oran7StartupCoordinator` | 保存原 opacity；show 前归零，实际帧后执行 320ms 交叉渐变并再等一帧 | 否 | 运行期不再接管 opacity；失败/取消/就绪撤销停止渐变；仅外部启动窗口显示加载画面 |
| 启动/主窗身份 | 同一 QQmlApplicationEngine；load 持有启动窗、QObject parent 持有异步孵化主窗 | 协调器 QPointer；配置按 __Oran7Window__ 查 rootObjects/引擎直属子对象 | 原生 HWND/标题栏先在隐藏时初始化，再应用客户区几何，首帧后交接 |
| 主窗孵化进度 | Coordinator 的 QQmlIncubator | 启动窗 incubationController 在 GUI 分帧创建；轮询只观察状态 | 否 | 取消/失败清理未完成创建，Ready 对象交给引擎 parent |
| 背景首帧代际/就绪 | `Oran7BackgroundFrameMonitor` | QML ready 只读依赖真实 QVideoSink 有效帧；切源重置代际 | 否 | 空帧不撤销已取得的首帧；旧排队回调无效 |
| 主窗背景路由/超时回退 | `Oran7MainWindowBackground` | main.startupPageReady 汇总；不保存运行期回退 | 否 | 视频失败/5 秒超时锁定图片/底色，切源或切模式才重新尝试 |
| 主窗原生背景样式应用结果 | main.qml 的 backgroundNativeReady/backgroundDwmApplied | 背景组件显式参数 | 否 | 意图来自主题配置，DWM 不再无条件启用 |
| 凭据已提交引用与加密基线 | Oran7AppConfigManager（Oran7Core） | Oran7AppConfig 为运行期解密值快照；业务不另行持久化 | config.json 的分组 UUID + 独立 DPAPI 文件 | 先验证新密文，再提交普通配置；错误组保留原引用，退出登录提交空代际 |
| 日志队列、统计与文件分段状态 | Oran7Core 内唯一 `Oran7LogSink` | statistics() 为锁保护值快照；QML 无副本 | AppLocalDataLocation/Logs | 生产者脱敏入队，专用线程独占文件；退出排空，满队列丢新，错误降级 |
| 播放器打开目标 | `Oran7PlaybackController::m_source` | `Oran7PlaybackState::source` 为 QML 播放状态快照；恢复聚焦可不启动 | last_used_file（音乐） | 已删除 appData.CurMediaFilePath；实际 Native 地址通过 playingSource 返回副本 |
| 当前音乐信息（入口路径/名/歌手/专辑/封面/标注时长/songId/VIP） | `Oran7PlaybackState::currentTrack`（Oran7Track 值） | BasicConfig.currentMedia* 只读绑定；currentSongId/currentIsVip 读取同一值 | `last_media_*` / last_used_file | 入口一次提交；恢复先更新快照；停止/视频期间保留音乐选择 |
| 是否正在播放 | `Oran7PlaybackState::state` | BasicConfig.isPlaying 只读绑定 active；playing 表示内核已开始播放，active 额外包含 Opening | 不落盘 | QML 不再写该属性 |
| 播放位置/总时长 | `Oran7PlaybackState::positionMs/durationMs` | 旧进度/歌词信号仍作兼容输出 | 不落盘 | GUI 更新，旧播放器时长消息按代际丢弃 |
| 播放模式 | `Oran7PlaybackState::mode`（0/1/2 顺序/单曲/随机；3 心动、4 私人漫游、5 熟悉、6 探索） | BasicConfig.playMode 只读绑定 | 不落盘 | 普通顺序经 Client.setPlayMode；推荐经 musicQueue.selectRecommendation；队列由 Oran7PlaybackViewModel 持有 |
| 音量 | `Oran7PlaybackState::volume` | BasicConfig.playerVolume 只读绑定；保存配置直接读 PlaybackState，无额外镜像 | ✅ | Client.setVolume 统一校验 0～100，启动恢复立即同步，不依赖弹窗实例化 |
| 当前播放器实例 | `Oran7PlaybackController::m_player`（unique_ptr，GUI 独占） | 内核 mp_state_、isPlaying 为原子；消息携带发生时状态 | 不落盘 | 退役时移动到后台闭包，GUI 不再访问；消息只带值经邮箱回传 |
| 退役播放器与文件操作等待 | Controller 退役计数/whenPlayersReleased | 后台闭包独占 Native；等待回调只在 GUI 调用 | 否 | 全部退役实例释放后才删除文件；shutdown 清空等待回调 |
| 播放队列归属（哪张列表在播） | `Oran7PlaybackViewModel` | Client.musicQueue.owner；BasicConfig 只读投影 | 不落盘 | C++ 借用模型/稳定 trackId，切歌时重定位 |
| 本地曲库文件快照/顺序 | `Oran7LocalLibraryService::m_entries/m_order`（GUI 线程） | TrackLibrary.local，批量 tracksReady 快照 | `local_music_sort.*` | 后台只读值快照；最新结果完整提交，顺序与 UI 发布一致 |
| 视频／直播观看记录 | `Oran7VideoHistory`（GUI 线程模型） | `Client.videoHistory`，本地与直播页签按来源展示 | `video.history` | 成功起播后记录，按身份去重并置顶，每类最多 50 条；直播只存房间号 |
| 本地视频观看进度与恢复意图 | `Oran7VideoHistory` 观察 PlaybackState 的来源与状态 | positionMs、durationMs、completed roles | `video.history` 各本地条目 | 有效状态采集，5 秒节流；每次打开只恢复一次，用户 seek 撤销恢复 |
| 直播房间展示资料／刷新状态 | `Oran7VideoHistory` 与内部 Oran7LiveRoomInfoService | title、anchorName、avatarUrl、thumbnail、liveStatus、roomInfoState roles | `video.history` 各直播条目 | 名称／图片地址持久化，实时状态重新查询；同房间去重、最多两路并发 |
| "我整合的音乐" | `Oran7TrackLibrary::integrated()` | BasicConfig 只读借用；`config.json:integratedMusic.list` | ✅ | C++ 去重/批量/增删/排序自动持久化 |
| 网易云收藏列表 | `Oran7TrackLibrary::liked()` | Oran7CloudMusicService 持有 loaded/fetching/dirty 与 30 秒 TTL | 不落盘 | 账号代际、并发去重、点赞共用 2 秒冷却 |
| 网易云登录态 | `CookieStore`（`MUSIC_U` 非空） | `NcmSession.isLoggedIn/userName/csrfToken/cookieString` | `netease_auth.*` | `NcmSession::setLoggedIn()` 是空实现，只发信号 |
| B 站登录态 | `Oran7BilibiliAuthManager::m_isLoggedIn` + 4 个 Cookie 字段 | QML `isLoggedIn/userName` | `bilibili_auth.*` | nav 校验失败只清内存不清配置 |
| 主题 token | `Oran7Theme` 内 9 个 `QVariantMap` | 主题 profile JSON | `profile-*.json` | 详见 §4.4，profile 会覆盖 JSON 源 |
| 深色模式 | `Oran7Theme.darkMode` / `isDark` | 设置与旧组件键只读派生 | profile v2 的 darkMode | P2-5 单一权威，支持系统跟随 |
| 设置窗口开关 | `Oran7MainUiSetting.openSettingWin/settingContentVisible` | 主题 JSON 里的 `OpenSettingWin`/`settingContent_visiable`（已死） | 部分 | 同一事实跨了两个存储 |
| 窗口几何 | `Oran7WindowGeometryState.normalGeometry`（普通窗口 rect） | 主窗 `normalWindowGeometry` 只读投影 | `window.*` | 隐藏恢复后初始化；普通状态防抖采集，关闭/保存边界补采集；不再通过设置单例读取 |
| 抓屏参数 | `Oran7ScreenCaptureController::m_capSettings` | `Oran7MediaClient` 透传接口 | `screen_capture.*` | 首帧确认才提交；待应用目标/最近可用值分离（§4.3） |
| 采集对象生命周期/绑定 | Client unique_ptr 桥 → unique_ptr 采集控制器 | 原 QObject 句柄，显式 CppOwnership；控制器观察渲染桥持有的项 | 否 | 桥先创建控制器再接线，关闭丢弃排队结果；析构等待采集退出后才释放渲染项 |
| 歌词加载与缓存生命周期 | `Oran7LyricsService` | Client 转发四个信号，3 个 Oran7LyricsModel 各自读取文件/更新高亮 | 本地 .lrc 持久化，网络独立临时目录不持久化 | 停止/换歌/析构清理网络目录，保留本地歌词 |
| 网络任务结果 | 无（一次性） | QML 局部变量/模型 | 不落盘 | `Oran7AsyncManager` 用 `QVariantMap{success,result,error}` |

新增事实所有权（2026-10-24）：

| 事实 | 权威存储 | 消费方 | 落盘 |
|---|---|---|---|
| 播放器错误 | `Oran7PlaybackState::errorString` | QML 只读；新打开目标清除旧错误 | 否 |
| 渲染槽位/宿主/源尺寸/缩放 | `Oran7RenderBridge::m_slots`（GUI 线程独占） | Client 兼容门面、D3D11VideoItem | 否 |
| 待投递播放视频帧与帧代际 | `Oran7RenderBridge` 共享邮箱（互斥锁保护，最多一帧） | 播放线程仅写邮箱；GUI 取帧并提交；停止/切换/销毁使旧回调失效 | 否 |
| Qt D3D11 设备引用 | `Oran7RenderBridge::m_device`（ComPtr） | Client 通知当前播放器设置设备；内核自行 AddRef | 否 |
| 音乐快照版本 | `Oran7PlaybackState::m_trackRevision` | 启动详情补全先校验版本，只补空字段；同 songId 再选择也不能接受旧版本 | 否 |
| 歌词请求代际 | `Oran7LyricsService::m_generation` | 每轮新获取器，排队成功/失败回服务时验证；停止先失效再 abort | 否 |
| 曲库扫描代际/文件任务队列 | `Oran7LocalLibraryService::m_generation/m_queue` | 单个串行执行器；旧扫描不发布，后台持独立值与取消标志 | 否 |
| 后台任务取消状态 | `Oran7TaskHandle` 的共享原子标志 | QML 门面按 taskId 持有句柄，执行器读取取消标志 | 否 |
| 当前播放器消息代际 | `Oran7PlaybackController::m_generation` | 消息线程仅携带固定代际/值快照，GUI 落地时校验；析构失效共享邮箱 | 否 |
| 播放命令版本/延迟切歌 | Controller `m_commandRevision`、m_pendingSwitch、成员 QTimer | 连续请求合并为最新目标；停止取消定时器，同步通知重入校验命令版本 | 否 |
| 网络歌曲请求与缓存 | `Oran7NetworkSongResolver::m_generation`、目标 `m_requestedSongId`、20 分钟缓存 | GUI 线程独占，Client 仅消费当前代际结果 | 否 |
| 地址解析 HTTP 尝试 | Resolver `m_pendingToken` / `m_pendingSongId` | 每次尝试独立编号，NcmSongApi 原样回传；普通查询为 0，旧/重复结果丢弃 | 否 |
| 网络歌曲重试预算 | Resolver `m_resolveRetried` / `m_openRetried` | 解析失败 3 秒后一次；打开失败使用缓存补救一次；新请求/取消复位，重试定时器随服务析构 | 否 |
| 音乐页是否需要底栏 | `RightPage.musicPageActive`（当前栈页派生） | `main.qml` 绑定高度与透明度 | 否 |
| 预览状态/启动意图/最新参数请求 | `Oran7ScreenCaptureController`，GUI | previewState/busy/error 只读，编号终态；QML 仅留草稿 | 否 |
| 采集取消/日志意图 | `shared_ptr<Oran7CaptureControl>` | GUI/worker 只访问原子值，控制器不持 worker QObject | 否 |
| 采集图/设备、诊断及录制节流 | `Oran7DesktopCaptureWorker` | 图/设备以 unique_ptr 管理；信号按代际消费，控制器只共享控制值/Feed | 否 |
| 预览池/VideoProcessor、录制转换/读回、swscale | worker 的 `unique_ptr<Oran7CaptureFrameConverter>` | 采集线程独占，资源 RAII；在图/设备前释放，帧租约独立延长槽位寿命 | 否 |

## 3. QML ↔ C++ 同步现状

### 3.1 用的机制

| 机制 | 用例 | 评价 |
|---|---|---|
| `qmlRegisterSingletonType` + 上下文 lambda | 全部业务单例（`main.cpp:192-253`） | 生命周期由 C++ 控制，合适 |
| `Q_INVOKABLE` 命令 | `Client.playMusic/seekTo/setVolume/playNetworkSong` | QML → C++ 的唯一通道，合适 |
| 信号广播 + QML `Connections` | `playProgressUpdated`、`lyricsAvailable` | 无状态快照，新页面挂载时拿不到当前值（靠 QML 自己保存） |
| `Q_PROPERTY` + `NOTIFY` | `Client.playback`、NcmSession、D3D11VideoItem | 播放快照/currentTrack 已接入；四类列表改为 C++ QAbstractListModel |
| 上下文属性 | `globalEventFilter` | 单例过少时的补丁 |
| 窗口几何快照 | C++ 退出时定位具名主窗，读取显式 `normalWindowGeometry`；必要时调用 `captureNormalGeometry` | 已移除设置单例依赖；GUI 状态仍由 QML 持有 |

### 3.2 目标规则（重构方向）

1. **命令下行、状态上行**：QML 只调 `Q_INVOKABLE` 表达意图；C++ 只通过 `Q_PROPERTY NOTIFY` 暴露事实。
2. **状态对象化**：已引入 `Oran7PlaybackState`（状态、位置、时长、音量和模式均为只读属性；音乐元信息为只读 currentTrack），逐步替代"信号流 + QML 自己拼"。
3. **列表模型化**：曲库、收藏、搜索、整合音乐统一为 `QAbstractListModel`，删除 `xxxElementReady` 逐条追加协议。
4. **避免通过通用 QML 单例取业务数据**：窗口几何目前由专用 QML 组件拥有，C++ 只读主窗显式快照，保存边界请求同步采集；不回退到设置样式单例。

## 4. 各状态机

### 4.1 播放状态机（P1 首批）

Oran7PlaybackState 由 Oran7MediaClient 持有，全部字段只在 GUI 线程修改：

```
Idle/Ended/Error → Opening → Playing ↔ Paused
                              ↕         ↕
                            Seeking   Seeking
Playing → Ended；打开/解析失败 → Error；显式停止 → Idle
```

Seeking 记住进入前的播放/暂停语义，完成后恢复。playing 表示已播放，active 还包含 Opening，用于兼容旧播放按钮；BasicConfig.isPlaying 是 active 的只读投影。网络地址解析且尚无播放器时也进入 Opening。

内核 MP_STATE_* 仍用于执行命令，mp_state_/isPlaying 为原子。FFP_MSG_PLAYBACK_STATE_CHANGED 携带状态值，Controller 静态消息循环通过共享邮箱将状态、时长、结束与错误快照投递到 GUI，落地前比较 m_generation，旧播放器消息不会覆盖新实例。状态消息不再只打日志。

m_switchGeneration 使停止前排队的切歌回调失效；新的快速切歌更新目标路径，复用一次延迟启动。歌曲标题/封面/身份等由 currentTrack 整体更新，BasicConfig 只读；P1-7 列表/漫游策略由 Oran7PlaybackViewModel 统一。

### 4.2 网易云登录状态机

旧 API 扫码控制器及注册已随 P1-4 删除；浏览器登录仍是现行路径。
`WebLoginController`（实际在用）：`Idle → StartingBrowser → WaitingLogin → Captured → Success/Error`，通过 Chromium CDP `Network.getAllCookies` 抓 `MUSIC_U`（`WebLoginController.cpp:96-380`）。
Cookie 的权威在 `CookieStore`；`NcmSession.isLoggedIn` 是派生投影。Session 的 m_verifyRequestId 过滤旧校验，浏览器的 m_accountRequestId 仅接收 Captured 阶段的本轮账号结果。

Song/User/Playlist 按 requestId 保存待处理请求，回调消费一次；Playlist 保存歌曲/动作/歌单/凭据快照，User 保存凭据快照，账号变化拒绝旧回复。DTO 使用毫秒时长，QML 兼容 timesize 仍为秒字符串。

### 4.3 预览与参数应用状态机（P3-2）

```text
applyCaptureSettings(新参数) → 校验录制忙/FPS/显示器 → requestId + 最新待应用目标
  ├─ 未绑定界面/纯日志热更新 → 保存并同步返回终态
  └─ 需要重启 → Stopping：取消旧控制值、close Feed、quit（不阻塞）
      → finished + 零超时完成探测 → 释放旧线程/控制/Feed → Starting
        ├─ 当前代际首帧 → 保存 capture 分组 → Running + 编号成功结果
        ├─ 初始化失败/首帧超时 → 编号失败 + 异步恢复最近可用目标
        └─ 恢复失败 → Error，可手动重连
```

Oran7ScreenCaptureController 唯一拥有预览状态；previewBusy 由 Starting/Stopping/Recovering 派生。m_previewRequested 仅是启动意图，m_pendingCapture 是一个最新请求，m_activeTarget 是当前会话快照，m_lastUsableTarget 只在首帧确认后建立；已保存参数不提前覆盖。被替代请求有 superseded 终态，旧 worker 回调按代际丢弃；finished 按线程身份回收。

控制器不借用后台 worker QObject，只共享 Oran7CaptureControl 原子取消/日志值及 Feed。QThread::finished 后若延迟删除/TLS 未完成，wait(0) 立即返回，10ms 定时器再检查；不占用 GUI 等待。普通 stop 只提交，stopped 才表示实际完成；唯一阻塞 join 为 shutdown。场景图设备/显示模式变化保留最多 8 次恢复，预算只由真实首帧重置。

QML 直接绑定 previewBusy/previewError，并按 requestId 完成设置等待；迟到无编号接口结果不能覆盖新请求或新草稿。同步 monitors 枚举接口仍在，不把本项描述为所有设备调用都已异步。

### 4.3.1 P1-6 录制会话状态机

Oran7RecordingSession 独占 Idle → Preparing → Recording → Stopping → Idle，失败在资源回收后进入 Error。Preparing/Recording/Stopping 均 busy，拒绝重复开始及配置修改；开始/停止只提交请求。独立线程完成首帧/编码器/WHIP 等待、flush 和收尾，回调按请求代际回 GUI。Oran7RecordingFeed 的 epoch 让每次录制重新建立视频时间戳；设备引用和编码入口由 mutex 保护。预览/参数重启由 P3-2 独立状态机管理。

recordingStatistics 包括推流连接状态、ICE、实际码率与发送帧数，QML 不维护独立录制状态。参数集中在配置 recording 段，WHIP 默认地址/凭据为空，不启动陈旧 TCP/Server。

### 4.4 主题状态机

P2-5：源 JSON + 当前 profile v2 差量 → 唯一 darkMode（手动或系统）→ 类型化表达式计算 → 完整快照提交 → 仅变化组发通知。切换清除旧覆盖，重载恢复保存值；install* 临时调整不跨重载。

旧全量 profile 在读取时先保留备份、原子迁移；新增源 token 不再被旧 profile 全量覆盖遮蔽。组件 Map 是通知粒度，尚未实现逐键订阅。详见 [[Theme - 主题系统]]。

### 4.5 页面/焦点状态机

- 页面：`MainStackView` + `RightPage.pageManager`，`navigateTo()` 先找已存在的 `pageName` 再 `pop`，否则 `push(Immediate)`；`popPage()` 定义了但没人调用 → 没有真正的返回栈。
- 焦点：`BasicConfig.lastFocusedItem/currentFocusedItem` + `Oran7MainUiSetting.activeOtherItemCount` 引用计数，手写注册表；P0 已将 RightPage 的标题栏状态改为显式输入、底栏显示改为当前页面的派生输出，歌词背景使用附属窗口尺寸，不依赖外部 id。

## 5. 冗余与冲突清单（改状态前必读）

| 冲突 | 位置 | 风险 |
|---|---|---|
| 已解决：QML 独立播放态 | `Client.playback.state` | BasicConfig.isPlaying 只读绑定 |
| 已解决：QML 独立音乐元信息 | `Client.playback.currentTrack` | BasicConfig.currentMedia* 只读绑定，恢复不依赖底栏信号接收 |
| 已集中：播放归属/索引 | `Oran7PlaybackViewModel` | 模型与稳定 trackId，自动推进时重新定位 |
| 深色模式三份 | token / 设置单例 / `Oran7Theme` | 设置里切换只改组件色，Index 调色板仍为亮色 |
| 主题状态混入设置状态 | `Oran7MainGUI.json` 的 `OpenSettingWin`/`settingContent_visiable` | 主题文件承担了 UI 开关，profile 里永久残留死字段 |
| 已集中：网易云收藏策略 | `Oran7TrackLibrary` / `Oran7CloudMusicService` | 模型数据与缓存策略分离，写失败触发强制对账 |
| 歌词模型三份 | `Oran7LyricsIsland` / `Oran7LyricsView` / 浮动窗复用 | 每帧三次解析/查找 |
| 抓屏目标与诊断控制 | 控制器存已提交/待应用/在途值；worker 与 GUI 共享取消/日志原子控制 | 启停状态只由控制器发布，转换器无状态副本 |

## 6. 持久化格式

`<exe>/../Config/config.json`（默认值来自嵌入资源 `:/config/default.json`）：

| 段 | 键 | 写入方 | 备注 |
|---|---|---|---|
| `window` | `width/height/maximized/position.{x,y}` | `Oran7MediaClient::saveConfig_AppWindow*` 读 QML 单例 | — |
| `appearance` | `themeProfile` | `Oran7Theme::setCurrentThemeProfile` | — |
| `last_used_file` | 路径字符串 | `Oran7AppConfigManager::setLastUsedFile` | — |
| `local_music_sort` | `last_modified/play_order/sort_type` | `Oran7MediaClient::saveConfig_localMusicList_playOrder` | — |
| `playerVolume` | 0~100 | `saveConfig_AppSetPlayerVolume` | — |
| `recent_files` / `recent_files_max_count` | 数组/整数 | `Oran7AppConfigManager` | 当前 UI 未使用 |
| `user` | `auto_login/remember_password/username` | 遗留 | 遗留登录，未使用 |
| `bilibili_auth`（内存） | `sessdata/dede_user_id/bili_jct/user_name` | `Oran7BilibiliAuthManager` | P2-3 DPAPI 分离保存，普通配置仅保存引用 |
| `netease_auth`（内存） | `MUSIC_U/__csrf/nickname` | `CookieStore` | P2-3 Oran7DpapiCookiePersistence，普通配置仅保存引用 |
| `screen_capture` | `output_index/fps/draw_mouse/debug_log` | 抓屏控制器 | fps 越界回退 60，索引会按当前枚举钳制 |
| `integratedMusic.list` | 数组 | `Oran7MediaClient::saveIntegratedMusic` | P1-5 默认提供空数组，列表行格式保留 |

主题 profile：`profile-<name>.json`，P2-5 起为 profileVersion=2 差量；旧全量文件先备份迁移。

## 7. 改进优先级

1. 播放状态对象化（消灭 `isPlaying` 双份与"信号拼状态"）。
2. 曲库/收藏/搜索统一 `QAbstractListModel`。
3. P1-5 已实现带 schemaVersion=2 的配置值访问层、迁移、集中键与深合并默认值。
4. 主题：单一实例 + 单一深色来源 + token 保留类型。
5. P3-2 已实现首帧成功后落盘；失败仍需恢复旧采集会话，但不再把失败候选参数写入磁盘再回滚。
6. Cookie 走 DPAPI 加密，日志禁止输出明文/半截响应体。

详见 [[REFACTOR - 重构路线]]。

## P1-2 胶水层收尾后的所有权

- ConfigBinding 持有目录值及最近音乐的持久化入口；窗口/音量/曲库顺序/整合列表均映射原配置键，音量不再复制。
- TrackMetadataService 持有 NAM/reply 取消与代际，返回 songId/trackRevision/专辑/时长；Client 校验快照后协调提交，不把 HTTP 和配置合成一个服务。
- LegacyServerClient 隔离旧 Socket/UID/缓冲/SQLite，默认未启用，无启动网络/数据库副作用；原服务端不启动，官方服务器计划延后。
- Client 不再保存 Oran7AppData、m_metaNam、client_mutex 或 TCP/SQL 字段；只保存服务所有权、状态/渲染对象引用和信号连接。

## P1-3 列表所有权更新（2026-10-24）

四个列表实例由 Client → unique_ptr<Oran7TrackLibrary> → unique_ptr<Oran7TrackListModel> 持有，QML 只读借用。服务的文件快照是后台作业/排序依据；模型是 GUI 列表投影，扫描结果按代际整批替换。本地拖拽直接同步服务顺序；喜欢/整合成员关系只有 C++ 索引一份，角色通知同步其他模型。P1-7 后网络收藏拉取重入/TTL/冷却由 Oran7CloudMusicService 统一，播放归属/队列/漫游由 Oran7PlaybackViewModel 统一；搜索关键词/请求代际保留在搜索页。
