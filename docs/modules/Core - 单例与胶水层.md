---
status: ✅ 已核对
updated: 2026-10-24
---

# Core：单例与胶水层

> 源码 `src/cpp/GlobalSingleton/` 与 `src/cpp/Oran7MediaClient/`　产物：`Oran7Core.dll`（4 个）+ EXE（其余）
> 这是全项目的"中央车站"：QML 只认识这一层的对象，内核能力都从这里被拉起来。也是耦合最重的地方。

## 职责与边界

- 提供 **QML 唯一入口** `Oran7MediaClient`，以及认证、搜索、网络监测、异步、主题 profile 等全局服务。
- 持有进程级生命周期：`Oran7ApplicationContext` 负责创建/销毁 `Oran7MediaClient`，`main.cpp` 负责按序 `cleanup()`。
- **不承担**：具体协议实现（在 `src/ncm`）、解码渲染（在 `Oran7MediaPlayer`/`Oran7VideoGPURender`）、主题解析（在 `QtQuicCppExtend`）。

## 对象清单

`Oran7MediaClient/` 集中放置 Client、PlaybackState、RenderBridge、LyricsService、LocalLibraryService、MediaInfoReader、NetworkSongResolver、PlaybackController、ScreenCaptureBridge、ConfigBinding、TrackMetadataService、LegacyServerClient（各一对 `.h/.cpp`），新增 `Oran7TrackListModel` / `Oran7TrackLibrary`（各一对 `.h/.cpp`），以及独立值类型 `Oran7Track.h`。Client 仍由 ApplicationContext 持有，内部服务统一由 unique_ptr 独占，不叠加 QObject parent；MediaInfoReader 无状态。不新增库、单例或 QML 注册。其余下表文件仍位于 `GlobalSingleton/`，包括暂未迁移的歌词模型、解析器和在线获取器。

| 类 | 文件 | 职责 | 暴露给 QML |
|---|---|---|---|
| `Oran7ApplicationContext` | `Oran7ApplicationContext.h/.cpp` | 静态单例，创建并持有 `Oran7MediaClient`；抓屏控制器由 Oran7MediaClient 自行创建 | 否 |
| `Oran7MediaClient` | `Oran7MediaClient.h/.cpp` | QML 门面 + 播放控制 + 曲库门面 + 配置读写 + 歌词门面 + 录屏桥 + 遗留 TCP | `Client` 单例 |
| `Oran7RenderBridge` | `Oran7RenderBridge.h/.cpp` | GUI 渲染槽位/布局、设备引用、带代际的最新帧邮箱；由 Client 持有 | 通过 Client 兼容门面 |
| `Oran7AppConfigManager` | `Oran7AppConfigManager.h/.cpp` + Oran7Config | 唯一内存配置、类型化快照、迁移/深合并、备份与原子保存 | 否 |
| `Oran7ThemeProfileManager` | `Oran7ThemeProfileManager.h/.cpp` | 主题 profile 文件 CRUD | 否 |
| `Oran7GlobalHelper` + `Oran7Logger` | `Oran7GlobalHelper.h/.cpp` | `getConfigDir()`、日志宏、`Oran7VideoInfo` | 否 |
| `Oran7Async` | `Oran7Async.h/.cpp` | 跨模块共享线程池 + 退出闸门 | 否（C++ 用） |
| `Oran7AsyncManager` | `Oran7AsyncManager.h/.cpp` | QML 侧异步门面，注册 handler | `Oran7Async` 单例 |
| `Oran7BilibiliAuthManager` | `Oran7BilibiliAuthManager.h/.cpp` | B 站 Cookie 生命周期、扫码登录、`dedeUserId()` | `BilibiliAuth` 单例 |
| `Oran7NetworkMonitor` | `Oran7NetworkMonitor.h/.cpp` | 3s 延迟探测、在线态、触发 `verifyLogin` | `Oran7Network` 单例 |
| `Oran7LyricsModel` + `Oran7LrcParser` | `Oran7LyricsModel.h/.cpp`、`Oran7LrcParser.h/.cpp` | LRC 解析 + 高亮行模型 | `Oran7LyricsModel` 类型 |
| `Oran7TrackLibrary` | `Oran7TrackLibrary.h/.cpp` | 四模型的独占所有权、身份索引、批量更新及成员标记同步 | Client.tracks；普通实例 |
| `Oran7TrackListModel` | `Oran7TrackListModel.h/.cpp` | 只读元数据角色、快照、选择/动画/移动命令 | tracks.local/integrated/liked/search |
| `Oran7LocalLibraryService` | `Oran7LocalLibraryService.h/.cpp` | 串行文件任务、最新曲库快照、顺序、路径范围；Client unique_ptr 独占 | 通过原曲库信号/命令 |
| `Oran7MediaInfoReader` | `Oran7MediaInfoReader.h/.cpp` | 无状态 ffprobe/标签/时长/封面读取，GUI 恢复与后台扫描共用 | 否 |
| `Oran7NetworkSongResolver` | `Oran7NetworkSongResolver.h/.cpp` | 地址解析、缓存、关联隔离、两套重试预算；Client unique_ptr 独占 | 经 Client 原命令/信号 |
| `Oran7ConfigBinding` | `Oran7ConfigBinding.h/.cpp` | 原配置键/窗口/音量/曲目恢复/列表/目录值映射，普通 C++ 类 | Client 原接口转发 |
| `Oran7TrackMetadataService` | `Oran7TrackMetadataService.h/.cpp` | 网络详情/超时/取消，带 trackRevision 的结果值 | Client 协调提交 |
| `Oran7LegacyServerClient` | `Oran7LegacyServerClient.h/.cpp` | 陈旧、默认停用的 TCP/SQLite 会话 | 保留原登录信号接口 |
| `Oran7ScreenCaptureBridge` | `Oran7ScreenCaptureBridge.h/.cpp` | 控制器 unique_ptr、参数/忙状态转发、采集项绑定、异步结果和关闭 | Client.screenCapture() 保留原控制器 |
| `Oran7PlaybackController` | `Oran7PlaybackController.h/.cpp` | Native unique_ptr、播放/暂停/Seek/音量、消息邮箱、切歌/进度定时器、后台退役；Client unique_ptr 独占 | 经 Client 原命令/信号 |
| `Oran7LyricsService` | `Oran7LyricsService.h/.cpp` | LRC 匹配/歌词请求代际/网络临时缓存，Client 以 unique_ptr 独占 | 通过 Client 四个原信号 |
| `Oran7OnlineLyricsFetcher` | `Oran7OnlineLyricsFetcher.h`（仅头文件实现） | LRCLIB → 网易云 歌词级联 | 否 |
| `Oran7GlobalEventFilter` | `Oran7GlobalEventFilter.h/.cpp` | 全局键 + `RegisterHotKey` | 上下文属性 `globalEventFilter` |
| `Oran7SoundEffect` | `Oran7SoundEffect.h`（仅头文件） | WAV 音效（`SOUND_EFFECT` 宏生成 invokable） | `Oran7Sound` 类型 |
| `Oran7ConsoleLogger` | `Oran7ConsoleLogger.h` | 附加/分配控制台 | 否 |

`Oran7Core.dll` 含 5 个服务/辅助类及 Oran7Config 值类型：`Oran7GlobalHelper` / `Oran7Async` / `Oran7AppConfigManager` / `Oran7ThemeProfileManager` / `Oran7LogSink`。P2-2 日志的有界队列、滚动文件、退出排空与脱敏见 [[LOGGING - 日志与脱敏]]。

## 类名统一（2026-10-24）

本目录的 12 个项目自有类已补齐 Oran7 前缀；对应源文件、头文件、保护宏与构建清单同步更新。运行逻辑及持久化键名不变。

| 原 C++ 类名 | 当前 C++ 类名 | QML 兼容接口 |
|---|---|---|
| `Client` | `Oran7MediaClient` | Client（URI Client） |
| `ApplicationContext` | `Oran7ApplicationContext` | — |
| `AppConfigManager` | `Oran7AppConfigManager` | — |
| `GlobalHelper` | `Oran7GlobalHelper` | — |
| `Logger` | `Oran7Logger` | — |
| `ConsoleLogger` | `Oran7ConsoleLogger` | — |
| `LyricsModel` | `Oran7LyricsModel` | LyricsModel（URI LyricsModel） |
| `LrcParser` | `Oran7LrcParser` | — |
| `BilibiliAuthManager` | `Oran7BilibiliAuthManager` | BilibiliAuthManager（URI BilibiliAuth） |
| `GlobalEventFilter` | `Oran7GlobalEventFilter` | globalEventFilter 上下文属性 |
| `WindowApi` | `Oran7WindowApi`（P2-1 已删除） | 无注册/调用，正式窗口 API 由插件 Oran7Api 提供 |
| `OnlineLyricsFetcher` | `Oran7OnlineLyricsFetcher` | — |

本次验证：Release 主程序、Oran7Core 与 QML 插件构建通过；现有 P0/P1 定向检查通过；QML 注册名/URI/上下文属性与改名前一致；C++ 代码按命名映射逆向核对无额外逻辑改动。未启动主程序。

## 对外接口

### `Oran7MediaClient`（QML 单例 `Client`）

新增只读 Client.playback（Oran7PlaybackState* CONSTANT），状态与 Oran7Track 音乐值快照通过 NOTIFY 更新。状态枚举类型在 Client URI 注册为不可创建，实例由 Oran7MediaClient 持有，不在插件重复编译。

曲目值类型独立放在 `Oran7Track.h`，沿用只读 Q_GADGET 字段：`path/title/artist/album/cover/timesize/songId/isVip`。`timesize` 保持秒数字符串，以兼容当前配置和模型。QML 输入采用 QVariantMap，进入边界后转换为 Oran7Track；输出信号传递 `const Oran7Track&`，QML 直接读取字段。无效/负 songId、没有 path 且 songId 为 0 的选择不会覆盖当前曲目。大 ID 可用十进制字符串提交，避免输入先经过 JS 浮点数丢失精度。

| 命令 | 语义 |
|---|---|
| `selectMusicTrack({path,title,artist,album,cover,timesize,songId,isVip})` | 原子选择音乐快照；网络歌同步持久化，播放另由命令触发 |
| `persistCurrentMusic()` | 持久化当前网络音乐快照，不再接受重复元数据参数 |
| `playMusic(source)` / `playVideo(source)` | 本地音频/视频入口 |
| `playNetworkSong(songId, fallbackUrl, isVip)` | 网络音乐或带兜底地址的音乐；同一播放器目标可暂停/恢复 |
| `stopPlayback()` | 主动停止并取消待解析/延迟起播请求 |
| `seekTo(seconds)` / `setVolume(volume)` / `setPlayMode(mode)` | 进度单位为秒；音量 0～100；模式 0～3 |
| `playNext()` / `playPrevious()` | 发出通知，由 QML 决定目标 |
| `mediaSource()` / `currentSongId()` / `currentIsVip()` | 查询实际播放器目标、选中歌曲身份 |
| `scanLocalMusic(directory)` / `importLocalMusic(files)` / `removeLocalMusic(files)` | 曲库扫描、导入、删除 |
| `focusLocalMusic(path)` | 本地音乐焦点；排序由 tracks.local.moveTrack 直接同步曲库 |
| `Client.tracks` | 统一集合入口；整合保存/恢复由 C++ 协调，沿用原配置键 |
| `restoreConfiguration(engine)` / `saveConfiguration(engine)` | C++ 启动/退出门面；配置管理器先加载，调用方最终落盘 |
| `shutdown()` | 取消后台任务并关闭播放器/采集服务，可重复调用 |

渲染/录屏命令：`attachVideoItem(RenderObject, host)`、`setScaleMode(RenderObject, ScaleMode)`、`renderBlackFrame(RenderObject) -> bool`、`videoItem(RenderObject)`、`screenCapture()`、`screenCaptureMonitors()`、`screenCaptureSettings()`、`applyScreenCaptureSettings(...)`、`screenCaptureRecording()`。渲染创建、源尺寸及设备绑定留在桥内，Client 不再公开无调用的转发接口。

| 信号 | 消费方 |
|---|---|
| `focusedMusicRestored(track)` / `playerVolumeConfigLoaded(volume)` | 恢复/元数据补全通知；底栏仍直接绑定 playback.currentTrack |
| `playbackStopRequested()` | 内部停止旧播放器的通知；视频清理 UI，不作命令使用 |
| `playProgressUpdated(int,double)` / `totalDurationUpdated(int)` | 滑条、歌词、视频进度 |
| `stopIconUpdated()` / `playNextTriggered()` / `playPreviousTriggered()` | 图标、自动切歌 |
| `lyricsAvailable(QString)` / `lyricsUnavailable()` / `onlineLyricsSearching()` / `onlineLyricsNotFound()` | 歌词视图 |
| `networkPlayResolveFailed(qint64)` | 自动下一首/暂停 |
| `videoRenderInfoUpdated(QVariantMap)` | 渲染信息浮层 |
| `screenCaptureApplyFinished(bool,QString)` | 抓屏设置窗 |

`initializeConnections`、逐项配置恢复/保存、`publishFocusedMusic`、详情补拉等为 private。移除无调用的身份局部 setter、元数据探测/排序/渲染冗余转发；公开 API 不保留旧名别名，QML/C++ 调用方已一起迁移。Controller 内部播放/停止请求直接接回控制器，Client 不再通过自身信号驱动播放。陈旧 TCP 服务继续默认关闭，旧六参数回包仅在 Client 边界适配到 tracks 集合；旧 ID 继续当作播放入口，不冒充网易云 songId。

### 其他单例

| 对象 | 关键成员 | 说明 |
|---|---|---|
| `BilibiliAuth` | `isLoggedIn/userName/qrCodeUrl/loginState`；`startLogin/cancelLogin/logout` | 另有 C++ 专用 `cookieString()/rawCookieString()/ffmpegCookieString()/dedeUserId()` |
| `Oran7Network` | `latencyMs/online`、`start/stop`、`cookieExpired()` | `cookieExpired` 无消费者 |
| `Oran7Async` | `run(op,args,completed,failed)`、`cancel(taskId)`、`cancelAll()` | handler 见 [[ASYNC - 异步与线程]] §3.2 |
| `Oran7LyricsModel` | `currentLine/currentText/hasLyrics/count`、`loadFromFile/updateTime/clearLyrics` | 每个视图各持一个实例 |
| `Oran7Sound` | `playOpen()/playClose()/expandItem()`、`volume` | 宏生成，见 `Oran7Definitions.h:38-43` |
| `globalEventFilter` | 方向键按下/释放、`escapeKeyPressed`、`keyCombinationTriggered` | 原生热键经定时器转安全时机发信号 |

## 关键策略

### Oran7MediaClient 的 8 个职责（拆分重点）

1. **播放门面（P1-2 第五批已拆）**：Oran7PlaybackController 独占 Native unique_ptr、消息代际、100ms 进度/节流和 300ms 切歌定时器；Client 不再从 Native 消息线程执行成员函数。
2. **网络歌门面（P1-2 第四批已拆）**：Oran7NetworkSongResolver 独占地址缓存、请求代际、HTTP 尝试编号、外链分支、解析重试和起播缓存补救。Client 保留同歌暂停判定、状态和持久化，转发兼容失败信号。
3. **曲库门面（P1-2 第三批已拆）**：Oran7LocalLibraryService GUI 快照/顺序独占，后台串行值任务，原信号推送；Oran7MediaInfoReader 负责无状态元数据读取。
4. **配置门面（P1-2 收尾已拆）**：委托 Oran7ConfigBinding 处理原键、几何、音量、音乐恢复、列表与路径；Client 只协调服务结果。
5. **歌词门面（P1-2 第二批已拆）**：Oran7LyricsService 接收路径/元数据快照，接管匹配、级联请求、过期隔离及缓存生命周期；Client 注入同步元数据读取函数并转发四个原信号。
6. **渲染门面（P1-2 首批已拆）**：转发到 `Oran7RenderBridge`，桥独占两个槽位、尺寸/缩放、黑帧与设备引用。视频线程仅写共享帧邮箱，GUI 取帧后访问渲染项；Client 负责连接当前播放器与采集控制器。
7. **录屏门面（P1-2 第六批已拆）**：Oran7ScreenCaptureBridge 拥有采集控制器，接管参数转发、采集项绑定、排队通知和关闭；Client 保留原 QML 接口。
8. **遗留 TCP 门面（陈旧未启用）**：委托 Oran7LegacyServerClient；默认无 Socket/连接/SQLite，v1.0 之后再评估官方服务器。

### 生命周期

```
main() → Oran7ApplicationContext::instance()
          └─ new Oran7MediaClient("127.0.0.1",55711)
               └─ 创建 m_captureBridge → 独占创建采集控制器并接线 → InitSignalsAndSlots()
exit   → aboutToQuit 链（见 ARCHITECTURE §4）
          └─ ~Oran7ApplicationContext → delete Oran7MediaClient
```

## 易踩坑

1. **历史问题（2026-10-24 已修复）：`m_screenCap` 赋值晚于连接**：`Oran7MediaClient` 构造函数调 `InitSignalsAndSlots()`（`Oran7MediaClient.cpp:95`），而 `m_screenCap` 在 `Oran7ApplicationContext.cpp:42` 才赋值 → `:204-211` 两次 `connect(nullptr,...)` 失败；`screenCaptureApplyFinished` 永不发射，抓屏错误也不进日志。现已在连接前创建控制器。
2. P0 已修复：`renderBlackFrame()` 查询槽位并判空，无效时返回 false。
3. P0 已修复：`attachVideoItem()` 校验宿主、控制器及创建后的渲染项。
4. P2-1 已删除无调用且未注册的 `Oran7WindowApi`；正式导出的 `Oran7Api` 保留。
5. P1-4 已删除旧 Oran7MusicSearchService 及 QML 注册；搜索使用共享线程池 handler 和统一 DTO。
6. `setLoggedIn()` 是空实现（只 emit），登录态真正来源是 `CookieStore`。
7. P0 后：退出时先停止采集，Oran7MediaClient 析构体内销毁控制器；播放器后台销毁仍需保留原线程池排空与生命周期守卫。
8. 历史锁序问题已随 Controller/ConfigBinding 拆分消除：Client 不再持有这两把锁；当前 Native 由 Controller GUI 独占。

## 待办（模块内）

- [x] 把 `Oran7MediaClient` 按职责拆成 `Oran7PlaybackService` / `Oran7LocalLibraryService` / `Oran7NetworkSongResolver` / `Oran7LyricsService` / `Oran7RenderBridge` / `Oran7ScreenCaptureBridge` / `Oran7ConfigBinding`。
- [x] 状态/位置/时长/音量/模式经 Client.playback 暴露。
- [x] 音乐元数据与 songId/VIP 统一到 Client.playback.currentTrack；BasicConfig 六项信息只读。
- [x] P1-3 四类列表统一 C++ 模型。
- [x] P1-7：Client 以 unique_ptr 持有 Oran7PlaybackViewModel/Oran7CloudMusicService，集中归属/漫游及收藏 TTL/冷却；仅 EXE 编译，无新增全局单例。

P1-4：NcmSession 独占 CookieStore、CurlHttpClient、NcmUserApi、NcmSongApi、NcmPlaylistApi、WebLoginController；均为 unique_ptr，无 parent，QML 以 CppOwnership 借用。请求按 ID 隔离，详见 [[Ncm - 网易云接入]]。
- [x] 遗留 TCP 客户端隔离并默认停用；用户要求保留旧模块，v1.0 之后再评估官方服务器。
- [x] 修复 `m_screenCap` 接线时序，控制器在 Oran7MediaClient 构造内创建。
- [ ] 统一成单一 `NcmTrackMeta` 数据类型，替换所有"6 个 QString 参数"的信号。

## P1-2 首批边界

Oran7RenderBridge 不是新增全局单例，也不进入 Oran7Core 或 QML 插件。播放器的帧回调仅捕获共享邮箱与代际；邮箱覆盖、停止、失效和销毁均释放 AVFrame。桥销毁先失效 receiver，再在 GUI 线程释放持有的渲染项；Client 先停止采集。宿主只作视觉 parentItem，窗口变化及尺寸连接随宿主切换。渲染桥之外，歌词、曲库、网络解析与播放控制也已拆离；录屏桥也已拆离；配置绑定与详情服务、遗留会话隔离也已完成。

音乐选择快照 currentTrack 与实际解码 source 分开；元数据恢复先更新快照，再发兼容信号。详情补全用快照版本隔离。底栏标签直接绑定，首次创建可读取已恢复值，无需重发旧信号。

## P1-2 歌词所有权

Client → unique_ptr<Oran7LyricsService> → unique_ptr<Oran7OnlineLyricsFetcher> → unique_ptr<QNetworkAccessManager>；reply 由管理器持有、QPointer 观察。网络临时目录为每轮独立 QTemporaryDir，本地 LRC 缓存继续持久保留。服务无 Client/配置/应用上下文依赖，元数据读取同步注入；请求代际校验同时覆盖成功和失败。选择新歌先取消旧请求，内部切歌的旧播放器停止不得取消新歌词。

## P1-2 曲库边界

曲库状态由 Oran7LocalLibraryService 的 GUI 快照/顺序独占；后台工作只持有值与独立函数，串行文件操作，结果按请求代际落地。配置读写仍由 Client 调用原配置接口；遗留 TCP 的 SQLite 缓存继续留在原路径。P1-3 已将界面列表迁到 TrackLibrary.local，服务仅发布批量 tracksReady；原逐条信号已移除。元数据读取改为 Oran7MediaInfoReader 静态接口，不依赖 Client。

## P1-2 网络解析边界

Client → unique_ptr<Oran7NetworkSongResolver>；服务用 QPointer 观察 NcmSession 所有的 NcmSongApi，无 Client/配置/播放器依赖。普通歌立即交付稳定外链并后台缓存，VIP 等待解析，缓存 20 分钟；历史 URL 提取 ID、本地及无 ID 直传保留。用户代际防止信号同步重入，HTTP 尝试编号跨实例唯一，取消使旧结果失效并停止 3 秒重试定时器。两套一次性预算独立，内部起播不复位。Client 负责 currentTrack、播放状态、命令和原 networkPlayResolveFailed 通知；服务析构自动断开 API 回调，不强杀已在运行的 curl。

## P1-2 播放控制边界

Client → unique_ptr<Oran7PlaybackController> → unique_ptr<Oran7MediaPlayer>。Controller 只在 GUI 线程访问当前实例，旧实例移动到 Oran7Async 后台闭包释放，不再与 GUI 共享拥有指针。Native 消息循环是静态函数，仅持有所属实例、固定代际和共享值邮箱；析构用同一把锁使 receiver 失效，GUI 队列自动随 QObject 销毁清理。旧消息/旧时长/EOF/错误不影响新实例。

Controller 接收 Client 生命周期覆盖的 PlaybackState 引用和 GUI 渲染钩子，不访问配置、歌词、NcmSession 或 Client。准备期间暂停、100ms 进度、300ms 合并切歌、音量默认入口及兼容信号保留。Client 转发 mediaAccepted/stopped 来加载歌词并区分内部切歌；主动停止取消 Resolver，内部停止不取消新请求。文件删除通过 whenPlayersReleased 等待全部退役实例，退出取消延迟起播和等待回调；先 shutdown Controller 再关闭共享调度器。

## P1-2 录屏桥边界

Client → unique_ptr<Oran7ScreenCaptureBridge> → unique_ptr<Oran7ScreenCaptureController>，不叠加 QObject parent；控制器显式 CppOwnership。Client.screenCapture() 仍返回同一个控制器，原预览/录制/枚举/信号接口保留；桥不另造录屏状态副本，不替代原控制器参数校验、保存、回滚和 WHIP 决策。

桥的 GUI 渲染钩子连接 RenderBridge 的 ScreenCaptureRender 槽位；附着成功且实际项存在后再绑定控制器，同项换宿主沿用原去重逻辑。错误与应用结果先排队回桥，再转发给 Client；关闭后忽略未消费结果。shutdown 幂等停止现有采集并拒绝桥的新参数/绑定请求；兼容控制器句柄仍供原 QML 直接调用。Client 析构先销毁录屏桥/控制器，等待工作线程结束后才销毁渲染项。

## P1-2 胶水层收尾

ConfigBinding（普通类）拥有配置映射和路径值；TrackMetadataService（Client unique_ptr）独占网络详情与取消；LegacyServerClient（Client unique_ptr）保留默认停用的旧会话。Client 不再持有 appData、client_mutex、m_metaNam 或 Socket/SQL/UID/缓冲等业务字段。

保留服务所有权、只读 playback 入口及 QML 方法/信号、选歌与歌词取消、地址交付/播放组合、详情提交/持久化和文件删除等待等协调。Qt parent 持有的 PlaybackState/RenderBridge 引用继续有效，其他服务 unique_ptr 独占，不把字段藏到 Private 类冒充职责迁移。

自定义服务端及客户端状态见 [[Legacy - 自定义服务端（陈旧未启用）]]。ConfigBinding 没有收纳 HTTP/协议，两个业务分别隔离；Client 无重复音量状态，保存配置读取唯一 PlaybackState。

### Client 内部所有权统一（2026-10-24）

Oran7PlaybackState 与 Oran7RenderBridge 均由 Client 的 unique_ptr 独占，构造时不传 QObject parent。playback() 通过 get() 返回非拥有指针，并显式指定 QQmlEngine::CppOwnership，QML 只读借用。成员声明先放状态/渲染桥，再放依赖它们的播放器控制器/采集桥，正常析构与构造异常展开都确保依赖者先释放；析构主体仍先 shutdown 并释放控制器/采集桥。

### 统一列表模型（P1-3）

`Client.tracks` 为 Oran7TrackLibrary 借用指针，提供 local/integrated/liked/search 四个稳定模型对象。标准字段与旧界面只读别名共用同一数据，get(row) 返回值快照；QML 不允许通用角色写入。选择/动画/移动使用模型命令，喜欢/整合增删和搜索/收藏快照使用集合命令。集合维护身份去重索引和跨模型 isLiked/isIntegrated，避免界面扫描列表。

整合模型的 contentsChanged 自动落盘，恢复期间抑制写回；本地 contentsChanged 直接同步 LocalLibraryService 顺序，扫描期间拖拽也使用最新顺序。UI 状态通过 dataChanged 通知，不发送 contentsChanged，不触发持久化。曲库后台规则、队列与网络请求策略保持原职责边界。
