---
status: 📝 进行中
updated: 2026-09-08
---

# 模块分工

> 本文回答：这个项目有哪些模块，各自职责、关键类、对外接口。
>
> **变更（2026-09-08）**：原 INTERFACES 已并入本文——C++↔QML 接口归属到各模块的「对外接口」。同日去掉了模块/组序号，改为清爽的统一结构。
>
> 接口快速核验（代码永远是最新事实源）：
> - 运行时注册全量：`grep -n "qmlRegister" main.cpp`
> - Oran7UI.Impl 编译期导出：`grep -rn "QML_NAMED_ELEMENT\|QML_SINGLETON" src/cpp/QtQuicCppExtend`

## 模板：一个模块怎么组织

每个模块都用同一套"最省、最可扫"的结构——**一段职责开头，元信息用列表，只有「对外接口」用表格**。不堆字段标题、不贴代码。

```markdown
### <模块名>

<一段话：职责 / 边界 / 易踩坑，读完即懂，别贴代码>

- **源码** `src/...`
- **产物** EXE / Oran7Core.dll / Oran7UI.Impl 插件 DLL / 其他
- **关键类** 值得单列的：`类名` — 一句话（每条一行；没有就不写）
- **备注**（有才写）

对外接口（C++↔QML 或跨模块调用面；只记"是什么 + 去哪看"）
| 暴露名 | 形态 | URI / 来源 | 位置 | 说明 |
|---|---|---|---|---|
| `Xxx` | S | `...` | 文件:行 | ... |

<!-- TODO: 待补问题 -->
```

- 形态：**S**=单例 · **T**=类型 · 上下文属性=`engine.rootContext` · ImageProvider=`image://`；无 QML 暴露的模块写"无直接 QML 注册"，并说明能力由谁触发。
- 模块**不加序号、不嵌套深**：插入新模块不需要给别的模块改号。
- **模块名是最小标题**：模块内部不再出现任何标题（`##`~`####`），内容只用正文 / 列表 / 表格；需要给某块内容命名，就用一行普通文字（如 `对外接口（main.cpp 运行时注册）：`），它不会进大纲。
- 新增/改动 `qmlRegister*` 或 Oran7UI.Impl 导出 → 只动所属模块「对外接口」表。
- 别复述代码：接口行只给"名字 → 位置 → 一句话"。
- 正文填完并对照代码核过才标 ✅，核对日期写 `<!-- 核对日期 -->`。

---

## C++ 源模块

| 目录                     | 职责                            | 产物                             |
| ---------------------- | ----------------------------- | ------------------------------ |
| `GlobalSingleton/`     | 全局单例层                         | 4 个进 Oran7Core.dll，其余编进 EXE    |
| `QtQuicCppExtend/`     | QML 导出层 `import Oran7UI.Impl` | Oran7UI.Impl 插件 + EXE（每文件编译两次） |
| `Oran7MediaPlayer/`    | FFmpeg 播放内核                   | EXE                            |
| `Oran7VideoGPURender/` | D3D11 视频渲染                    | EXE                            |
| `Oran7ScreenCapture/`  | 录屏 + WHIP 推流                  | EXE                            |
| `Oran7Sever/`          | 遗留 TCP 服务器（未启用）               | EXE（main.cpp 已注释）              |
| `Oran7Utils/`          | 共享定义                          | 头文件 `Oran7Definitions.h`       |

### GlobalSingleton

全局单例层：对象生命周期集中管理，`ApplicationContext::instance()` 在 `main()` 早期实例化全部单例（见 [[ARCHITECTURE - 架构]] 启动流程）。

- **源码** `src/cpp/GlobalSingleton/`
- **产物** 编进 EXE；其中 `GlobalHelper` / `Oran7Async` / `AppJsonConfigManager` / `Oran7ThemeProfileManager` 进 `Oran7Core.dll`（CMakeLists `ORAN7_CORE_SOURCES`，从 EXE glob 排除）
- **关键类** 均由 `ApplicationContext` 持有：`Oran7MediaClient`（QML 面向对象入口）· `ApplicationContext` · `AppJsonConfigManager` · `Oran7ThemeProfileManager` · `GlobalHelper` · `Oran7Async` · `Oran7AsyncManager` · `BilibiliAuthManager` · `Oran7MusicSearchService` · `Oran7NetworkMonitor` · `LyricsModel`+`LrcParser` · `GlobalEventFilter` · `WindowApi` · `ConsoleLogger` · `Oran7SoundEffect`
- **待确认** `OnlineLyricsFetcher`（OnlineLyricsFetcher.h）是否单例、被谁调用
- **备注** 未暴露给 QML（仅 C++ 内部用）：`ApplicationContext` / `AppJsonConfigManager` / `Oran7ThemeProfileManager` / `GlobalHelper` / `Oran7Async` / `LrcParser` / `WindowApi` / `ConsoleLogger`

对外接口（main.cpp 运行时注册）

| 暴露名                       | 形态    | URI                | 位置           | 说明                      |
| ------------------------- | ----- | ------------------ | ------------ | ----------------------- |
| `Client`                  | S     | `Client`           | main.cpp:278 | 主 QML 面向对象入口（胶水层）       |
| `BilibiliAuthManager`     | S     | `BilibiliAuth`     | main.cpp:193 | B站认证（dedeUserId）        |
| `Oran7MusicSearchService` | S     | `Oran7MusicSearch` | main.cpp:201 | 网易云搜索                   |
| `Oran7NetworkMonitor`     | S     | `Oran7Network`     | main.cpp:241 | 网络监测                    |
| `Oran7AsyncManager`       | S     | `Oran7Async`       | main.cpp:249 | 异步线程池，`Async.run()` 入口  |
| `LyricsModel`             | T     | `LyricsModel`      | main.cpp:188 | 歌词模型（解析靠内部 `LrcParser`） |
| `Oran7SoundEffect`        | T     | `Oran7Sound`       | main.cpp:189 | 音效播放                    |
| `globalEventFilter`       | 上下文属性 | rootContext        | main.cpp:260 | 全局事件过滤器实例               |

<!-- TODO: 每个单例的一句话职责 + 生命周期（创建/cleanup，对照 ApplicationContext.cpp）；每个暴露对象的信号/槽/属性（从 .h 提取） -->

### QtQuicCppExtend

QML 导出层：主题引擎、无边框窗口、通用工具、属性生成器。目录每份源文件被编译两次（EXE + Oran7UI.Impl 插件 DLL）。

- **源码** `src/cpp/QtQuicCppExtend/`
- **产物** `Oran7UI/Impl/Oran7UI_Implplugin.dll`（`qt_add_qml_module`，CMakeLists.txt:175）+ 同一批源文件编进 EXE
- **备注** `Oran7Theme_p` 是主题私有实现，无 QML 宏、不导出

对外接口（编译期导出，`QML_NAMED_ELEMENT` / `QML_SINGLETON`，`import Oran7UI.Impl`）

| 暴露名 | 形态 | 来源 | 说明 |
|---|---|---|---|
| `Oran7Theme` | S | Oran7Theme.h:19 | 主题引擎 |
| `Oran7ThemeFunctions` | S | Oran7ThemeFunctions.h:11 | 主题函数工具 |
| `Oran7Api` | S | Oran7Api.h:11 | 通用工具 API |
| `Oran7ColorGenerator` | S | Oran7ColorGenerator.h:12 | 颜色生成器（主题派生用） |
| `Oran7FileHelper` | T | Oran7FileHelper.h:12 | 文件工具（活路经此导出；main.cpp:184 同名 `qmlRegisterType` 是注释死代码） |
| `Oran7RadiusGenerator` | T | Oran7RadiusGenerator.h:11 | 圆角生成器 |
| `Oran7SizeGenerator` | T | Oran7SizeGenerator.h:11 | 尺寸生成器 |
| `Oran7WindowAgent` | T | Oran7WindowAgent.h:12 | 无边框窗口代理（基于 qwindowkit） |

<!-- TODO: 每个 Generator 输入输出契约；Oran7Theme 加载流程 -->

### Oran7MediaPlayer

FFmpeg 播放内核 + B站扩展（弹幕 / 直播间地址解析）+ 工具（变速变调）。

- **源码** `src/cpp/Oran7MediaPlayer/`
- **产物** 编进 EXE
- **关键类**
  - `Oran7FFplayerCore`（Oran7FFplayerCore.h/.cpp）— 播放核心：解封装读取、音视频同步、音频输出、seek/变速/消息队列编排
  - `Oran7FFmpegDecoderCore`（Oran7FFmpegDecoderCore.h/.cpp）— 单个音/视频流解码线程，被前者以 `auddec`/`viddec` 持有
  - `FFMsgQueue` + `ffmsg.h` — 消息队列与消息号定义
  - `Oran7MediaPlayer`（Oran7MediaPlayer.h/.cpp）— 门面封装，唯一外部入口
  - `Sonic`（Sonic.h/.cpp）— 变速变调
  - `ImageScaler`（ImageScaler.h）— 图像缩放（当前未实际使用）

对外接口（运行时注册）

| 暴露名                        | 形态  | URI                        | 位置           | 说明                                                                           |
| -------------------------- | --- | -------------------------- | ------------ | ---------------------------------------------------------------------------- |
| `BilibiliRoomAddressCatch` | T   | `BilibiliRoomAddressCatch` | main.cpp:185 | 直播间地址解析（类名 `BilibiliRoomAddressCatch`，头文件名为 `BilibliLiveRoomAddressCatch.h`） |
| `BilibiliDanmakuClient`    | T   | `BilibiliDanmakuClient`    | main.cpp:186 | B站弹幕客户端                                                                      |

<!-- TODO: 解码/渲染线程模型（解码线程、消息队列投递、video_refresh 链路） -->

### Oran7VideoGPURender

D3D11 视频渲染。

- **源码** `src/cpp/Oran7VideoGPURender/`
- **产物** 编进 EXE
- **关键类** `D3d11Videoitem`（QML 侧名 `D3D11VideoItem`）· `D3d11VideoRenderNode` · `D3d11DeviceProvider`

对外接口（运行时注册）

| 暴露名 | 形态 | URI | 位置 | 说明 |
|---|---|---|---|---|
| `D3D11VideoItem` | T | `D3D11VideoItem` | main.cpp:187 | D3D11 视频渲染项；`D3d11DeviceProvider` / `D3d11VideoRenderNode` 为内部支撑 |

<!-- TODO: 纹理上传与渲染链路 -->

### Oran7ScreenCapture

录屏 → 编码 → 本地 MP4 / WHIP 推流。

- **源码** `src/cpp/Oran7ScreenCapture/`
- **产物** 编进 EXE
- **关键类** 采集 `Oran7ScreenCapture` · 编码 `Oran7VideoEncoder` / `Oran7AudioEncoder` · 封装 `Oran7Mp4Muxer` · 推流 `Oran7WhipPublisher` + `Oran7SignalingClient`（libdatachannel）

对外接口

无直接 QML 注册（全目录未注册为 QML 类型），能力由上层胶水触发（ScreenCapture 设置页 / `Client`）。

<!-- TODO: 确认调用链入口（谁拉起采集/推流）；录屏→编码→本地 MP4 / WHIP 两条链路 -->

### Oran7Sever（遗留，未启用）

遗留 TCP 服务器，`main.cpp` 已注释未启用。

- **源码** `src/cpp/Oran7Sever/`
- **产物** 编进 EXE
- **关键类** `Sever` · `DataBase` · `LoginHandler` · `ProtocolHandler` · `LoadingDataHandler`

对外接口

无 QML 注册。

<!-- TODO: 记录为何弃用、是否计划删除 -->

---

## 网易云客户端

```
crypto/    WeapiCrypto + CryptoProvider   （AES/RSA 加密，依赖 MSYS2 OpenSSL）
network/   HttpClient（QNAM）+ CurlHttpClient/BlockingCurl（系统 curl.exe）+ CookieStore
protocol/  NcmProtocol / NcmRequest       （weapi 签名、请求组装）
api/       NcmAuthApi / NcmSongApi / NcmPlaylistApi / NcmUserApi / NcmAsyncHandlers
session/   NcmSession + QrLoginController + WebLoginController
```

自下而上 crypto → network → protocol → api → session。crypto/network/protocol 为纯 C++ 内部层，不直接对 QML 开放；对 QML 的暴露集中在 session/api 两层，四者同属一个 import `Oran7NetEaseAuth 1.0`。

- **源码** `src/ncm/`
- **易踩坑** 双 HTTP 通道：登录态 / VIP 歌单 / 写操作走系统 `C:\Windows\System32\curl.exe`（QProcess + Schannel TLS）；QNAM 通道会被网易 WAF 拦截（VIP 歌返回 code:-110）。详见 [[ADR/README]]

对外接口（运行时注册）

| 暴露名 | 形态 | 注册位置 | 归属层 | 说明 |
|---|---|---|---|---|
| `NcmSession` | S | main.cpp:209 | session | 登录会话（isLoggedIn/userName/logout） |
| `NcmQrLogin` | S | main.cpp:217 | session | 扫码登录弹窗（`QrLoginController`，并同时提供 `image://ncmqr` provider，见 main.cpp:293） |
| `NcmWebLogin` | S | main.cpp:225 | session | 网页登录（`WebLoginController`） |
| `NcmPlaylistApi` | S | main.cpp:233 | api | 歌单 API |

- api 层其余类（`NcmAuthApi` / `NcmSongApi` / `NcmUserApi`）与 `Oran7MusicSearchService`（GlobalSingleton）复用关系待补。
- <!-- TODO: 各 API 类暴露的槽/信号；NcmAsyncHandlers 与 Oran7AsyncManager 的关系 -->

---

## QML 模块

```
Basic/        BasicConfig.qml（配置单例）+ qmldir
Components/   通用组件 ~28 个
LeftPage/     左侧导航
RightPage/    MainStackView（LocalMusic/SearchMusic/VideoPlayer/ScreenCapture/Setting 五个 Stack）
              + TopMenuBar（BilibiliLoginPopup/SearchPopup）
BottomPage/   底部播放栏
Settings/     GlobalSettings + PagesSettings + SettingWindows（5 个设置窗口）
```

全部 UI。与 C++ 的接线有三类：① 直接 `import` 各注册 URI 用单例/类型；② 模块树内的 QML 文件以 QUrl 形式注册为单例；③ 组件间属性/信号。

- **源码** `src/qml/`

对外接口（QML 文件自身以 QUrl 注册为单例，仍是 main.cpp 运行时注册）

| 暴露名 | 形态 | 来源文件 | 注册位置 | 说明 |
|---|---|---|---|---|
| `BasicConfig` | S | src/qml/Basic/BasicConfig.qml | main.cpp:181 | QML 侧配置单例 |
| `Oran7MainUiSetting` | S | src/qml/Settings/GlobalSettings/Oran7MainUiSetting.qml | main.cpp:182 | 主 UI 设置 |

<!-- TODO: 扫 QML 侧自声明单例/模块（`grep -rn "qmldir\|pragma Singleton\|singleton " src/qml`）；Basic/ 的 qmldir 是否独立模块待确认 -->
<!-- TODO: 每个组件一句话用途；五个 Stack 进入/返回路由 -->

---

## 第三方

| 目录 | 用途 | 构建方式 |
|---|---|---|
| `qwindowkit` | 无边框窗口（QWKQuick，被 `Oran7WindowAgent` 使用） | add_subdirectory 静态编译，仅 QML |
| `libdatachannel` | WHIP/WebRTC 推流 | subdir 构建，含 libjuice/libsrtp/usrsctp/plog |
| `qrcodegen` | 网易云登录二维码（nayuki） | 源文件编入 EXE |
| `brotli` | B站弹幕 protover=3 解压 | ~10 个 .c 编入 EXE（CMake 因此启用 C） |

<!-- TODO: 补 ffmpeg6.0（FFmpeg + SDL2）与 SQLite 两个运行时依赖的说明 -->

## 变更记录

- 2026-09-08（重排）：去掉模块/组序号，改用"职责一段 + 元信息列表 + 对外接口表"；原 INTERFACES 并入（`Oran7SoundEffect` URI 实为 `Oran7Sound`；`Oran7FileHelper`/`FramelessWindow` 的注册是注释死代码）；类改名 `Oran7FFplayerCore` / `Oran7FFmpegDecoderCore` 及文件重命名已同步进 Oran7MediaPlayer 模块卡。
