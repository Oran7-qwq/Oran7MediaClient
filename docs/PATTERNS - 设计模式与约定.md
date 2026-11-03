---
status: ✅ 已核对
updated: 2026-10-24
---

# 设计模式与编码约定（PATTERNS）

> 本文回答：项目里实际用了哪些模式、为什么、在哪儿能看到范式，以及新增代码应该照哪个模板写。
> 目的不是"教科书对齐"，而是让后来人一眼看出某个文件属于哪种写法。

## 1. 实际使用的模式（按重要度）

### 1.1 单例（双层：进程单例 + 共享库单例）

| 形态 | 例子 | 注意 |
|---|---|---|
| `static T& instance()` + 私有构造 | `Oran7ApplicationContext`、`NcmSession`、`Oran7BilibiliAuthManager`、`Oran7NetworkMonitor`、`Oran7ScreenCaptureController`（无单例，由 Oran7MediaClient 持有） | 构造时机由 `main.cpp` 显式控制，析构靠 `aboutToQuit` + `cleanup()` |
| QML 单例注册（lambda 返回全局对象） | `main.cpp:192-253` 全部业务单例 | `QQmlEngine::CppOwnership` 必须设置，否则 QML 引擎会接管 delete |
| 编译期 QML 单例（`QML_SINGLETON`） | `Oran7Theme`、`Oran7ThemeFunctions`、`Oran7Api` | 只对 `import Oran7UI.Impl` 生效，且**只存在于插件那份** |
| 共享库单例（`Oran7Core`） | `Oran7Async`（头文件全 inline + .cpp 里持状态） | 跨 DLL 必须落到共享库，否则每个模块一份 |

历史问题：主题双实例已收敛；P2-1 已删除未收录、无调用的 Oran7WindowApi。新增导出类型仍必须显式加入 QML 模块。

### 1.2 门面（Facade）

- `Oran7MediaPlayer` 对 `Oran7FFplayerCore` 的门面：外部只见 `oran7mp_*` 一组 C 风格接口（`Oran7MediaPlayer.h:105-215`）。
- `Oran7MediaClient` 对 QML 的门面：QML 只认 `Oran7MediaClient`，但**它同时是 8 个职责的门面**，属于过度聚合（见 [[REFACTOR - 重构路线]]）。
- `NcmSession` 对 ncm 五层的门面：外部不直接碰 `NcmAuthApi` 等。

### 1.3 Pimpl

`Oran7Theme` / `Oran7ThemePrivate`（`Oran7Theme_p.h`）。好处是解析器完全隐藏、头文件只暴露 `QVariantMap` 属性。
代价：`Oran7Theme.cpp` 1102 行，`reload*` 一族方法彼此耦合，且 pimpl 里的 `QMap<QObject*, Oran7ThemeData>` 让实例迁移（EXE/插件）变得不可见。

### 1.4 消息队列 + 命令拦截（ffplay 风格）

`MessageQueue`（`FFMsgQueue.h`）+ `ffmsg.h` 的消息号 + `Oran7MediaPlayer::oran7mp_get_msg()` 里的 **REQ_* 拦截执行**。
这是从 ffplay 继承的控制面/数据面分离：GUI 只投递"请求"，内核线程按自己的节奏执行。
新增播放控制（如变速）应走这条路，而不是直接调内核方法。

### 1.5 回调适配器（把 C 风格线程回调接进 QObject）

Native 以 `std::function` 接收帧与消息回调。帧回调捕获渲染桥共享邮箱/代际；消息回调捕获事件邮箱/代际并执行静态 messageLoop，均不捕获 Client/Controller。GUI 接收快照后校验代际；邮箱锁保护投递与接收者析构。

### 1.6 生成器（Generator）

`Oran7ColorGenerator`（Ant Design 10 阶色板）、`Oran7SizeGenerator`（字号/行高）、`Oran7RadiusGenerator`（圆角梯度）。
纯函数、无状态、可单测——是全项目最适合补单测的部分。

### 1.7 表达式驱动的主题 DSL

`Index.json` / `Component.json` 中的 `#Preset_X`、`@token`、`$genColor(...)` 等由内部 Oran7ThemeEvaluator 按依赖解释。P2-5 保留 bool/数字/字符串原生类型，普通字符串不丢空白；组件 Map 按键、值及类型比较后通知，深色单源、profile v2 差量。详见 [[Theme - 主题系统]]。

### 1.8 Model/View

`Oran7LyricsModel : QAbstractListModel`（`Oran7LyricsModel.h:8`），角色 `text/lineIndex`。
反面：曲库/收藏/搜索/整合音乐都不是 model，而是"逐条信号 + QML `ListModel.append()`"。这是本项目最该统一的模式缺口。

### 1.9 状态机（手写）

- 播放：`mp_state_` + 内核裸标志（两层，不同步）。
- 网易云浏览器登录：WebLoginController 状态机；旧 API 扫码链路已随 P1-4 删除。
- 网易云网页登录：`WebLoginController::WebLoginState`（在用）。
- 抓屏参数应用：`m_applyPending` + `m_workerGen` 世代号（事实上的状态机，写得不错）。

### 1.10 世代号 / 单飞（Single-flight）防竞态

- `m_session`（弹幕客户端，HTTP 请求失效）
- `m_fetching`（getDanmuInfo 单飞）
- `m_pendingSwitch`（切歌防重入）
- `m_workerGen`（抓屏 worker 迟到信号过滤）
- `m_generation` / `m_resolveGen`（搜索服务丢弃跨轮结果）
- `syncPending`（D3D 尺寸同步合并）
- `m_checkPending`（扫码轮询重入）

这是本项目最值得保留的"土办法模式"：**凡是异步回调可能晚到的地方，都要有世代号或 pending 标志**。新增异步逻辑请照抄。

### 1.11 适配器（网络传输层）

`HttpClient`（QNAM）/ `CurlHttpClient`（异步 curl.exe）/ `blockingCurl`（同步 curl.exe）三种传输，统一产出 `(status, body, setCookie)`。
`NcmRequest` 是唯一的传输无关 DTO。

### 1.12 策略注入（主题/窗口）

`Oran7WindowAgent` 继承 QWK 的 `QuickWindowAgent`，按 `objectName`（`__Oran7Window__` / `__BORDER__`）识别宿主；`FramelessWindow` 旧策略已在 P2-1 核查后删除。

## 2. 编码约定

### 2.1 命名

| 对象 | 约定 | 例子 |
|---|---|---|
| 类 | `Oran7` 前缀 + PascalCase | `Oran7MediaPlayer`、`Oran7ScreenCaptureController` |
| 模块内实现类 | 无前缀 + `Private` | `Oran7ThemePrivate` |
| 成员 | `m_` + camelCase；内核代码混用 `snake_case_` | `m_capSettings` vs `msg_thread_` |
| 布尔 | `is/has/can` 前缀或过去式 | `isConnected`、`m_applyPending` |
| 信号 | 过去式/完成态 | `urlsReady`、`captureApplyFinished` |
| 槽 | `on` + 信号名 | `onSharedHandlesReady` |
| QML 单例 | 首字母大写、与类同名 | `BasicConfig`、`Oran7MainUiSetting` |

### 2.2 QML 暴露纪律

1. `Q_INVOKABLE` 只做"一件事"，且在 GUI 线程返回；耗时逻辑立刻转 `Oran7Async`。
2. 只读状态用 `Q_PROPERTY(... NOTIFY ...)`，不要用"信号 + QML 自己存"。
3. 暴露给 QML 的枚举必须 `Q_ENUM`（`Oran7MediaClient::ScaleMode`/`RenderObject` 是正确的范例）。
4. 不要把 `QObject*` 作为 `Q_INVOKABLE` 返回值暴露未注册类型（`Oran7MediaClient::screenCapture()` 就是反例，QML 只能动态调 `Q_INVOKABLE`，拿不到枚举和属性通知）。

### 2.3 日志

- 宏：`CLIENT_LOG` / `INFO_LOG` / `WARNING_LOG` / `ERROR_LOG` / `NETWORK_LOG` / `CONFIG_LOG`（`Oran7GlobalHelper.h:152-157`）。
- 实现：`Oran7Logger` 格式化级别/模块后交统一 Qt 入口，Oran7Core 的 Oran7LogSink 脱敏并入有界队列，单写线程写控制台与滚动文件；退出排空，不回退业务线程同步输出。
- 禁止：Cookie、`MUSIC_U`、`__csrf`、`SESSDATA`、`unikey`、`params`、`encSecKey`。
- P1-4/P2-2 已收敛响应正文输出并审计真实调用；禁止新增凭据、正文或二进制包转储，统一规则不能识别无标签的任意秘密。见 [[LOGGING - 日志与脱敏]]。

### 2.4 错误处理

三种风格并存，新增代码请统一到第 3 种：

1. C 风格返回码（内核 `oran7mp_*`，0/-1）。
2. 信号 `errorOccurred(QString)` / `apiError(QString)`。
3. 异步结果 `QVariantMap{success,result,error}`（`Oran7AsyncManager` 契约）。

跨线程错误必须回 GUI 线程再 emit（`Oran7ScreenCapture.cpp:471-476` 是正确写法）。

### 2.5 资源与生命周期

- QObject 父子关系优先；QML 引擎托管的类型显式 `QQmlEngine::CppOwnership`。
- 线程对象用 `moveToThread` + `deleteLater`，不要 `delete` 一个正在跑的 `QThread`。
- 析构里不要做阻塞等待（除 `waitForDone` 这类退出专用路径）。

### 2.6 注释

- 中文、解释"为什么"而非"是什么"；涉及踩坑的地方写清现象（例：`Oran7MediaClient.cpp:1451-1453` 记录"曾经把抓屏参数重置为 0/60/true"）。
- 弃用代码标 `// Discard` 并保留下标（例：`Oran7ScreenCapture.h:335`）。

## 3. 新增功能模板

### 3.1 新增一个 QML 可调用的 C++ 能力

```
1. 判断归属：属于 Oran7MediaClient 还是新单例？跨 EXE/插件共享吗？
2. 共享 → 加进 ORAN7_CORE_SOURCES + 从 EXE glob 排除
3. Q_INVOKABLE/Q_PROPERTY 暴露，枚举加 Q_ENUM
4. 耗时 → Oran7Async::run( 快照参数, 结果回 GUI )
5. 退出清理 → 加进 main.cpp 的 aboutToQuit 链
6. 文档 → MODULES 对应模块的「对外接口」表加一行
```

### 3.2 新增一个 QML 页面/组件

```
1. 放 src/qml/<域>/，手动加进 qml.qrc
2. 需要跨页状态 → 提到 C++ 的 Q_PROPERTY，不要新增 QML 单例
3. 需要页面路由 → BasicConfig 的 page 枚举 + RightPage.navigateTo
4. 主题色 → 用 Oran7Theme 的 token，禁止硬编码 #RRGGBB
```

### 3.3 新增一个网络接口

```
1. 传输层只认 NcmRequest；不要在 api 层直接写 QNetworkRequest
2. 走 curl（带 Cookie/写操作）还是 QNAM（匿名只读）→ 见 Ncm 文档的判定表
3. 解析结果统一成 NcmTrackMeta（目标）→ 不要把 JSON 字段名泄漏到 QML
4. 需要后台 → Oran7AsyncManager 注册 handler，参数全值传递
```

### 3.4 新增一个主题 token

```
1. 语义 token → Index.json __style__
2. 组件 token → Component.json __style__
3. 新组件 → 加 __component__ 映射 + Oran7Theme 的 Q_PROPERTY + g_componentTable + <Comp>Changed 信号
4. 数字/布尔 token 必须同时确认 profile 会带类型写入（否则会被 toString() 清空）
5. 已运行过的机器要删 ../../Config/profile-default.json 才看得到源文件改动
```

## 4. 反模式清单（看到就该警惕）

| 反模式 | 例子 |
|---|---|
| 上帝对象 | `Oran7MediaClient`（2050 行，8 个职责） |
| 跨文件引用 QML id | `RightPage.qml` 用 `main.qml` 的 `bottomRectangle` |
| 裸指针 + 手动 delete 跨线程（本项已改） | Controller unique_ptr 移交后台闭包，消息通过共享邮箱隔离生命周期 |
| 字符串 key 配置 | `getValueQVariant("screen_capture.fps")` 散落各处 |
| 全局可变静态 | `DEFAULT_VOLUM_SIZE`、抓屏 `static SwsContext*` |
| 已清理：死代码留在主路径 | SearchPopup 已于 P1-7 删除，FramelessWindow/Oran7WindowApi 已于 P2-1 删除 |
| 一个事实多副本 | `isPlaying`、`currentPlaylistOwner`、深色模式 |
| 已清理：录制硬编码路径/凭据 | P1-6 改为 recording 配置与唯一 MP4 文件名，P2-3 已完成凭据分离与 DPAPI 加密 |
