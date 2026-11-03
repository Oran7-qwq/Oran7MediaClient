---
status: ✅ 已核对
updated: 2026-10-24
---

# Build：构建、资源与打包

> `CMakeLists.txt`、`qml.qrc`、`res.qrc`、`oran7_pkt.bat`、`PACKAGING_GUIDE.md`
> 这一层的硬编码最多，换机器/换目录最容易失败。

## 工具链（硬编码）

| 项 | 路径 |
|---|---|
| Qt | `D:/Qt/6.7.3/mingw_64`（**6.7.2 套件是坏的**：缺 `qmake.exe` 与 `lib/cmake/Qt6`） |
| GCC 11.2 | `D:/Qt/Tools/mingw1120_64/bin` |
| CMake | `D:/Qt/Tools/CMake_64/bin/cmake.exe` |
| Ninja | `D:/Qt/Tools/Ninja/ninja.exe` |
| OpenSSL / zlib | `C:/msys64/mingw64`（`libwinpthread.a`、`libcrypto.dll.a`、`libz.dll.a`） |
| FFmpeg 6.0 + SDL2 | 仓库内 `ffmpeg6.0/` |
| SQLite | 仓库内 `SQLite/` |

只构建 Release（项目约定）：`build/Desktop_Qt_6_7_3_MinGW_64_bit-Release`。

```bat
D:/Qt/Tools/CMake_64/bin/cmake.exe -S . -B build/Desktop_Qt_6_7_3_MinGW_64_bit-Release ^
  -G Ninja ^
  -DCMAKE_PREFIX_PATH=D:/Qt/6.7.3/mingw_64 ^
  -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1120_64/bin/gcc.exe ^
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1120_64/bin/g++.exe ^
  -DCMAKE_BUILD_TYPE=Release
D:/Qt/Tools/Ninja/ninja.exe -C build/Desktop_Qt_6_7_3_MinGW_64_bit-Release
```

`Oran7ScreenCapture` 的几份大 TU 并行编译可能 `cc1plus.exe: out of memory` → 用 `ninja -j 2` 收尾。

## CMake 目标结构

```
add_subdirectory(3rdparty/qwindowkit)          QWKQuick 静态
add_subdirectory(3rdparty/libdatachannel)       WHIP/WebRTC，依赖 MSYS2 OpenSSL
qt_add_library(Oran7Core SHARED ...)           5 个共享服务/辅助类 + Oran7Config 纯配置类型（含 P2-2 Oran7LogSink）
qt_add_qml_module(Oran7UI_Impl ...)            QML 插件（QtQuicCppExtend）
qt_add_resources(Oran7UI_Impl ...)             主题 JSON 全量
qt_add_shaders(...)                            video.vert/frag、gradientText.frag → .qsb
add_executable(Oran7MediaClient ...)           主程序
qt_add_resources(Oran7MediaClient ...)          主题 JSON 子集 + default.json
```

源文件收集：

```cmake
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/cpp/*.cpp" "src/cpp/*.h")
# 在 ORAN7_CORE_SOURCES / ORAN7_QML_SOURCES 定义后，按清单排除共享源码。
foreach(source IN LISTS ORAN7_CORE_SOURCES ORAN7_QML_SOURCES)
    list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
endforeach()
file(GLOB_RECURSE NCM_SOURCES ... "src/ncm/*.cpp" "src/ncm/*.h")
```

⇒ 新增 `.cpp` 不用重配；但 `Oran7UI_Impl` 的 `SOURCES` 是**手写清单**，新增导出类型必须手动加。

## 第三方依赖

| 目录 | 用途 | 构建方式 | 备注 |
|---|---|---|---|
| `3rdparty/qwindowkit` | 无边框窗口（QWKQuick） | `add_subdirectory` 静态，仅 Quick | 需 `QWK_QUICK_LIBRARY=1`/`QWK_CORE_LIBRARY=1` 宏 |
| `3rdparty/libdatachannel` | WHIP/WebRTC | subdir，含 libjuice/libsrtp/usrsctp/plog | `-Wno-format`、`-Wno-cast-function-type`；`NO_WEBSOCKET=ON` |
| `3rdparty/qrcodegen` | 网易云登录二维码 | 源文件编进 EXE | nayuki |
| `3rdparty/brotli` | B 站弹幕 `protover=3` | 10 个 `.c` 编进 EXE | CMake `LANGUAGES C CXX` 的原因 |
| `ffmpeg6.0/` | 解码/编码/采集/封装 | 预编译 DLL | POST_BUILD 拷到 exe 旁 |
| `SQLite/` | QSQLITE | 预编译 DLL | POST_BUILD 拷 `SQLite/` |

只有 ncm 的 crypto 文件拿到 MSYS2 include 目录（避免全局头文件遮蔽）；只有 `BilibiliDanmakuClient.cpp` 拿到 MSYS2 include（`zlib.h`）。

## 资源嵌入

| 文件 | 内容 | 铁律 |
|---|---|---|
| `qml.qrc` | 全部应用 QML（`qrc:/main.qml` 入口） | 新增 `.qml` **手动**登记，无 glob |
| `res.qrc` | `image/`、`sound/`、`app.rc`；P2-1 已移除停用的 `bgra2rgba_*.hlsl` | 新增图片/声音手动登记 |
| `config/themeJson/*.json` | 主题 | **两处** `qt_add_resources`，清单不同（插件全量 / EXE 子集） |
| `config/default.json` | 默认配置 | 两处都含 |
| `shaders/*.vert|frag` | 运行时着色器 | 由 `qt_add_shaders` 编译成 `.qsb` |

P0 已将 RightPage/BottomPage 识别到的外部 id 引用改为显式属性或附属窗口访问；其他组件的创建上下文依赖需按实际使用逐项核查。

## 运行时产物布局

```
<build dir>/                       或打包输出目录
├── Oran7MediaClient.exe
├── Oran7Core.dll
├── Oran7UI/Impl/Oran7UI_Implplugin.dll   ← 最常遗漏，缺了程序起不来
├── ffmpeg6.0/bin/*.dll
├── SQLite/*.dll
├── zlib1.dll（POST_BUILD 拷入）
├── libcrypto-3-x64.dll / libssl-3-x64.dll / libwinpthread-1.dll（打包脚本拷入）
└── ../Config/                    ← 注意是 exe 的上一级
    ├── config.json
    └── profile-default.json
```

运行时还要求 `C:/msys64/mingw64/bin` 在 PATH（或 DLL 在 exe 旁），否则 `libcrypto`/`libz` 加载失败。

## 启动器

`C:\Users\funny\Desktop\Oran7MediaClient-Launcher-build\Oran7MediaClient-Launcher.exe`
作用：给原始构建目录的 exe 补 PATH（Qt bin、MinGW bin、MSYS2 bin、exe 目录），否则 `0xC0000135`；回退顺序 Release → Debug。

## 打包

`oran7_pkt.bat`：`-idir <构建目录> -odir <输出目录>`，无参数则交互询问，并记住到 `oran7_pkt.ini`。
流程：windeployqt → 拷 MinGW/MSYS2 OpenSSL DLL → 写 `qt.conf` + README → 拷 `Oran7UI/` 插件目录。
`package_release.bat/.sh` 已过期（仍指向 6.7.2），不要用。

## 易踩坑

1. **绝对路径硬编码**：Qt、GCC、CMake、Ninja、MSYS2 全写死；换机器必须改 `CMakeLists.txt` + `.vscode/settings.json`。
2. **6.7.2 套件是坏的**，误用会 `find_package(Qt6)` 失败。
3. **主题 JSON 双份清单**：只加一处会出现"开发机能跑、插件里找不到"。
4. **插件目录**：`Oran7UI/Impl/` 是构建产物目录，打包漏拷就白屏/起不来。
5. **`Oran7UI_Impl` 的 SOURCES 手写**：新导出类型忘了加，`import Oran7UI.Impl` 里就是没有。
6. **同一批文件编两次**：`QtQuicCppExtend` 同时进 EXE 与插件，符号/宏问题要在两份里一起验证。
7. **`resources` 前缀都是 `/`**：同名资源后注册者生效。
8. **没有测试/lint/CI**：验证方式只有"编译 + 手跑"。

## 待办

- [ ] 把工具链路径改为 `-D` 缓存变量（带默认值），去掉 `CMakeLists.txt` 里的绝对路径。
- [ ] `Oran7UI_Impl` 的 `SOURCES` 也改 glob + 显式排除，避免手写清单漂移。
- [ ] 合并双份主题资源注册。
- [ ] 把 `QtQuicCppExtend` 抽成独立静态库，EXE 与插件都链接，消除双重编译。
- [ ] 给纯逻辑模块（`Oran7LrcParser`、`WeapiCrypto`、主题表达式、生成器、`NcmJsonMapper`）加 CTest 单测。
- [ ] 加 `.clang-format`/`.clang-tidy` 与最小 CI（编译 + 单测）。
