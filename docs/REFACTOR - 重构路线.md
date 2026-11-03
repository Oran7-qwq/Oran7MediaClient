---
status: 🔧 P3-1 首版与 P3-2/3 代码完成，运行待验收
updated: 2026-10-27
---

# 重构路线（REFACTOR）

> 本文回答：哪些地方值得改、为什么、按什么顺序改、怎么验收。
> 原则：**先修"看起来像设计问题、实际是 bug"的，再动结构**；每一步都能单独编译发布，不做大爆炸式重写。

## 0. 判断标准

一个改动值得做，必须至少满足一条：

1. **状态只有一个所有者**，否则会持续产生"界面不刷新/重启回退/切歌串台"类 bug。
2. **跨线程边界清晰**，否则会在低概率路径上崩溃（本项目已有 minidump 兜底，说明确实在崩）。
3. **删掉代码**（死链路、死 UI、死配置）——删代码是最安全的重构。
4. **能让某个模块被单测**（纯函数化）。

不满足以上任何一条的"风格统一"暂缓。

## 1. 总览

2026-10-24 目录整理：Client 及已拆出的 PlaybackState、RenderBridge、LyricsService、LocalLibraryService、MediaInfoReader 共 12 个源文件迁入 `src/cpp/Oran7MediaClient/`。后续 Client 内部服务也放在此目录。仅调整源码组织及 EXE include 路径，原有所有权、QML 接口和四个 Oran7Core 共享编译单元保持不变。

| 级别 | 编号 | 事项 | 影响面 | 工作量 | 风险 |
|---|---|---|---|---|---|
| P0 | 1 | 修 `m_screenCap` 空连接 + 抓屏回调断链 | 设置窗/日志 | 0.5h | 极低 |
| P0 | 2 | 修 `renderBlackFrame`/`attachVideoItem` 空指针 | 稳定性 | 0.5h | 极低 |
| P0 | 3 | 初始化 `Oran7MediaPlayer` 成员 | 稳定性 | 0.5h | 极低 |
| P0 | 4 | 修 QML 跨文件 id 引用 | 布局 | 1h | 低 |
| P0 | 5 | 每首歌复位 `m_retriedOnce` | 播放可靠性 | 0.5h | 低 |
| P0 | 6 | 抓屏 `static` 局部状态改成员 | 录制正确性 | 2h | 低 |
| P0 | 7 | `Oran7Theme` 单实例化 | 主题/退出 | 4h | 中 |
| P1 | 1 | 播放状态对象化（`Oran7PlaybackState`） | 全 UI | 2-3d | 中 |
| P1 | 2 | 拆 `Oran7MediaClient`（2050 行 → 7 个服务） | 全项目 | 5-8d | 中高 |
| P1 | 3 | 曲库/收藏/搜索统一 `QAbstractListModel` | 全 UI | 3-5d | 中 |
| P1 | 4 | 网易云 DTO/解析统一 + 请求 ID | 网络 | 3d | 中 |
| P1 | 5 | 配置结构化（schema + 版本 + 迁移） | 持久化 | 2d | 中 |
| P1 | 6 | 录屏异步化 + 参数化 + WHIP 决议 | 录屏 | 3-5d | 中 |
| P1 | 7 | QML 业务逻辑下沉 + 巨型文件拆分 | UI | 4-6d | 中 |
| P1 | 8 | 单一任务调度器（合并两个线程池） | 性能/退出 | 1-2d | 低 |
| P1 | 9 | 启动加载窗口 + 主界面就绪后显示（转入 P3-1） | 启动入口/窗口/配置/首帧 | 实施前拆批评估 | 中 |
| P2 | 1 | 剩余死代码/资源核查（部分已完成） | 源码/资源/注册 | 按核查清单估算 | 低至中 |
| P2 | 2 | 日志队列/落盘/脱敏收尾（已有分级基础） | 日志/退出 | 实施前拆批估算 | 中 |
| P2 | 3 | 凭据 DPAPI 存储与旧配置迁移（代码完成，待运行验收） | 登录/持久化 | 实施前拆批估算 | 中 |
| P2 | 4 | 构建收敛与现有检查正式接入（部分已完成） | 构建/插件/检查 | 实施前拆批估算 | 中 |
| P2 | 5 | 主题类型/通知/深色/差量配置（共享实例已处理） | 主题/配置 | 实施前拆批估算 | 中 |
| P2 | 6 | 文案国际化与字体收敛（中英已实施，待运行验收） | QML/主题 | 实施前拆批估算 | 低至中 |
| P2 | 7 | QRhi 统一消费与采集同步（代码完成，待设备验收） | 播放/预览/录屏 | 已实施 | 中高 |
| P3 | 1 | 启动协调与主界面就绪显示（承接 P1-9，首版代码完成） | 启动/配置/窗口 | 首版构建/离线通过，待启动验收 | 中 |
| P3 | 2 | 预览启停、参数重启与恢复异步化（代码完成，设备待验收） | GUI 响应/采集生命周期 | 已实施并通过离线检查 | 中高 |
| P3 | 3 | 采集 worker 与格式转换职责分离（代码完成，设备待验收） | 采集/转换/资源所有权 | 已拆分并通过离线检查 | 中 |

---

## 2. P0：先修正确性

> 2026-10-24：七项已完成代码修改，Release 构建与运行验收状态见本节末尾。保留 P1/P2 范围，不扩展为结构重写。

### P0-1 抓屏回调断链

`Oran7MediaClient` 构造函数在 `InitSignalsAndSlots()` 前创建 `Oran7ScreenCaptureController(this)`，保证错误和应用结果信号可以连接；删除 `Oran7ApplicationContext` 的补建分支。控制器由 `Oran7MediaClient` 持有，并在 Oran7MediaClient 析构体内提前销毁，等待采集退出。

**人工验收**：修改 fps/显示器/鼠标开关后能收到应用结果；失败时显示错误或回滚结果。

### P0-2 渲染项空指针

`renderBlackFrame()` 改为返回 `bool`，使用 `constFind` 查询，不为不存在的槽位创建空记录；槽位或渲染项无效时返回 `false`。`createVideoItem()`/`attachVideoItem()` 在创建失败、宿主为空或抓屏控制器无效时提前返回。

**人工验收**：未打开视频/录屏页时停止或清屏不崩溃，切换页面后仍能正确挂载渲染项。

### P0-3 播放器成员未初始化

`msg_thread_`/`data_source_` 默认 `nullptr`，`mp_state_` 默认 `MP_STATE_IDLE`，音量默认 25。销毁沿用已有判空和重复销毁守卫；设置数据源时先复制成功，再释放旧地址，避免泄漏并保留分配失败时的旧数据源。

**人工验收**：正常播放、打开失败、快速切换和退出均无新增崩溃。

### P0-4 QML 跨文件隐式引用

纠正原定性：QML 动态作用域允许通过创建上下文解析部分跨文件标识符，不能仅凭跨文件引用断言一定出现 `ReferenceError`。本项消除已识别的隐式依赖：

- `main.qml` 显式传入 `RightPage.captionExpanded`。
- `RightPage.musicPageActive` 从当前栈页派生，主窗口绑定底栏高度与透明度。
- 底栏歌词背景尺寸读取 `root.Window.window`，不再引用外部 `mainWindow` id。

**人工验收**：本地/搜索页显示底栏，视频/录屏页收起底栏；标题栏展开状态与全屏歌词背景尺寸保持正确。

### P0-5 网络重试预算与旧请求串歌

新播放意图由 `resetNetworkPlayRequest()` 创建代际并复位重试/外链起播标志；记录用户请求目标，点回仍在播放的旧歌曲时撤销另一首待处理请求；退出先使请求失效并停止采集。内部起播通过 `startMusicPlayback()`，不在 `preparePlayingMedia()` 清空解析状态，否则会破坏非 VIP 外链先播、后台解析的流程。

P0 当时为 `NcmSongApi.resolveUrls` 增加播放关联号与专用结果信号，保留兼容查询广播。P1-4 已进一步收敛为唯一 `urlsResolved(requestId, map)`，所有查询均分配 ID；当前契约见 P1-4。旧结果隔离与重试代际校验继续保留。

**人工验收**：VIP 解析失败等待重试期间切换另一首/本地歌/视频，旧结果与旧重试不得覆盖当前媒体；每首新歌独立获得一次重试预算；非 VIP 外链起播不被后台解析打断。

### P0-6 抓屏局部 static 生命周期

诊断 pts、帧率计数和 `SwsContext*` 改为 worker 成员；转换使用 `sws_getCachedContext`，在 `cleanup()` 释放并复位。初始化失败也清理已有资源；停止录制后复位下一次录制的起始标志。删除默认写入 `C:/temp` 的 NV12 dump，避免跨实例静态文件句柄和未关闭文件。

**人工验收**：反复启动/停止预览、应用分辨率变化、连续录制两次，时间戳与画面正常。

### P0-7 主题单实例

采用最小构建调整：`Oran7Theme.cpp/.h/_p.h` 从 EXE glob 排除，仅在现有 `Oran7UI_Impl` DLL 编译。类使用显式导入/导出宏，EXE 通过已有 DLL 链接调用同一实例及元对象；不迁移到 `Oran7Core`，避免本次引入生成器和 QML 注册的重组。

`cleanup()` 改为 DLL 内实现；QML 工厂明确 `CppOwnership`，仍由既有 post routine 在 QML 引擎销毁后统一清理。组件指针表跟随实例注册/清理，避免清理后再次创建时遗留指针。

**人工验收**：切换/保存主题与重启恢复正常，退出无主题析构崩溃。

### P0 验证状态

- [x] Qt 6.7.3 / MinGW Release 构建完成（Ninja -j 2，最终增量构建退出码 0）。
- [x] 网络 API 七项定向检查通过：请求乱序、查询隔离、旧信号兼容、网络失败、HTTP 失败、同步发送失败、其他 API 回调隔离。使用真实 NcmSongApi 与替身传输，不启动主程序。
- [x] 三个修改的 QML 文件经 qmlformat 语法解析通过（未改写文件）。
- [x] 导入/导出核对：EXE 从 Oran7UI_Impl.dll 导入 cleanup，主题实例与元对象由 DLL 导出，EXE 不再生成主题 moc。
- [x] 用户已于 2026-10-24 确认 P0 运行验收通过，无额外事故。
- P0 已验收；后续 P1 的运行验收单独记录，不沿用 P0 结论。

---

## 3. P1：结构重构

### 本轮实施进度（2026-10-24，首批）

新增或拆分的自有类均使用 **Oran7** 前缀。按用户追加要求，GlobalSingleton 中 12 个既有类已统一前缀，18 个对应文件同步改名；其余模块既有类不在本轮批量改名。QML 注册名、URI 与上下文属性保持兼容，映射见 [[Core - 单例与胶水层]]。

| 项目 | 本批结果 | 后续范围 |
|---|---|---|
| P1-1 | 已实现 Oran7PlaybackState：状态、实际播放/打开意图、位置、时长、音量、模式、媒体源、错误和 Oran7Track 音乐快照；BasicConfig 状态与六项音乐信息均只读 | 列表模型/队列策略/播放焦点留待 P1-3/7；旧进度/歌词/恢复信号保留兼容 |
| P1-8 | C++ 与 QML 共用 Oran7Core 内的一份池；QML 阻塞网络组并发上限 2；Oran7TaskHandle、QML cancel/cancelAll、退出统一排空 | 取消不强制中断运行中的 curl；异步 CurlHttpClient 自身不在该限流组 |
| P1-2 | 已拆出 Oran7RenderBridge、Oran7LyricsService、Oran7LocalLibraryService、Oran7NetworkSongResolver、Oran7PlaybackController、Oran7ScreenCaptureBridge、Oran7ConfigBinding；补充拆出详情服务及默认停用的旧会话，Client 收敛为胶水层 | 本项代码收尾完成，实际运行待验收；列表模型统一属于 P1-3 |
| P1-3 | 四类列表已统一为 Oran7TrackListModel，Oran7TrackLibrary 独占模型与身份索引，批量发布/去重/成员关系和整合持久化接入 | 代码完成，用户运行验收待进行 |
| P1-4 | 网易云 DTO/解析与请求 ID 已统一，废弃网络链路已删除 | Release 与定向检查通过，用户运行验收待进行 |
| P1-5 | 配置分组、schemaVersion=2 迁移、默认值深合并、原子保存与备份已实施 | 构建与验证结果见本节；用户运行验收待进行 |
| P1-6 | 录制会话异步化、参数配置/面板、WHIP 保留并接通状态 | 代码完成，实际设备/服务运行待验收 |
| P1-7 | Oran7PlaybackViewModel/Oran7CloudMusicService 接管播放归属、漫游和收藏策略；列表拆分、旧 SearchPopup 删除 | 代码完成，运行待验收 |
| P1-9 | 新增计划：前置加载动画与主窗口显示时序，尚未实施 | 高优先级启动体验优化，可作为独立小批次安排 |

播放消息中的状态值/时长以值快照传回 GUI，落地前再次比较播放器代际。停止命令失效网络请求及延迟切歌；切歌期间的多次请求合并到最新路径。已去掉消息循环固定 10ms sleep。

**本批验收**：Release 构建、状态与调度定向检查、修改 QML 的语法解析；主程序不由代理启动。运行回归重点为暂停/继续、播放中和暂停时 Seek、快速切歌后停止、VIP/非 VIP 及退出。最终验证结果：

- [x] Qt 6.7.3 / MinGW Release 最终构建通过，EXE、Oran7Core 与 QML 插件均已编译。
- [x] build/p1-checks 定向检查通过：状态转换、QML 绑定、音量/模式校验、暂停 Seek、值/void/仅可移动结果、QML 成功与失败回调、单个/批量取消、上下文销毁、网络组并发上限、通用任务推进、退出排空与拒绝新任务。
- [x] 七个修改的 QML 文件语法解析通过。
- [x] 使用当前源码重建并通过 P0 的七项网络关联回归检查。
- [ ] 用户运行验收本批 P1（未启动主程序）。


### P1-1 播放状态对象化（最高性价比的结构改动）

**原状**：Oran7MediaClient 无播放快照，QML 自行维护播放态。**首批现状**：Client.playback 已成为播放状态、位置、时长、音量和模式的界面权威；音乐元信息已迁入 currentTrack；列表模型与队列策略暂未迁移。

**目标**：

```cpp
class Oran7PlaybackState : public QObject {
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)      // Idle/Opening/Playing/Paused/Seeking/Ended/Error
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(int mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(Oran7Track currentTrack READ currentTrack NOTIFY currentTrackChanged)  // 或用 trackModel+index
signals:
    void stateChanged(State);
    void positionChanged(qint64);
    // ...
};
```

`Oran7PlaybackState` 定义唯一真相；`Oran7MediaPlayer` 的内核标志降级为私有实现细节，通过 `invokeMethod` 回 GUI 后更新 `Oran7PlaybackState`；QML 删掉 `isPlaying`，改为绑定 `Client.playback.state`。

**收益**：一次解决"两套状态机""QML 猜状态""暂停后 C++ 不知道""播放模式分裂"。
**风险**：触及所有播放 UI；建议先加对象并双写（旧信号保留），验证后再删旧信号。

#### P1-1 续批：音乐快照（2026-10-24）

- 在 Oran7PlaybackState.h 中定义 Oran7Track 值类型（Q_GADGET）：path/title/artist/album/cover/timesize/songId/isVip，QML 字段与 currentTrack 属性均只读，整体变更通知去重。
- currentTrack 表示当前选择/启动恢复的音乐信息；path 为列表入口或恢复路径（VIP/漫游允许空），source 是实际播放器打开目标（可为解析后的 CDN 地址或视频/直播）。停止、切到视频时保留音乐快照，沿用原底栏语义。
- 本地/整合/搜索/收藏/漫游入口统一先调用 Client.selectMusicTrack 提交完整快照，再发播放命令；删除 QML 中所有 currentMedia* 写入与零散身份设置。currentSongId/currentIsVip 不再另存字段，读取同一快照。
- 启动恢复与本地元数据分析先更新快照，再发 focusedMusicRestored 兼容信号；底栏封面/标题/作者直接绑定，新实例无需等旧信号重发。rememberLastMedia 和两个身份设置接口保留兼容，均写入同一快照。
- 启动详情补全携带 trackRevision，切走再切回相同 songId 也拒绝旧回复；只补缺失专辑/时长，只有持久化 songId 仍匹配才写回。

本批验证：状态/QML 定向检查通过（完整值快照、同封面切歌、空 URL VIP、只读字段/整体赋值拒绝、版本隔离、只补缺字段、入口路径与解码地址分离、同歌保留已补全字段/不同歌清空旧详情），四个 QML 文件语法解析通过。Qt 6.7.3 / MinGW Release 最终增量构建通过（Ninja -j 2，退出码 0）；主程序未启动，由用户验收。

人工验收：启动恢复、本地歌/整合歌/收藏/VIP/漫游切换、同专辑切歌、底栏喜欢与整合状态、视频切回音乐、快速切歌后迟到详情不得覆盖新歌。

**P1-1 运行回归修复：播放成功但列表失去高亮（2026-10-24）**

selectMusicTrack 同步触发快照变更，公共列表已把选中索引改到新歌曲；页面 dealClicked 随后读这个新索引，把播放中的切歌误判为当前歌曲暂停，并清空索引。现删除本地/搜索页基于 UI 索引猜测播放动作的分支，统一提交一次播放命令；公共列表按 currentTrack/state/音乐播放焦点同步索引，从完整模型定位，不依赖可见 delegate 回应。网络空路径按 songId 定位，整合路径同时匹配 filepath/url。

build/p1-list-focus-checks.cjs 抽取真实 QML 函数，使用同步重入的快照/播放替身：旧代码复现失焦，修复后覆盖本地/搜索切歌、暂停/继续、空 URL VIP 切换、整合 URL 定位、其他播放焦点及不在模型中的歌曲。QML 语法与 Release 构建验证通过；未启动主程序，运行验收由用户完成。

### P1-2 拆 `Oran7MediaClient`

**证据**：2050 行 .cpp、8 个职责（见 [[Core - 单例与胶水层]]）、锁序不一致、`m_d3d11Slots` 跨线程读。

**目标结构**：

```
Oran7MediaClient（只剩 QML 门面 + 组合，<300 行）
├── Oran7PlaybackController      mp_ 生命周期、切歌防重入、进度定时器
├── Oran7PlaybackState           状态真相（P1-1）
├── Oran7NetworkSongResolver     songId → url、缓存、-110 重试
├── Oran7LocalLibraryService     扫描/增删/排序，输出 TrackModel
├── Oran7LyricsService           LRC 匹配 + 在线级联 + 缓存生命周期
├── Oran7RenderBridge            m_d3d11Slots、尺寸/缩放、设备注入（GUI 线程独占）
├── Oran7ScreenCaptureBridge     9 个透传方法 + 参数（不再直接持有控制器也可以）
└── Oran7LegacyServerClient      陈旧未启用，保留至后续版本评估
```

**迁移顺序**：先抽 `Oran7RenderBridge`（自包含、风险低）→ `Oran7LyricsService` → `Oran7LocalLibraryService` → `Oran7NetworkSongResolver` → `Oran7PlaybackController`。每抽一个，`Oran7MediaClient` 里对应方法改为一行转发。

#### P1-2 首批：Oran7RenderBridge（2026-10-24）

- Oran7MediaClient 组合一个非单例 Oran7RenderBridge，仅编译进 EXE。QML 的 Client 注册、两个枚举、渲染命令与信息信号保持兼容。
- 桥持有两个渲染槽位、宿主连接、源尺寸、缩放模式和 Qt D3D11 设备引用；设备就绪通知由门面转交当前播放器。采集控制器仍由 Client 持有，挂载成功后接收渲染项，抓屏设置不重置。
- 视频刷新线程不再访问槽位表或 QQuickItem。每个播放器获得固定代际的共享邮箱回调；邮箱最多保留一帧与一个 GUI 投递任务，覆盖立即释放旧帧；停止/新播放器/桥销毁后旧回调只释放帧。
- GUI 线程取帧后查询有效渲染项并转交所有权；无渲染项时释放。渲染项的 QObject 父对象为桥，parentItem 为当前宿主，更换宿主时重连尺寸/windowChanged。
- Fit/Fill 像素对齐算法沿用原实现；本批未调整渲染节点双驱动、解码内核或采集纹理同步；后续 P2-7 已实施 QRhi 消费与同步专项。

验证记录：

- [x] Qt 6.7.3 / MinGW Release 完整构建与最终增量构建均通过（Ninja -j 2，退出码 0）；EXE、Oran7Core 与 QML 插件已构建。
- [x] build/p1-render-checks 隔离检查通过：使用真实桥与 Qt Quick 宿主、渲染项/设备提供者替身，无窗口与 GPU 初始化；验证 Fit/Fill、最新帧合并、停止/切换代际、宿主切换/销毁、生产线程与桥并发析构、失效回调及 11505 个 AVFrame 对应缓冲全部释放。
- [x] QML 两个枚举的名称与数值逐项比对一致，未修改 QML 文件与注册。
- [ ] 用户运行验收（未启动主程序）。重点：本地视频/直播、暂停与 Seek、快速切换后停止、窗口缩放/Fit/Fill、离开并返回视频/录屏页、录屏预览与退出。

**P1-2 运行回归修复：VideoInfo 面板 undefined（2026-10-24）**

GUI 接收视频帧后，updatePaintNode 经常先于 beforeRendering 消费帧；原信息发布只在 beforeRendering 取到帧时执行，因此面板收不到信息。将字段采集与每秒节流抽成 D3D11VideoItem::publishVideoInfo，两条消费路径均调用。QML 字段与来源标签不变。此前桥的替身检查没有覆盖真实渲染项的信息生产路径，本次补真实 D3D11VideoItem 的无窗口回归。

验证结果：Release 增量构建通过；build/p1-videoinfo-checks 使用真实渲染项/渲染节点（不创建窗口、不初始化 GPU），修复前稳定复现缺失信息（退出码 1），修复后通过（退出码 0），覆盖两条消费路径、源尺寸/格式与共享节流；桥的异步信息转发/来源标签检查通过。主程序运行仍由用户验收。

#### P1-2 第二批：Oran7LyricsService（2026-10-24）

- 非单例服务，仅编译进 EXE。Client 使用 unique_ptr 独占，不叠加 QObject parent 所有权；服务独占在线获取器、获取器独占网络管理器。reply 属于管理器，使用 QPointer 观察；取消先失效并断开回调再 abort，析构时先释放管理器。
- Client 提交实际媒体路径、歌词目录和 Oran7Track 值快照。服务负责本地同名 LRC、元数据校验、网易云 songId/URL id 解析、LRCLIB 精确/模糊 → 网易云搜索/歌词级联；本地 ffprobe 通过同步 MetadataReader 注入，不反查 Client/配置。元数据分析仍同步，异步化不在本批。
- 每轮拥有新获取器和递增代际；成功、失败通知排队回服务后均校验代际。选择不同歌曲时立即取消旧歌词，包括 VIP 仍等播放 URL 的阶段；内部切歌停止旧播放器不会取消新歌词。视频/直播入口停止音乐歌词查询，暂停同一首保留歌词。
- 网络歌词放入本轮 QTemporaryDir，停止/换歌/析构自动删除；带 songId 的 CDN 地址生成 song_<id>.lrc，不再通过 URL/文件名猜结果归属。同名或相同 songId 的两轮请求拥有不同目录。本地下载继续使用 audioLRC 同名持久缓存，QSaveFile 原子写入。
- lyricsAvailable/lyricsUnavailable/onlineLyricsSearching/onlineLyricsNotFound 四个 QML 信号保持兼容；不可搜索或失败有明确 notFound 通知。删除 Client 的匹配/搜索/网络缓存字段与相关实现。

验证：build/p1-lyrics-checks 使用真实服务和获取器、替身 QNetworkAccessManager/reply 与 Cookie 提供者，不访问网络、不启动主程序。已覆盖本地缓存命中/下载保留、CDN id/鉴权头、完整降级链、排队旧成功/失败、在途取消、相同身份请求隔离、停止与析构资源释放。Qt 6.7.3 / MinGW Release 完整构建和最终增量构建均通过（Ninja -j 2，退出码 0），四个 QML 歌词信号签名与拆分前一致。

运行验收由用户执行：本地歌词、无本地歌词的在线搜索、VIP/CDN 网络歌词、无歌词提示、快速切歌与等待解析时切换、暂停/继续、音乐切到视频/直播、停止与退出；网络临时目录应自动清理，本地 .lrc 保留。

#### P1-2 第三批：Oran7LocalLibraryService（2026-10-24）

- Client 以 unique_ptr 独占非单例曲库服务，仅编译进 EXE；扫描、导入、回收站删除、顺序归一与曲库快照迁移。现有 QML 命令、localMusicListCleared/localMusicElementReady/localMusicListReordered 与配置键继续兼容，不提前替换 QML ListModel。
- 文件项与顺序只在 GUI 线程持有。后台任务只捕获工作目录、路径列表、独立读取函数、回收站函数和共享取消标志，不捕获 Client/服务。删除 Oran7AppData 三个曲库容器和无调用者的可变引用 getter，避免锁释放后仍把共享引用交出去。
- 所有文件任务串行排队，尚未执行的扫描合并；新请求使旧扫描失效，但不会打断已开始的文件操作。GUI 仅接收最新完整快照，扫描期间拖拽以最新顺序为准；发布顺序与持久化顺序一致，新增文件仍排在保存顺序之前。完成时一次清空/重建，扫描期间保留现有显示。
- 导入转到后台，正确解码 file URL，已存在目标不覆盖。删除仅限当前曲库根目录的普通文件，拒绝目录/外部路径/符号链接；回收站失败保留文件和封面，刷新后仍显示失败项。删除当前播放文件时先等待全部退役播放器释放句柄，再提交文件操作；退役播放器由后台闭包的 unique_ptr 负责释放。
- 新增无状态 Oran7MediaInfoReader，迁移 ffprobe JSON/标签/时长/封面读取；恢复、歌词与扫描共用。歌词读取函数不再捕获 Client。QProcess/QTemporaryFile/QSaveFile 管理进程与封面临时资源，避免同文件并发读取时共用 .raw 文件名。原同步读取入口和超时策略保留；扫描取消在文件之间检查，不强杀正在运行的 ffprobe。
- 公共列表对当前模型 count/listModel 变化延后一轮按歌曲身份恢复高亮，避免快照重建/导入在前方插行后仍使用旧下标。未改变播放点击接口或队列策略。

验证：build/p1-library-checks 使用真实曲库服务、真实元数据解析函数，文件全部在 QTemporaryDir；元数据探测和回收站操作为替身，不调用真实回收站、不访问用户媒体。已覆盖顺序/去重/过期扫描/扫描中重排、串行导入及重复保护、删除路径范围与失败保留、异常后继续、GUI 发布、关闭/析构和后台生命周期；QtQml 模型检查验证批量刷新后播放/选中高亮移动到正确行。Qt 6.7.3 / MinGW Release 最终构建通过（Ninja -j 2，退出码 0），公共列表 QML 语法解析及之前的点击/双击高亮回归通过；三个曲库兼容信号签名未变，两个新增模块只在 EXE 编译。

人工验收：启动扫描与恢复顺序、添加本地文件、连续刷新/导入、拖拽排序重启恢复、删除普通项/正在播放项/最后一项、导入后高亮跟随歌曲、歌词和封面读取、扫描期间退出。主程序由用户启动。

#### P1-2 第四批：Oran7NetworkSongResolver（2026-10-24）

- 新服务位于 `src/cpp/Oran7MediaClient/`，由 Client unique_ptr 独占，无 QObject parent，仅 EXE 编译；NcmSession 继续持有 NcmSongApi，服务用 QPointer 观察，不新增全局单例。
- 迁移稳定外链/VIP 等待分支、历史 URL ID 提取、20 分钟地址缓存、解析失败 3 秒后重试一次、起播失败使用缓存补救一次。两套预算独立，新请求/取消复位；内部切歌和补救不复位。
- 用户请求代际与 HTTP 尝试编号分开，每次尝试使用跨实例唯一编号；普通查询、旧歌、重复回调、旧重试及销毁后重建的迟到结果均不触发播放。取消停止成员 QTimer，析构自动断开 API 回调；不强杀已在运行的 curl。
- 服务只输出身份、地址和失败值结果，不修改 currentTrack/播放状态，不读取配置或操纵播放器。Client 保留同歌暂停判定、状态、持久化和原 networkPlayResolveFailed 信号；结果消费校验代际，兼顾同步 QML 通知触发新请求的重入。停止、本地/视频请求、删除当前媒体和退出沿用取消入口。
- Client 本轮移除约 170 行实现；具体行数随后续变动，不作为验收标准。QML 命令、注册和信号签名均未改。

验证：`build/p1-resolver-checks` 使用真实 Resolver + NcmSongApi，仅替换 HTTP 传输及注入缓存时钟，30 项检查通过；覆盖外链/VIP、Cookie provider、缓存过期/复用、乱序/重复/查询隔离、取消与同歌保留、两套预算、历史记录/本地兼容、多实例、析构、同步失败及信号重入。原列表点击/双击高亮回归通过。Qt 6.7.3 MinGW Release 主程序、Oran7Core 和 QML 插件构建通过（Ninja -j 2，退出码 0）；最终增量构建也通过。未启动主程序。

人工验收：普通/VIP 歌播放、同歌暂停继续、快速切换并点回旧歌、解析中停止/切视频、解析失败后的自动下一首、外链打开失败补救、退出期间仍有网络请求。下一批建议 Oran7PlaybackController。

#### P1-2 第五批：Oran7PlaybackController（2026-10-24）

- 位于 `src/cpp/Oran7MediaClient/`，Client unique_ptr 独占、无 QObject parent，仅 EXE 编译，不新增全局单例。Controller 以 unique_ptr 独占 Native 播放器，GUI 独占当前实例；退役时把所有权移动到 Oran7Async 后台闭包。
- 迁移创建/准备、播放暂停、准备期间暂停意图、Seek、音量/设备注入、100ms 进度与请求节流、300ms 合并切歌、停止/退出和 Native 消息处理。单次切歌定时器读取最新目标，主动停止/退出取消待起播；直接播放命令提前接管时不留下随后暂停新实例的旧定时器。
- Native 消息循环改为静态函数，仅捕获所属播放器、固定代际、共享事件邮箱；不捕获 Controller 或 Client。只投递关心的消息和发生时状态/时长值，GUI 校验代际后更新原 PlaybackState。邮箱以同一把锁保护投递与析构，析构先失效 receiver，再 join 当前实例；已退役实例可以在 Controller 销毁后独立结束。
- 文件删除等待迁入 whenPlayersReleased，全部退役实例释放后才在 GUI 提交曲库任务。初始化失败的实例也按 RAII 释放，不向未完成初始化的实例发送 pause。退出清空等待回调、取消延迟切歌，并在共享调度器关闭前提交 Native 释放。
- 源路径归 Controller，删除 Oran7AppData.CurMediaFilePath 和 Client 的 Native 原始拥有指针/递归锁/消息与切歌标志。Client 保留歌词/网络解析/配置组合，转发原 QML 命令与信号；内部切歌停止旧实例不取消新歌词/地址解析。音乐 currentTrack、QML 注册、配置键和队列策略不变。
- 同步信号通知触发新命令时检查命令版本，避免旧 EOF/错误/进度继续影响新播放。保留停止路径的 Windows 堆整理及现有内核音量默认入口；不修改 FFmpeg 解码、SDL 音频或 D3D 渲染实现。

验证：`build/p1-playback-checks` 使用真实 Controller/PlaybackState/Oran7Async/Qt 定时器，Native 和渲染 I/O 为替身，不播放媒体、不建窗口；53 项检查通过、12 个实例全部释放，覆盖准备期间暂停、Seek/时长快照/NaN 进度、切歌合并、停止/退出取消、旧消息、EOF/错误、后台释放线程、等待全部句柄、初始化失败、析构后投递和同步重入。原 QML 点击/双击高亮回归通过。Client QML 可调用声明及信号签名与拆分前一致。Qt 6.7.3 MinGW Release 主程序/Core/QML 插件全构建及最后增量构建均通过（Ninja -j 2，退出码 0）。未启动主程序，实际播放由用户验收。

人工验收：本地/网络/VIP/视频/直播播放与暂停、准备中暂停、快速连续切歌、切歌中停止、Seek、音量恢复、自然结束下一首/单曲循环、网络打开失败补救、删除正在播放文件、播放或解析期间退出。P1-2 尚余录屏桥和配置绑定；下一批建议 Oran7ScreenCaptureBridge。

#### P1-2 第六批：Oran7ScreenCaptureBridge（2026-10-24）

- 位于 `src/cpp/Oran7MediaClient/`，Client unique_ptr 独占桥，桥 unique_ptr 独占原采集控制器，均无 QObject parent，仅 EXE 编译，不新增全局单例。移除 Client 的 m_screenCap 和采集头文件依赖，ApplicationContext 也不再 include 采集实现。
- 参数/忙状态/启动配置转发、采集渲染项附着、错误与应用结果转发、关闭生命周期迁入桥。显示器原始序号/参数原样交原控制器处理，不复制配置、不增加第二份录屏状态，不修改其校验、回滚、录制/WHIP 决策和采集/编码实现。
- Client.screenCapture() 仍返回原控制器对象；显式 QQmlEngine::CppOwnership，保证无 parent 的 unique_ptr 对象不会被 QML GC 删除。原预览/录制方法、控制器信号/枚举和 Client 信号/命令签名不变；QML 文件未修改。
- 渲染附着成功且实际渲染项存在后才绑定；同项切换视觉宿主沿用控制器去重，不重启或重置用户参数。桥先创建对象再接线，错误和应用结果保持一个 queued 跳转；shutdown 幂等停止现有采集，屏蔽排队结果并拒绝桥的新写入/附着。原 QML 控制器句柄继续允许直接启停。
- Client 析构先 shutdown 并销毁桥/控制器，等待采集工作线程退出后才释放渲染桥的项。未提前实施 P1-6，不变更现有同步停止/重启、WASAPI 或 D3D 纹理同步策略。

验证：`build/p1-capture-bridge-checks` 使用真实桥、Qt queued 信号、QML 引擎/垃圾回收和无窗口 QQuickItem，采集控制器为 I/O 替身；34 项检查通过，2 个控制器全部释放。覆盖原句柄 QML 预览/录制调用、CppOwnership、GC、参数/原始序号透传、忙状态拒绝、失败回滚通知、同项换宿主、绑定失败、关闭后丢弃结果、析构先于渲染项。Client QML 可调用声明及信号签名与拆分前一致。Qt 6.7.3 MinGW Release 主程序/Core/QML 插件构建通过（Ninja -j 2，退出码 0）；移除间接头文件后 main.cpp 已显式包含其注册的 D3D11VideoItem 定义。未启动主程序/真实录屏/编码/推流。

人工验收：预览初次打开、切页/换宿主、显示器/fps/鼠标/日志参数应用与失败回滚、录制启停及录制中改参拒绝、MP4/WHIP、预览或录制期间退出。P1-2 尚余 Oran7ConfigBinding。

#### P1-2 第七批：配置绑定与胶水层补充收尾（2026-10-24）

实施范围经用户确认：配置绑定 → 歌曲详情补全 → 遗留 TCP 隔离 → 残留状态清理。实施中用户追加：自定义 TCP/Server 列为陈旧未启用，待 v1.0 正式版之后或后续版本再评估官方服务器。

- 新增 Oran7ConfigBinding：原配置键、窗口几何、音量保存/恢复、最近音乐、整合列表、曲库顺序及目录值集中映射。音乐恢复返回 Oran7Track 值，沿用本地探测、网络 ID/旧路径兼容逻辑；配置不持有播放器、HTTP 或重复音量状态。
- 新增 Oran7TrackMetadataService：独占 NAM，reply 由 NAM 持有、QPointer 观察，15 秒超时/原 URL 与请求头保留。新请求/退出取消旧请求；只返回带 songId/trackRevision 的详情值。Client 在 GUI 校验快照，补空字段、委托配置持久化并转发兼容通知。详情服务不读取配置或全局 Client。
- 新增 Oran7LegacyServerClient：把旧 Socket/收包/UID/处理器表/SQLite 移出 Client；源码保留，默认不创建 Socket、不发起连接、不打开旧库，原登录入口不自动启用。测试可显式开启本机回环旧协议。旧服务端本来就未从 main 启动。说明见 [[Legacy - 自定义服务端（陈旧未启用）]]。
- 清理 Oran7AppData 整个旧混合容器、无用途 lastDirectory/只写 videoAbsoluteFilePath、重复 playerVolume；保存音量读唯一 PlaybackState。移除 Client 共享锁及无调用者 getClientMutex。Client 头文件不再持有网络管理器、Socket、SQL 数据库、UID、收包/处理器表或路径/播放缓存。
- Client 保留服务所有权、QML 接口、信号转发和跨服务协调（选歌/歌词取消/地址交付/删除等待）。新增类位于同一模块，仅 EXE 编译、不新增单例；QML 可调用声明和信号签名逐项对照迁移前一致。实现从 1156 行减至 536 行，头文件 315 行至 262 行；行数仅记录，不作为验收目标。
- 旧协议仅补迁移相关边界与生命周期：最小帧长/截断校验、无符号 UID、SQL 连接名隔离及析构 removeDatabase；保留有效格式与去重，不推进官方服务端功能。

验证：build/p1-facade-checks 的 47 项检查通过；配置/HTTP/探测用替身，旧协议只访问本机测试服务器和临时数据库，默认停用的零网络/数据库副作用已覆盖。原列表点击/双击高亮回归通过，Client QML 声明与信号签名保持。Release 全构建及停用调整后的增量构建通过（退出码 0），未启动主程序。

人工验收：重启恢复音乐/专辑/时长、网络详情晚到不覆盖新歌、窗口几何/音量、整合列表与本地顺序持久化、选歌/暂停/停止/退出。默认不再尝试连接旧服务器或打开旧 SQLite。这里只完成 P1-2 收尾，P1-3～7 仍按原路线安排，不等同整个 P1 或 P2 完成。

#### P1-2 接口收口补充（2026-10-24）

- Oran7Track 从 PlaybackState 头文件拆为独立值类型；三个 Client 曲目信号均改为单快照参数，QML 选择改用具名字段对象，内部转换为强类型快照。列表模型本身仍沿用原字段，不提前实施 P1-3。
- 公开方法统一 lowerCamelCase，配置启动/退出收敛到 restoreConfiguration/saveConfiguration；辅助方法私有化，删除无调用的兼容 setter 和冗余转发。rememberLastMedia 改为 persistCurrentMusic，避免再次提交同一份元数据。
- 排序写入使用 setLocalMusicOrder；Client 信号仅通知，Controller 内部命令直接连接控制器。旧 TCP 协议不开发、不启用，只做边界值适配。
- 本地、搜索、漫游、底栏和视频调用方同步迁移。Release 全量及最终增量构建通过；30 项 QML 边界检查、原焦点回归、播放状态/调度器、53 项播放器生命周期和 47 项配置/元数据/默认关闭 TCP 检查通过，记录见 build/p1-public-api-result.txt。本批不表示 P1-3～P1-7 已完成。主程序运行体验由用户验收。


### P1-3 列表模型统一（代码完成，2026-10-24）

新增 Oran7TrackListModel（QAbstractListModel）与 Oran7TrackLibrary。Client unique_ptr 独占集合，集合以 unique_ptr 独占 local/integrated/liked/search 四个模型，均不设置 QObject parent；QML 只借用 CppOwnership 对象，不新增全局单例。

- 统一标准 roles：trackId/url/title/artist/album/cover/durationMs/isVip/isLiked/isIntegrated/path，并提供 songId/source、多选/动画状态；原 icon/music_name/music_artist/music_album/timesize/filepath/isLoved 为只读别名，避免本批同时重写整个大型视图。
- LocalLibraryService 以单次 tracksReady 快照发布扫描/导入/删除结果。搜索、收藏回调也整批提交，不再逐条跨语言 append；QML 不持有上述四张 ListModel。主栈窗口不再中转列表信号，页面重新创建仍读取同一份 C++ 数据。
- C++ 维护 ID/URL/路径去重索引，文件 URL 与 Windows 路径按相同规则归一；批内/跨批判重、VIP 空路径身份、批量顶部插入顺序均统一。喜欢/整合变化通过 dataChanged 同步其他列表的成员标记，不在每个 delegate 遍历其他列表。
- 模型元数据只读，无通用 QML append/setProperty/remove。界面通过 setSelected/setAnimationPending/moveTrack 表达操作；元数据批量更新、喜欢与整合增删通过集合命令提交。多选状态在同身份重建时保留。
- 本地模型内容/顺序变化直接同步曲库顺序，不再经 QML 回传路径数组；整合列表变化自动沿用 integratedMusic.list 格式落盘，启动恢复不反写。选择/动画变化不触发保存。
- 本地删除等待文件操作完成后发布实际快照，回收站失败项不提前从界面消失；导入、删除或同数量重建后按身份恢复播放/选中索引。拖拽使用 beginMoveRows，持久索引与角色状态随行移动。
- 移除 Client 的 localMusicTrackReady/integratedMusicTrackReady/localMusicListCleared 和曲库旧逐条信号，以及已无调用的 Client 列表保存/加载/排序转发。BasicConfig 保留薄调用转发与网络事件，不再保存列表数据或去重哈希。

旧网络类中未被现行链路消费的 likedSongReady/searchResultReady 已随 P1-4 的废弃网络实现一起清理，不另造第二套批量 API。陈旧 TCP 协议保持默认停用，仅在 Client 边界适配到集合，不开发服务端。

验证：build/p1-track-model-checks 的 82 项检查通过，含 Qt QAbstractItemModelTester、真实 ListView、生产 BasicConfig 入口、配置往返、2000 首单次批量通知、成员标记、身份/顺序/选择及 C++/QML 所有权。原曲库测试已迁移至批量信号和真实模型，覆盖扫描期间拖拽、失败删除保留、过期任务与生命周期；焦点重入回归通过。六个修改的 QML 文件语法通过。Release 构建通过，最终增量及验收记录见 build/p1-track-model-result.txt；主程序由用户启动验收。

#### 收藏加载记录收口补充（2026-10-24）

启动预加载、登录态同步、页签进入及云端排序失败共用 BasicConfig 的唯一加载入口。移除搜索页私有 loaded/时间戳；全局记录最近一次成功完成时间，空歌单也视为已加载。成功缓存 30 秒内且未置脏时不重复请求，加载中不并发。刷新保留原模型，失败置脏供重试；拉取期间新增的脏标记不会被成功回调抹掉。登录/凭据变化清理缓存并递增请求代际，旧账号/登出后的迟到结果被拒绝。该补充先统一 QML 全局策略，不代表 P1-4/P1-7 的网络协调全面下沉已完成。

验证：build/liked-cache-checks.cjs 使用生产函数覆盖预加载后首次进入、加载中进入、30 秒边界、空列表、失败/强制重试、加载中置脏、账号切换/登出旧回调及完成通知重入。82 项真实模型/BasicConfig 检查与两个修改 QML 的语法检查通过；Release 最终增量构建通过。未启动主程序。

### P1-4 网易云接入统一

1. `Oran7NcmTrackMeta`（唯一 DTO）+ `Oran7NcmJsonMapper`（唯一解析器，`fee==1` → `isVip`）。
2. 请求加 `requestId`：`NcmRequest{ id, tag }`，回调带 id，删除 `m_lastLikeSongId` 这类 last-state。
3. 播放和普通查询统一为 `urlsResolved(requestId, map)`，删除无关联号广播与临时播放专用信号。
4. 已删除死链路：`HttpClient`、`NcmAuthApi`、`QrLoginController`、`Oran7MusicSearchService` 及 Playlist 中旧拉取/漫游实现；保留实际在用的浏览器登录。
5. 日志脱敏（见 P2-2）。

**2026-10-24 实施结果：代码完成，运行待验收。**

- 新增 `Oran7NcmTrackMeta` / `Oran7NcmJsonMapper`，搜索、喜欢、漫游与启动详情补全共享解析；统一输出 title/artist/album/cover/durationMs，fee==1 为 VIP。列表保留毫秒精度，配置兼容秒字符串。
- `NcmRequest::nextId()` 原子分配请求 ID，传输回调携带 ID/tag；Song、User、Playlist 各自登记并消费请求一次。点赞保存每次歌曲/动作/凭据快照，排序只接受当前请求与当前歌单的结果；账号切换丢弃旧凭据结果。
- Resolver 每次 HTTP 尝试使用同一 ID 分配器，保留外链先播、20 分钟缓存、3 秒一次解析重试与打开失败补救预算。Session 校验与浏览器账号读取也按各自 ID 接收。
- Session 六个子对象统一 unique_ptr，无 QObject parent；按依赖逆序释放，QML 只借用 CppOwnership 对象。删除旧 QML 注册、二维码 provider 与无用 qrcodegen 编译项，第三方源码保留。
- curl 临时文件改 RAII，启动失败有且仅有一次结束回调；销毁终止剩余进程。修正阻塞 curl 的毫秒/秒超时单位，禁止响应体、Cookie、token 与原始 stderr 进入该链路日志。
- 有效空喜欢列表成功发布；缺少 trackIds 或全部详情失败报错，避免误清空旧缓存。部分详情缺失仍沿用原跳过策略。

**验证**：Qt 6.7.3 / MinGW Release 构建通过；独立检查覆盖 DTO/请求/API/实际 handler、真实系统 curl 的本机传输、Resolver、门面和 QML 列表。主程序不由代理启动。用户需回归登录/退出、搜索、喜欢增删/排序、漫游、VIP/非 VIP 播放与快速切歌。

检查结果：95 项网易云检查、20 项真实 curl 本机检查、30 项 Resolver、7 项请求关联回归、47 项门面及 83 项列表检查通过；共享喜欢缓存、播放焦点、漫游 DTO 的 Node 回归通过，3 个修改的 QML 通过 Qt 6.7.3 语法解析。完整记录位于 `build/p1-ncm-result.md`。

**范围**：搜索与喜欢加载协调、TTL 和漫游队列仍在 QML，留待 P1-7；本批只保证排序结果归属，不串行化云端写请求。凭据加密及全项目日志治理仍属 P2。P1-9 启动窗口仅保留计划。

### P1-5 配置结构化

```cpp
struct Oran7AppConfig {
    static constexpr int kVersion = 2;
    Oran7WindowConfig window; Oran7AppearanceConfig appearance; Oran7PlayerConfig player;
    Oran7ScreenCaptureConfig capture; Oran7AuthConfig auth; Oran7LocalLibraryConfig library;
    QJsonObject toJson() const;
    static Oran7AppConfig fromJson(const QJsonObject& normalized);
    static bool normalize(const QJsonObject&, const QJsonObject& defaults, QJsonObject*, QString* error);
};
```

- 单一 `default.json` 深合并，调用点不再传默认值。
- 键名常量集中在 `Oran7ConfigKeys.h`。
- 凭据拆到独立加密文件属于 P2-3，本批保留原格式。

**2026-10-24 实施结果：代码完成，运行待验收。**

- 新建 `src/cpp/Oran7Config/`，提供 Oran7AppConfig 及窗口/主题/播放器/抓屏/凭据/曲库/最近曲目分组与 Oran7ConfigKeys。纯配置实现只进 Oran7Core，EXE/插件共享 Manager，不新增单例。
- 业务调用改用 snapshot/updateConfig；Manager 的归一化 JSON 是唯一存储。默认值只来自 default.json，递归补键/修复类型与范围，保留未知键、两套凭据、旧歌曲行和排序格式。版本缺失/v1 幂等升级 schemaVersion=2。
- 首次访问 Manager 即加载，后续 loadConfig 不重复读盘；删除整合列表保存前的 reload，修复待保存音量被旧磁盘值覆盖的问题。
- QSaveFile 原子保存，首次覆盖已有文件前备份原字节；损坏文件恢复前也必须备份。较新/非法版本或不可读原件禁止覆盖；失败保留脏数据供重试。窗口负坐标可保存，空本地顺序也可持久化。
- 运行验收重点：旧配置首次启动、音量/窗口/主题重启恢复、整合列表及排序、登录状态、抓屏设置；不启动主程序代验收。P1-6/7 与 P1-9 均未提前实施。

**验证**：Qt 6.7.3 / MinGW Release 全量及最终增量构建通过，EXE、Oran7Core 和 QML 插件均完成链接。78 项真实配置/迁移/备份/原子保存/ConfigBinding 检查与 47 项门面回归通过；使用临时文件和无窗口 QML 对象，不读写用户配置、不访问网络、不启动主程序。记录见 `build/p1-config-result.md`。

### P1-6 录屏异步化

1. `startRecording()` 改为"提交请求 + 信号回状态"（`Oran7RecordingState`：`Idle/Preparing/Recording/Stopping/Error`），GUI 零等待。
2. 录制参数（输出目录/文件名/分辨率/码率/WHIP URL/凭据）进配置 + 设置 UI。
3. 已决议保留 WHIP/libdatachannel 客户端：启用录制/推流模式选择，配置接口地址/凭据，展示连接状态、ICE、实际码率和发送帧数；房间信令为可选辅助功能。
4. 已删除固定 H264 dump 与个人录像路径，MP4 使用配置目录/系统视频目录与唯一文件名，不覆盖旧录像。

**2026-10-24 实施结果：代码完成，运行待验收。**

- Controller unique_ptr 持有 Oran7RecordingSession；GUI 只提交请求，独立任务线程执行 Oran7RecordingBackend。Oran7RecordingState 统一 Idle/Preparing/Recording/Stopping/Error；准备和停止均算忙，旧代际、取消和重入结果隔离。
- Oran7RecordingFeed 通过 mutex 共享首帧尺寸、带引用计数的硬件设备、编码入口和 epoch；任务不访问采集 QObject。独立 Oran7WasapiAudioWorker 接管原音频采集实现。
- Oran7RecordingConfig 与 recording 默认段接入 P1-5；聚合设置面板提供模式、输出目录/名前缀、尺寸、帧率上限、码率、音频与 WHIP/ICE/可选信令凭据。数值/路径/URL/类型校验后一次保存，忙状态拒绝修改。
- 任务用同一路径关入口、停桥接/采集、flush 编码、关闭 MP4/WHIP；修复打开失败未检查、失败线程未 join、迟到帧、文件句柄泄漏及固定 dump。准备失败的临时输出自动清理，已提交录像保留。
- WHIP 回调改为共享事件值，gathering 不捕获栈变量；ICE 候选由发布线程串行 PATCH，Bearer 来自配置并用于 POST/PATCH/DELETE，Location 限制同源，连接就绪后才宣布 Recording。原 WS 信令停止去掉 GUI 局部事件循环。

**边界**：录制启停不在 GUI 轮询、等待 HTTP 或 join。预览启停、显示器枚举、抓屏参数应用/回滚与退出同步等待沿用原流程，不能把本批表述为整个抓屏模块零等待。WHIP 默认不连接服务；用户的陈旧 TCP/Server 继续停用。真实 D3D11/h264_mf、WASAPI 和外部推流服务留待用户验收。

**验证**：Qt 6.7.3 / MinGW Release 全量及最终增量构建通过（含运行库复制）。28 项会话生命周期、50 项替身设备下的 backend/资源清理、155 项真实软件编码与 MP4 回读、10 项离屏 QML 面板、9 项本机 WHIP HTTP/凭据检查通过；79 项配置与 34 项录屏桥回归通过，两个 QML 文件语法解析通过。未启动主程序、未采集桌面/系统声音、未连接实际推流服务；完整记录见 `build/p1-recording-result.md`。

**2026-10-25 录屏页补充调整（按用户最新范围）：**

- WHIP 暂停后续改造，保留现有实现、配置及原设置弹窗源码；本轮聚焦本地录屏链路。
- 主 GUI 新增 Oran7RecordingWorkspace：左来源、右输出，中栏上方预览、下方独立 Oran7RecordingToolbar（音频/启停/状态），工具栏与预览同宽，先定布局、后续再美化；窄屏保持左右工具栏位置，宽度不足时横向滚动，复用既有 SettingItem/数字组/开关组件。
- 全局下拉设置移除旧录屏面板；Oran7RecordingPageState 只管理参数草稿，控制器继续独占运行状态。采集参数应用后再开始，输出参数可保存后开始；录制和保存期间锁定设置，保留 WHIP 相关配置。
- applyCaptureSettings 返回 pending，修复同步成功无完成信号时的界面等待；渲染挂载失败时先重试，成功后再启动预览。
- 验证：Qt 6.7.3 / MinGW Release 全量与最终增量构建通过；26 项离屏布局/控件/页面挂载与 20 项设置/启停流程检查通过，6 个 QML 文件语法解析通过。实际设备操作仍由用户验收，记录见 `build/recording-page-result.md`。未提前实施 P1-7 或 P1-9。

### P1-7 QML 业务下沉

**2026-10-25 实施结果：代码完成，运行待验收。**

- Client 以 unique_ptr 独占 Oran7PlaybackViewModel 和 Oran7CloudMusicService，无 QObject parent，显式 CppOwnership；仅 EXE 编译，不新增全局单例。ViewModel 管理播放意图/归属/队列，PlaybackState 继续独占播放事实。
- 两页播放按钮/双击统一调用 Client.musicQueue.activate(model,index)，底栏播放按钮调用 toggleCurrent()；自动下一首、上一首及解析失败由 C++ 处理。归属使用模型借用指针和稳定 trackId，每次切歌重新定位，排序/删除后不沿用旧下标。自动推进和单曲循环明确重播，点击当前曲目仍保持暂停/继续；启动恢复或从视频经底栏切回音乐时可按身份找回归属，视频和退出取消旧命令。
- 漫游队列、半队列预取、上一首/队首重播及等待新批次下沉；取消、模式退出、账号变化使旧结果失效。预取失败不打断当前歌曲，耗尽后停止；连续解析失败有上限，避免无限漫游。
- 收藏 30 秒成功缓存/并发去重/脏标记/账号代际由 CloudMusicService 持有；列表和底栏共用 2 秒点赞冷却。拉取期间点赞不会被旧快照覆盖，云端写失败触发重新对账；后台只捕获凭据值与无 QObject 的 handler，沿用 Oran7Async Network 组。
- Oran7MusicPlaylistView 从 2242 行拆为 559 行门面，分离 Oran7PlaylistHeader、Oran7PlaylistDelegate、Oran7PlaylistSelection、Oran7PlaylistColumns、Oran7PlaylistReorderController。实际功能是拖拽排序，因此拆出排序控制器，不凭空增加 SortMenu；布局与现有交互保持原样。
- 删除未使用的 SearchPopup.qml、qrc 项和旧注释入口。BasicConfig 保留只读投影和少量兼容转发，不再拥有收藏请求状态/TTL/漫游队列。搜索关键词及搜索结果请求代际仍由搜索页处理，未扩展到独立搜索模块。

验证：Qt 6.7.3 / MinGW Release 全量及最终增量构建通过；32 项 C++ 策略检查、14 项真实 QML 组件/两页加载检查、11 个 QML 文件语法解析及 qrc 引用核对通过。记录位于 `build/refactor-work/P1-7/result.md`。运行验收重点：播放按钮/双击聚焦，跨页签自动切歌，拖拽后上一首/下一首，单曲重播，VIP 失败推进，收藏/底栏点赞、退出登录、漫游预取期间切歌/停止/切视频及退出。未启动主程序、未访问实际网易云服务，P1-9 启动窗口仍未实施。

### P1-8 单一任务调度器

已合并到 Oran7Async::pool()，分 General/Network 两组。Network 组以两个排队执行器消费队列，不用等待信号量占住额外线程；通用任务共享同一个池，池线程数至少 4。C++ run/runDetached 返回 Oran7TaskHandle；QML 保留整数 taskId，并增加 cancel(taskId)/cancelAll()。取消会跳过未开始的 work，或抑制已运行任务的回调；运行中 curl 仍按既有超时返回。退出冻结提交、取消 QML 任务，再排空共享池。


### P1-9 启动加载窗口与主界面显示时序（转入 P3-1，首版完成）

2026-10-26：本项由 [[REFACTOR - P3阶段计划]] 的 P3-1 承接；2026-10-27 首版实现，启动效果待用户验收。下文保留原计划背景，不另立重复任务。

**提出时间**：2026-10-24。**优先级**：高（启动体验），建议在 P1-3 后作为独立小批次实施。本次仅登记方案，不修改启动代码或创建窗口。

**现状与问题（2026-10-26 核对）**：main.qml 的主 ApplicationWindow 初始 visible=true；main.cpp 先 engine.load，再调用 Client.restoreConfiguration。通用语言/字体偏好已在 QML 引擎创建前恢复，不能将问题笼统归因为所有配置加载过晚。用户观察到启动期间界面闪烁、布局逐步挂载。当前显示/配置应用顺序存在提前暴露中间状态的可能，实施时需记录配置、主题、布局与首帧时点定位实际原因，不把全部闪烁简单归因于资源加载速度。

**目标体验**：先显示轻量加载动画与中文阶段提示，再创建主界面、应用初始配置并完成必要布局；主界面具备可用首帧后平滑切换。前置窗口负责反馈真实启动状态，主界面显示时序也要修正，避免只增加动画而保留中间状态闪烁。

**拟定职责与所有权**：

- Oran7StartupCoordinator：启动入口持有的普通 C++ QObject，按 RAII 管理，不新增全局单例，不塞回 Oran7MediaClient。统一持有启动阶段、就绪条件、错误与显示交接状态，QML 只读绑定并发送操作意图。
- Oran7StartupWindow.qml：轻量前置加载窗口，使用内嵌静态资源和简单动画，避免背景视频、网络图片或完整业务页成为其前置依赖；新增文件显式加入 qml.qrc。显示“读取配置 / 加载界面 / 准备显示”等实际阶段，不展示虚构百分比。
- 主窗口：准备期间不暴露不完整布局；必要时使用覆盖主窗口的启动层保护首次曝光，再与前置窗口交接。窗口形态、动画与短过渡在实施时结合透明窗、DWM 与 D3D11 验证，不强制增加固定秒数等待。

**实施步骤与就绪条件**：

1. 梳理并拆分配置值读取与窗口属性应用：能影响首屏的主题、字号、窗口尺寸/位置/状态等在主界面正式展示前落地，避免先显示默认几何/主题再跳变；异常配置使用原有默认值恢复路径。
2. 尽早显示轻量启动窗口并使事件循环能够推进，再分阶段创建主 QML。评估 QQmlComponent 异步加载/Loader 孵化，耗时 I/O 放现有异步设施；QObject/QQuickItem 创建及窗口操作仍在 GUI 线程，避免同步等待、sleep 或手动反复 processEvents 阻塞动画与制造重入。
3. 由协调器汇总“主根对象创建成功、关键组件/初始页面就绪、配置应用完成、必要布局稳定、有效首帧确认”。Component.onCompleted 或单次 Qt.callLater 只能作为部分证据，不单独等同于可显示。
4. 验证 Qt 6.7.3 / D3D11 下窗口曝光和首帧通知的先后关系：不能让主窗口永远隐藏却等待它提交首帧。需要曝光时先以完整启动覆盖层保护主窗口，确认展示的是应用配置后的可用主界面帧，再撤层/关闭前置窗口；交接只执行一次。
5. 首屏必需的是模型实例、可用占位布局及所选背景就绪；背景视频等待有效首帧，失败/5 秒超时固定回退。全量本地曲库扫描、在线收藏/搜索、网络连通性或播放媒体解码不参与等待。后台加载继续沿用批量模型通知，离线或曲库很大也能正常进入主界面。
6. 资源/主 QML 创建失败时给出明确错误并允许退出；对就绪超时提供诊断和明确处理，避免永久转圈或盲目放行空白主窗。启动期间关闭窗口会失效延迟回调、取消可取消任务，并走已有安全退出流程。

**必须保留的工程约束**：

- 保持 Direct3D11、basic 渲染循环、透明窗口、Qt Multimedia windows 后端等既有配置；不以切换后端规避闪烁。
- 尽量沿用同一 QQmlApplicationEngine，避免双引擎重复创建 BasicConfig 等 QML 单例；核心共享单例保持唯一。引入额外根窗口后，配置/退出代码必须显式定位主窗口，不能继续默认 engine.rootObjects().first() 就是主窗，防止把几何配置应用到启动窗口。
- 分别记录启动窗口首帧、主 QML 创建、配置完成、布局就绪、主窗口首帧和交接耗时，用实测判断瓶颈；不打印认证凭据。

**验收标准**：

- [ ] 冷启动/热启动都先有可响应的启动反馈，主界面出现时无明显白闪、默认主题跳变、窗口尺寸跳变或未完成布局外露。
- [ ] 正常、最大化、不同 DPI/多显示器、背景视频开/关、深浅主题均验证恢复位置和显示交接；不破坏透明/DWM 效果或抢焦点。
- [ ] 断网、收藏请求超时、大曲库扫描期间仍能进入主界面，不以等待网络换取首屏稳定。
- [ ] 加载失败、超时和用户提前退出不会无限等待、崩溃、重复显示主窗口或遗留前置窗口；重复就绪通知不会重复交接。
- [ ] 对比改造前后的关键启动耗时与画面表现，不通过固定延迟掩盖问题；只编译 Release，主程序运行验收由用户执行。

---

## 4. P2：工程卫生与剩余专项

**2026-10-25 按 P1-7 完成后的源码重新整理，2026-10-26 持续补充至 P2-7 实施结果。** 下表的“部分完成”只表示部分子项已随 P0/P1 落地，不代表整个 P2 编号完成；代码/检查通过与用户运行验收分别记录。

| 编号 | 当前状态 | 已完成或已有基础 | 剩余重点 |
|---|---|---|---|
| P2-1 | 已实施，待主程序运行验收 | P1-4/P1-7 清理基础；本批完成剩余候选审计、11 个死文件及关联声明/路由/资源清理 | 用户验证窗口、设置、播放及录屏入口 |
| P2-2 | 已实施，待主程序运行验收 | 统一日志出口、有界队列、单写线程、日期/大小滚动、源头审计与统一脱敏、退出排空 | 用户验证控制台/文件、播放录屏与退出体验 |
| P2-3 | 已实施，待主程序运行验收 | 当前用户 DPAPI、分组不可变密文、先回读验证后提交引用、旧配置迁移/退出/失败恢复、打包排除 | 用户验证真实登录、重启恢复与设置保存 |
| P2-4 | 已实施，待主程序运行验收 | 可配置依赖/preset、QML 类型唯一编译、24 项 CTest、格式配置 | 用户验证主程序及发布包启动；设备专项另行验收 |
| P2-5 | 实施完成，待运行验收 | 保留共享实例；token 类型、显式分组通知、深色单源与 profile v2 差量已实施 | 用户验证深浅色、局部编辑和重启恢复 |
| P2-6 | 中英实施完成，待运行验收 | Qt 翻译资源、即时切换/持久化、通用字体与 fallback、设置入口 | 用户验证真实字体/DPI/重启恢复 |
| P2-7 | 代码实施完成，待真实设备验收 | QRhi 单一帧消费/分阶段更新、多 pass 独立资源、采集租约与 keyed mutex、完整信息快照、WARP 离线检查 | 用户验证真实播放/Seek/缩放、切屏、连续录制及退出；HDR 后续独立实施 |

### P2-1 剩余死代码与资源清理

**不再重复安排**：旧 NCM HttpClient/API 扫码/旧搜索服务、旧歌单读取/漫游实现及 SearchPopup 的删除；当前 NCM handler 与浏览器登录仍在使用。

**2026-10-26 实施结果**：已核查下列候选，删除无依赖对象并同步清理资源与路由；正式导出的 Oran7Api、现行录制转换及确有调用的 QML 组件保留。具体删除依据、保留清单与边界见 [[REFACTOR - P2-1清理记录]]。

**本批核查清单（已完成）**：

- `FramelessWindow.*`、`Oran7WindowApi.*`、`Oran7Api.*`：同时检查 C++ 调用、QML 导出/注册、qrc 和 CMake；不能只凭某一层没有直接调用就删除。
- `ImageScaler.h`：`Oran7FFmpegDecoderCore.h` 仍有前向声明和指针成员，先确认实际初始化/使用链，再决定删除类、清理残留成员或保留功能。
- 抓屏旧转换辅助函数 `blitBgraToRgbaShared/ensureSwizzlePipeline/ensureEncodeNv12Tex/vpBgraToNv12Shared`：区分现行编码/预览路径与备用路径，核实无依赖后单独处理，不改现行录屏行为。
- `SettingStack.qml`、`Oran7MediaPlayerSetting.qml`、`*Example.qml`、未进入资源的文件：先核对页面路由、动态创建、资源和文档引用；重新生成实际清单，不沿用旧“两个文件”等数量结论。

**边界**：自定义 TCP/Server 继续列为陈旧未启用模块，保留源码且不启动；WHIP 按用户要求暂缓，仅维护必要组织关系，不列为顺手删除/改造对象。不删除 SQLite 数据、用户配置或历史录像。

**验收**：删除对象的调用/注册/资源/构建引用同步清理，Release 通过；保留清单说明仍在用或刻意保留的对象，用户验证对应入口。

**本批验证**：Release 与检查工程构建通过，24/24 项正式 CTest 通过；Release 再次增量无待编译内容，资源和残留引用核查通过。未启动主程序，运行验收仍由用户完成。

### P2-2 日志输出与脱敏收尾

**2026-10-26 实施结果**：Oran7LogSink 由 Oran7Core 唯一编译，保留原日志宏，接管 Qt/QML/FFmpeg/WebRTC；默认 2048 条队列、8 MiB 分段、10 文件保留，文件失败降级、正常退出排空。修正共享服务的 glob 排除逻辑，避免 EXE 再持一份日志服务。源码审计、运行目录、满队列策略和离线检查见 [[LOGGING - 日志与脱敏]]。凭据加密仍属于 P2-3。

- 保留既有日志级别和调用宏，统一模块前缀及 Qt 消息处理入口，避免再建一套并行日志系统。
- `Oran7GlobalHelper.h` 中的 Oran7Logger 原先按行调用 Oran7Async::runDetached；本批改为有容量上限的队列和单写线程，定义溢出策略、退出排空及失败降级，避免高频日志挤占业务任务池。
- 增加文件输出、按日期/大小滚动及保留上限；文件不可写时不能阻塞播放或递归触发日志。
- 审计 Cookie、Authorization/Bearer、URL 查询凭据、账号密码和响应体的所有输出入口；使用统一脱敏规则，不把部分 NCM 链路已收敛等同于全项目已完成。

**验收**：用虚构敏感值验证控制台和文件均不泄漏；并发/高频输出、队列满、不可写目录和退出有定向检查。正常退出可完成既定排空，崩溃 dump 与普通日志的职责分别保留。

**本批验证**：Release 构建通过且再次增量无待编译内容；25/25 项离线 CTest 通过，新增 logging 检查含 89 项断言；编译数据库确认日志服务仅由 Oran7Core 持有一份。未启动主程序，实际运行仍待用户验收。

### P2-3 凭据加密与迁移

**2026-10-26 实施结果**：Oran7CredentialStore 仅在 Oran7Core 中实现当前用户 DPAPI；Oran7DpapiCookiePersistence、B 站、录制配置与旧配置残留认证字段统一接入。普通配置仅保存已验证密文引用，迁移在下次启动时执行；密文损坏不回退旧明文，重新登录按组恢复，退出提交空代际。文件/版本/失败恢复、历史备份范围和打包排除见 [[CREDENTIALS - 凭据加密与迁移]]。

- 先列出网易云、B 站及录制配置中实际存储的敏感字段，区分普通参数与凭据；先接现有 CookiePersistence，再分批覆盖其他凭据入口，WHIP 功能本身仍保持冻结。
- 新存储实现统一使用 Oran7 前缀，例如 Oran7DpapiCookiePersistence；Windows 当前用户范围使用 CryptProtectData/CryptUnprotectData，密文存储格式、版本及路径在实施时明确。
- 从旧配置迁移时先写密文并验证可回读，再原子提交普通配置中的凭据移除；失败保留可恢复原件，不生成半迁移状态。历史明文备份先列出处理范围，不自动销毁用户文件。
- 明确首次启动、退出登录、损坏密文和不同 Windows 账户下的恢复行为；不得把解密后的值写回普通配置、日志或打包产物。

**验收**：使用临时目录/虚构凭据验证旧格式迁移、重复迁移、保存中断、解密失败及清除操作；实际登录由用户验证。P1-5 的 schema/备份/QSaveFile 是迁移基础，不计为本项加密完成。

**本批验证**：26/26 项离线 CTest 通过，新增 credentials 检查含 101 项断言；Release 构建通过，未启动主程序、未迁移用户真实凭据、未执行打包或删除历史备份。

### P2-4 构建收敛与验证正式化

**2026-10-26 实施收尾**：已提供 release/checks preset、依赖 cache 和编译器工具链，QML 导出类型收敛到插件单独编译；正式检查迁至 tests，经可选 BUILD_TESTING/CTest 提供 24 个入口。加入 .clang-format，未全库格式化。真实插件检查补齐离屏字体条件，并修正 Oran7Api/Oran7ThemeFunctions 的 C++ 单例所有权，避免引擎销毁时误回收。构建步骤、隔离范围及历史检查取舍见 [[BUILD - 构建与验证]]。主程序/真实设备/发布包启动继续由用户验收，不计为自动通过。

- Qt、MinGW、OpenSSL、zlib、FFmpeg 等路径改为有现行默认值的 CMake cache/preset 配置；保留 Qt 6.7.3 MinGW 和现有 Release 构建/打包流程，不切回损坏的 6.7.2 kit。
- 核查 QtQuicCppExtend 同时编进 EXE/插件的文件，按共享实例和 QML 元对象注册要求确定唯一编译归属；可评估静态目标或显式源码清单，不把“全部改 glob”作为目标。
- 将可复用的独立检查整理到工程内稳定目录，经可选 BUILD_TESTING/CTest 提供统一入口；复用现有模型、状态、解析、配置、队列及异步检查，先补齐构建依赖，不重写重复测试。
- 纯逻辑补充覆盖可按 Oran7LrcParser、WeapiCrypto、主题表达式/生成器、Oran7NcmJsonMapper、弹幕解析逐批接入；普通构建不自动启动主程序、访问真实服务或使用用户凭据。
- 增加 `.clang-format`，先约定 C++ 风格，仅格式化触及文件，避免全库格式改动掩盖功能差异。临时迁移脚本集中在 build/refactor-work/<阶段>/，完成后清理一次性脚本。

**验收结果（2026-10-26）**：独立 checks Release 目录新配置/构建成功，24/24 项 CTest 通过；现有主程序 Release 构建成功，再次增量构建显示 `ninja: no work to do`。编译数据库确认导出类型仅进入 QML 插件，主程序/共享库/插件及 FFmpeg、zlib 产物存在，静态导入检查通过。未启动主程序或执行打包，真实设备与发布包启动仍待用户验收。检查日志与编译归属记录位于 `build/refactor-work/P2-4/`。

### P2-5 主题数据与通知机制（实施完成，待用户运行验收）

- 保留已经完成的主题共享实例，不重复实施 P0 的单实例化。
- 区分表达式字符串和普通 bool/int/real/color/string token，避免解析时统一转字符串丢失类型。
- 评估类型化变更信号与 revision 通知的最小兼容方案，替换脆弱的字符串反射更新；不是简单增加 revision 属性后就算完成。
- 统一深色模式权威来源，梳理主题对象、组件 token 与设置界面的关系；保留系统跟随/手动切换的明确语义。
- 源 JSON 提供默认值，profile 只保存用户覆盖；旧 profile 有兼容迁移，缺省新增 token 能从默认值获得。

**验收**：类型往返、表达式错误回退、动态切换/重启恢复、旧 profile、新 token 默认值及共享实例检查通过。用户验证深浅色、组件局部修改、配置恢复；本批不混入布局或组件美化。

**2026-10-26 实施记录**：

- [x] 新增优化项：在临时 Map 计算后比较键、值及类型，只替换并通知实际变化的分组；全局重算不等于全组广播，组内仍为 Map 粒度。
- [x] Oran7ThemeEvaluator 保留原生类型，按依赖解析引用/生成器，错误回退及循环/深度保护；普通字符串保留空白。
- [x] 自有分组用显式信号回调替代拼接反射，全部快照提交后通知；删除无人使用的旧 custom-theme 入口与数据表。
- [x] darkMode 唯一权威，Qt 系统配色跟随；QML 直绑 isDark，旧组件布尔只读派生兼容。
- [x] profileVersion=2 差量，旧全量先备份再原子迁移，保留未知差异；损坏/未来版本禁止覆盖。修复全局覆盖首次计算顺序、跨 profile 残留、圆角函数拼写及动画参数未写回。
- [x] 新增 theme 离线检查，复用实际插件与离屏 QML，覆盖类型/表达式/通知/迁移/重载/新默认值。验证记录位于 build/refactor-work/P2-5/。
- [ ] 用户运行验收：深浅色、组件编辑、切换/删除 profile、重启恢复；不代跑主程序。

### P2-6 文案国际化与字体收敛（中英实施完成，待用户运行验收）

- 梳理主程序/QML 的用户文案，分批接入 qsTr/翻译资源及加载入口；默认界面和代码注释保持中文，注释不属于待翻译内容。
- 字体族、基础字号与 fallback 收入统一主题/设置入口；保留对现有配置的兼容，不直接全局替换字体名。
- 清理重复硬编码的“微软雅黑”“华文琥珀”等引用，验证中文、英文、数字、缺字及不同 DPI 下的显示。

**验收**：中文默认效果稳定、翻译资源可加载、缺少翻译/字体时可回退，控件没有新增截断/布局跳变。具体视觉美化仍按用户另行安排。

**2026-10-26 实施记录**：

- [x] 仅中文/English，默认中文；下拉 GUI 的主界面设置顶部选择语言，自动保存、立即 retranslate，启动在 QML 创建前恢复。
- [x] Oran7UiPreferences 由 main 拥有，仅 Core 编译，借用配置/引擎；TS 经 lrelease 编译并嵌入 Core，不增加业务单例或外置运行依赖。
- [x] 本地 QML 文案接入共同翻译上下文；导航、监视器标签、动态状态在显示层翻译，保持模式键/页名/元数据/路径和录制草稿不变。285 条英文译文。
- [x] appearance 新增语言/字体/字号，旧配置自动补默认；保存失败回滚，中文源与缺失字体有 fallback。通用字体集中于应用入口，艺术字体用主题角色，保留组件专用字号。
- [x] localization 离线检查覆盖目录/占位符/静态 QML 翻译、真实语言选择框、两种语言截图、重启恢复/失败回滚、字体 fallback、录制草稿与显示器标签动态切换。
- [ ] 用户验收真实 GUI 的字体、DPI、窄屏及中英切换/恢复。日志、注释、原生系统对话框和外部诊断原文不在本地 UI 翻译范围。

详见 [[LOCALIZATION - 界面语言与字体]]；验证材料位于 build/refactor-work/P2-6/。

2026-10-26 后续 UI 调整：语言、字体、基础字号进入独立折叠组；字号复用已有滑块，选择弹层统一样式/动效与字体预览。收起关闭弹层，ESC 优先关闭菜单。补充验证位于 build/refactor-work/P2-6-ui/。

### P2-7 渲染驱动与采集同步专项（代码实施完成，待真实设备验收）

用户于 2026-10-26 确认：统一路径走 QRhi，为后续 HDR 留出扩展基础；本批不启用 HDR，不扩展 WHIP。

- [x] 保留 RenderBridge/播放器/录制会话职责和原 QML 接口；Item 删除重复 GPU 转换与两路消费，prepare() 为唯一消费点。beforeRendering 只推进帧编号，多 pass 共用同一准备结果。
- [x] QRhi 创建/导入/资源更新移至 prepare；render 只提交绘制。独立 pass uniform/SRB/pipeline、兼容 descriptor/sampleCount、标准矩阵、scissor/stencil 与透明度；显式 float uniform 修复 QMatrix4x4 内部布局差异。
- [x] 三槽池由 AVFrame 租约保护复用；keyed mutex 保证跨设备 GPU 读写顺序，零超时忙槽只丢预览，不跳过原始帧录制。取消/退出不等待共享纹理锁，异常沿用有限采集恢复。
- [x] worker 只持共享邮箱提交回调，不跨线程访问 Item/Controller；完整视频信息按代际经邮箱发布，源尺寸/Qt 信号在 GUI 线程更新。
- [x] 设备更换宿主断开旧信号，同宿主场景图失效后保留重建订阅。
- [x] 新增生产节点 + 两个 WARP 软件设备的离线像素检查，覆盖连续帧、代际/租约、跨设备同步、透明度/Fit/裁剪、主/离屏多 pass 及 pass 间注入新帧、重挂载和设备订阅；不启动主程序或抓真实桌面。

实现与边界见 [[modules/Render - D3D11视频渲染]]，验证材料位于 build/refactor-work/P2-7。

**验证结果**：Release 编译链接及依赖复制成功，30/30 项 CTest 通过（80.51 秒）；最终渲染定向检查复测通过。未启动主程序，真实设备运行验收仍待用户执行。

**运行验收待用户执行**：真实 D3D11 播放、Seek、缩放、页面往返、显示器/分辨率切换、预览、连续录制及退出，记录黑屏、撕裂、卡顿与资源释放情况。保持 Direct3D11、basic 渲染循环及透明窗口约束；现行输出仍为 BGRA8 SDR，HDR/PQ/HLG/高精度输出作为后续功能，不将软件检查当作真实设备性能结论。

---

## 5. P3：启动流程与采集生命周期（P3-1 首版与 P3-2/3 已实施）

完整计划见 [[REFACTOR - P3阶段计划]]。2026-10-26 先完成 P3-3，2026-10-27 完成 P3-2 与 P3-1 启动窗口首版及离线验证。

| 编号 | 范围 | 边界 |
|---|---|---|
| P3-1 | 启动协调、加载反馈、配置/布局/首帧就绪后交接 | 承接 P1-9，不等待全量曲库或网络，不改变 D3D11/basic/透明窗口 |
| P3-2 | 预览启停、参数重启、回滚与恢复异步化 | 普通交互不在 GUI wait；退出保留安全 join，录制状态仍由 RecordingSession 持有 |
| P3-3 | 采集 worker、图初始化与转换资源分离 | 保留 P2-7 QRhi/租约/同步，结构移动与画质/编码算法变更分批 |

**当前进度**：P3-3 已提取 Oran7DesktopCaptureWorker 与其独占的 Oran7CaptureFrameConverter，过滤图/设备/临时帧使用 RAII，控制器不持转换资源；保留录制与 QRhi 同步路径。Release 构建及完整 31 项离线检查通过，真实设备待验收。P3-2 已统一预览状态和请求代际，普通停止/重启仅提交取消，线程完全结束后再重建；首帧确认后保存新参数，失败恢复旧配置。阻塞 join 仅留退出，旧无编号接口保持兼容，工作台按请求编号过滤迟到结果。最新完整 CTest 31/31（80.68 秒）与最终定向复验通过。同步显示器枚举仍保留。

**P3-1 首版**：入口独占 Oran7StartupCoordinator；轻量启动窗口先提交一帧，随后初始化服务、异步预编译主 QML、隐藏恢复配置；页面布局与受保护首帧就绪后，撤下遮罩，实际页面再呈现一帧才关闭启动窗口。配置按名称找主窗，提前退出不提交部分窗口状态。Release 与完整 32 项检查通过，随后补充最终背景门控：取消无条件 DWM，视频在遮罩内等待有效首帧或 5 秒超时后锁定回退，避免逐层背景闪烁。实际 DWM/启动体验待用户验收，详见 [[Startup - 启动与首屏]]。

**后续顺序**：P3-1/2/3 真实启动与设备验收，再根据里程碑耗时评估进一步拆分同步初始化/孵化创建。HDR、音频功能与其他扩展不并入本次交付。

HDR、麦克风/混音属于后续功能，不列为 P3 重构欠项。WHIP 扩展、自定义 Server、播放器内核重写和 Qt/后端更换继续暂缓。

## 6. 不建议做的事

1. **不要重写播放内核**。`Oran7FFplayerCore` 是 ffplay 派生、能跑；要改的是它与 Qt 的边界（P1-1），不是解码逻辑。
2. **不要为了"MVVM 纯粹"立刻引入 QML 框架/第三方架构**。先把状态收成 `Q_PROPERTY` 就够了。
3. **不要在没有测试的情况下动 `FFPlay_Def`/`FFMsgQueue` 的同步逻辑**（锁序、serial、frame queue 索引）。这些是"能跑就行"的区域。
4. **不要同时改主题系统和 UI 布局**——主题改动的影响面被 profile 机制放大了。
5. **不要在 P0 阶段顺手做大重构**：P0 的价值是"立刻可验证"，混入结构改动会让回归无法归因。

## 7. 验收标准

每个阶段结束后应满足：

- [ ] 能编译 Release 且无新增 `QObject::connect: invalid nullptr` 告警。
- [ ] 启动 → 播本地歌 → 播网络歌（VIP/非 VIP）→ 播直播 + 弹幕 → 录屏 30s → 改主题 → 退出，全程无 minidump。
- [ ] 退出后 `config.json` 与 `profile-*.json` 内容符合预期，再次启动状态恢复。
- [ ] 抓屏设置应用/回滚都能在 UI 上看到结果。
- [ ] 新增/修改的状态项在 [[RUNTIME_STATE - 运行时状态]] 的所有权表里只有一行。
- [ ] 被删掉的死代码不再出现在 `grep` 结果里。
