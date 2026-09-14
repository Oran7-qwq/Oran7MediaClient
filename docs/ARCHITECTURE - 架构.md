---
status: 🚧 骨架
updated: 2026-09-08
---

# 整体架构（ARCHITECTURE）

> 本文回答：这个程序由哪几块拼成、它们怎么互相依赖、启动时发生了什么。

## 1. 运行时产物

```
Oran7MediaClient.exe              主程序：QML + 全部功能代码
├── Oran7Core.dll                 共享单例库（EXE 与 QML 插件共用一份）
└── Oran7UI/Impl/Oran7UI_Implplugin.dll   QML 插件模块（import Oran7UI.Impl）
```

- `Oran7Core.dll` 只含 4 个单例：`GlobalHelper` / `Oran7Async` / `AppJsonConfigManager` / `Oran7ThemeProfileManager`（见 `CMakeLists.txt` 的 `ORAN7_CORE_SOURCES`）。
- `Oran7UI_Impl` 插件由 `src/cpp/QtQuicCppExtend/` 生成，该目录**每份文件被编译两次**（EXE + 插件 DLL）。

<!-- TODO: 补一张依赖图（谁链接谁） -->

## 2. 目录结构总览

<!-- TODO: 补完整目录树（src/cpp、src/ncm、src/qml、config、3rdparty、ffmpeg6.0、SQLite、shaders、build） -->

详见 [[MODULES]] 与 [[RESOURCES - 资源]]。

## 3. 构建与工具链

- CMake + Ninja；Qt 6.7.3 (MinGW 11.2)，构建目录 `build/Desktop_Qt_6_7_3_MinGW_64_bit-{Debug,Release}`。
- 外部硬依赖（绝对路径）：MSYS2 OpenSSL、MSYS2 zlib；自带 FFmpeg 6.0 + SDL2。

<!-- TODO: 把 CMakeLists.txt 的每个 target 的关系梳理成一段说明（Oran7Core → Oran7UI_Impl → EXE 的链接链） -->

## 4. 启动流程（main.cpp）

1. 设置渲染相关环境：`QSG_RENDER_LOOP=basic`、`QT_MEDIA_BACKEND=windows`（强制 Media Foundation，避免与自带 FFmpeg 冲突）、GL 3.3 兼容面、透明 alpha。
2. 注册全局崩溃 minidump handler（`Oran7MediaClient_crash.dmp`）。
3. 创建 `QGuiApplication`，强制 D3D11 场景图后端。
4. `ApplicationContext::instance()` 实例化所有全局单例。
5. `qmlRegister*` 注册全部 C++ ↔ QML 接口（接口按类归属在 [[MODULES]] 各模块卡的「对外接口」；全量清单可 `grep -n "qmlRegister" main.cpp`）。
6. 加载 `qrc:/main.qml`。
7. 退出时按序清理：停播放器 → 线程池 shutdown → 保存 Cookie/配置 → 静态析构收尾。

## 5. 模块边界与依赖方向

- 分层：QML（UI）→ `Client`/单例（业务胶水）→ 功能模块（播放/渲染/录屏/网络）。
- 共享层：`Oran7Core.dll` 是唯一跨 EXE + 插件共享的单例所在；新增跨端单例必须加入 `ORAN7_CORE_SOURCES`。

<!-- TODO: 画出「哪些模块依赖哪些模块」的方向图，防止后续新增模块破坏依赖方向 -->

## 6. 关键架构决策索引

所有历史决策与理由见 [[ADR/README]]。已记录：

- 匿名单例为何必须进 Oran7Core
- ncm 为何走双 HTTP 通道（系统 curl.exe）
- B 站弹幕为何必须 uid=0 + protover=3
- 为何强制 `QT_MEDIA_BACKEND=windows`

<!-- TODO: 正文补充完成后移除本节占位 -->