---
status: ✅ 已核对
updated: 2026-10-24
---

# 模块索引（MODULES）

> 本文是模块地图：每个模块一句话职责 + 详细文档链接 + 全量 QML 注册清单。
> 模块内部设计、接口、线程、状态、踩坑都在各自的详情文档里，本文不复述。

2026-10-24 P0 接口更新：`Client.renderBlackFrame` 返回 bool；RightPage 增加 captionExpanded 输入与 musicPageActive 输出；NcmSongApi 的播放关联号已随 P1-4 统一为 urlsResolved(requestId, map)，无未关联广播。QML 注册名称不变，Oran7Theme 实现与元对象只由插件 DLL 提供。

2026-10-24 P1 首批：新增 Client.playback 与 Oran7PlaybackState；新增 stopPlayback 命令、Oran7TaskHandle 和 QML cancel/cancelAll；两种异步门面共用 Core 线程池。

2026-10-24 命名统一：GlobalSingleton 中 12 个 C++ 类补齐 Oran7 前缀；QML 导入名、注册名和上下文属性不变，映射见 [[Core - 单例与胶水层]]。

## 模块地图

| 模块 | 一句话职责 | 源码 | 产物 | 详情 |
|---|---|---|---|---|
| 单例与胶水层 | QML 唯一入口 `Oran7MediaClient` + 全局服务 + 生命周期 | `src/cpp/GlobalSingleton/`、`src/cpp/Oran7MediaClient/` | EXE + `Oran7Core.dll`（4 个既有共享类及 Oran7Config 类型） | [[Core - 单例与胶水层]] |
| 播放内核 | FFmpeg/SDL 解码、同步、seek、变速、B 站扩展 | `src/cpp/Oran7MediaPlayer/` | EXE | [[Player - 播放内核]] |
| D3D11 渲染 | `QSGRenderNode` 视频显示与格式转换 | `src/cpp/Oran7VideoGPURender/` | EXE | [[Render - D3D11视频渲染]] |
| 录屏与推流 | DDA 采集 → 编码 → MP4 / WHIP | `src/cpp/Oran7ScreenCapture/` | EXE | [[Capture - 录屏与推流]] |
| 网易云接入 | 登录、搜索、歌单、播放地址（五层 + 双通道） | `src/ncm/` | EXE | [[Ncm - 网易云接入]] |
| B 站接入 | 认证 Cookie、直播弹幕、直播流地址 | `GlobalSingleton/Oran7BilibiliAuthManager` + `Oran7MediaPlayer/Bilibili*` | EXE | [[Bilibili - B站接入]] |
| 主题系统 | JSON token 引擎 + 生成器 + profile | `src/cpp/QtQuicCppExtend/` + `Oran7ThemeProfileManager` | Oran7UI_Impl（引擎）+ Oran7Core（profile） | [[Theme - 主题系统]] |
| 配置与持久化 | `config.json` / `profile-*.json` | `Oran7AppConfigManager`、`Oran7ThemeProfileManager` | `Oran7Core.dll` | [[Config - 配置与持久化]] |
| QML 界面 | 全部 UI、单例、路由、组件 | `src/qml/` + `main.qml` | `qml.qrc` | [[UI - QML界面]] |
| 构建与资源 | 工具链、目标、第三方、打包 | `CMakeLists.txt`、`*.qrc`、`oran7_pkt.bat` | — | [[Build - 构建与资源]] |
| 遗留 TCP 服务 | 早期登录/云端协议，**未启动** | `src/cpp/Oran7Sever/` | EXE | 见本文末 |

横向文档：[[ARCHITECTURE - 架构]]、[[RUNTIME_STATE - 运行时状态]]、[[ASYNC - 异步与线程]]、[[DATAFLOW - 数据路由]]、[[PATTERNS - 设计模式与约定]]、[[REFACTOR - 重构路线]]。

## 全量 QML 注册清单（`main.cpp`）

运行时注册：

| 暴露名 | 形态 | URI | 位置 | 归属 |
|---|---|---|---|---|
| `BasicConfig` | 单例（QML 文件） | `BasicConfig` | `:180` | UI |
| `Oran7MainUiSetting` | 单例（QML 文件） | `Oran7MainUiSetting` | `:181` | UI |
| `BilibiliRoomAddressCatch` | 类型 | `BilibiliRoomAddressCatch` | `:184` | Player |
| `BilibiliDanmakuClient` | 类型 | `BilibiliDanmakuClient` | `:185` | Player |
| `D3D11VideoItem` | 类型 | `D3D11VideoItem` | `:186` | Render |
| `LyricsModel`（C++ `Oran7LyricsModel`） | 类型 | `LyricsModel` | `main.cpp` | Core |
| `Oran7SoundEffect` | 类型 | `Oran7SoundEffect`（URI `Oran7Sound`） | `:188` | Core |
| `BilibiliAuthManager`（C++ `Oran7BilibiliAuthManager`） | 单例 | `BilibiliAuth` | `main.cpp` | Bilibili |
| `NcmSession` | 单例 | `Oran7NetEaseAuth` | `:208-213` | Ncm |
| `NcmWebLogin`（`WebLoginController`） | 单例 | `Oran7NetEaseAuth` | `:224-229` | Ncm |
| `NcmPlaylistApi` | 单例 | `Oran7NetEaseAuth` | `:232-237` | Ncm |
| `Oran7NetworkMonitor` | 单例 | `Oran7Network` | `:240-245` | Core |
| `Oran7AsyncManager` | 单例 | `Oran7Async` | `:248-253` | Core |
| `Oran7PlaybackState` | 不可创建类型；由 Client.playback 持有 | `Client` | `main.cpp` | Core |
| `Oran7VideoHistory` | 不可创建模型；Client.videoHistory 借用 | `Client` | `main.cpp` | EXE |
| `Client`（C++ `Oran7MediaClient`） | 单例 | `Client` | `main.cpp` | Core |
| `globalEventFilter` | 上下文属性 | — | `:256-259` | Core |
| `image://ncmqr` | ImageProvider | — | `:292-293` | Ncm |

编译期导出（`import Oran7UI.Impl`）：

| 暴露名 | 形态 | 来源 | 说明 |
|---|---|---|---|
| `Oran7Theme` | 单例 | `Oran7Theme.h:16` | 主题引擎 |
| `Oran7ThemeFunctions` | 单例 | `Oran7ThemeFunctions.h:8` | 颜色/字号/圆角门面（QML 未用） |
| `Oran7Api` | 单例 | `Oran7Api.h:8` | Win32 窗口工具（QML 未用） |
| `Oran7ColorGenerator` | 单例 | `Oran7ColorGenerator.h:8` | 色板/预设色 |
| `Oran7FileHelper` | 类型 | `Oran7FileHelper.h:9` | 路径存在性 |
| `Oran7RadiusGenerator` | 类型 | `Oran7RadiusGenerator.h:8` | 圆角梯度 |
| `Oran7SizeGenerator` | 类型 | `Oran7SizeGenerator.h:8` | 字号/行高梯度 |
| `Oran7WindowAgent` | 类型 | `Oran7WindowAgent.h:8` | QWindowKit 无边框代理 |

P2-1 已删除未注册、无调用的 `Oran7WindowApi` 与 `FramelessWindow`，保留 `Oran7Api` 正式导出；见 [[REFACTOR - P2-1清理记录]]。

## 模块间关键依赖（补充 ARCHITECTURE 的分层图）

| 依赖 | 形式 | 评价 |
|---|---|---|
| Player → `Oran7BilibiliAuthManager` / `NcmSession` | 直接调 `ffmpegCookieString()`（`Oran7FFplayerCore.cpp:209-232`） | ❌ 内核反向依赖业务单例；应改为"打开时由上层传入 headers" |
| Capture → `D3D11Videoitem` | `Oran7ScreenCapture.h:14` 直接 include | ❌ 横向耦合；应只依赖 `QQuickItem`/帧接口 |
| Core → 所有模块 | 组合根 | ✅ 允许，但要防止 `Oran7MediaClient` 变成第二个组合根 |
| Plugin → `Oran7Core` | 链接 | ✅ 共享单例 |
| QML → Core/Ncm/Plugin | import + 单例 | ✅ 允许；禁止 QML 直接 import 内核头 |

## 陈旧未启用模块：自定义 TCP/Server

服务端 `src/cpp/Oran7Sever/` 保留但不从 main 启动。旧客户端链路已隔离到 `Oran7LegacyServerClient`，默认不创建 Socket、不连接服务器、不打开 SQLite，兼容入口不自动启用；Client 只持有会话服务。

用户明确推迟至 v1.0 正式版之后或后续版本再评估官方服务器。当前保留源码和旧库，不列入自动删除或功能开发范围；退出路径也已移除对 Client 子树的通用 Socket 查找。详见 [[Legacy - 自定义服务端（陈旧未启用）]]。

## 变更记录

- 2026-10-22：按模块拆出 10 份详情文档；本文改为索引 + 全量注册清单；补正 `Oran7SoundEffect` URI、`Oran7FileHelper`/`FramelessWindow` 注释死代码、`Oran7WindowApi` 假导出。
- 2026-09-08：原 INTERFACES 并入本文，去掉模块编号。
