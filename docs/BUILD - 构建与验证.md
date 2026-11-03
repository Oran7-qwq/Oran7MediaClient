# 构建与离线验证

P2-4 于 2026-10-26 整理。Windows、Qt 6.7.3 MinGW、C++17 保持不变；只构建 Release，主程序启动验收由用户执行。

## 常用命令

以下命令从项目根目录执行，使用现有 CMake 3.30.5（preset 要求 CMake 3.21 及以上）。

```powershell
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' --preset release
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' --build --preset release
```

输出仍是 `build/Desktop_Qt_6_7_3_MinGW_64_bit-Release`，原 Qt Creator kit、Ninja 增量构建、启动器和打包输入目录保持兼容。已有配置目录首次选择 preset 时编译器不会被强制替换；更换编译器应使用新的 binaryDir。

离线检查采用独立 Release 目录，不构建或启动主程序：

```powershell
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' --preset checks
& 'D:/Qt/Tools/CMake_64/bin/cmake.exe' --build --preset checks
& 'D:/Qt/Tools/CMake_64/bin/ctest.exe' --preset checks
```

`BUILD_TESTING` 默认关闭。可在普通 Release 配置显式开启，也可直接用 checks preset。CTest 按组命名并提供标签，例如 `ctest --preset checks -L recording`、`ctest --preset checks --rerun-failed`。检查不会作为普通构建的自动运行步骤。

## 路径配置

`cmake/toolchains/Oran7MinGW.cmake` 提供编译器/Ninja 默认值；`cmake/Oran7Dependencies.cmake` 提供依赖 cache 参数，命令行 `-D变量=路径` 或本机 `CMakeUserPresets.json` 可覆盖。

| 变量 | 默认来源 |
|---|---|
| ORAN7_QT_ROOT | D:/Qt/6.7.3/mingw_64 |
| ORAN7_MINGW_ROOT | D:/Qt/Tools/mingw1120_64，供 toolchain 使用 |
| ORAN7_NINJA | D:/Qt/Tools/Ninja/ninja.exe |
| ORAN7_MSYS2_ROOT | C:/msys64/mingw64 |
| OPENSSL_ROOT_DIR / ORAN7_ZLIB_ROOT | ORAN7_MSYS2_ROOT |
| FFMPEG_ROOT_DIR / ORAN7_SQLITE_ROOT | 项目内 ffmpeg6.0 / SQLite |
| ORAN7_CRYPTO_LIBRARY / ORAN7_ZLIB_LIBRARY / ORAN7_WINPTHREAD_LIBRARY | 对应依赖根目录内的库 |
| ORAN7_ZLIB_RUNTIME | zlib 根目录内的 bin/zlib1.dll |
| ORAN7_SSP_LIBRARY | 从当前 C++ 编译器查询 libssp.a，可显式覆盖 |

preset 的 PATH 保留本机默认工具路径；替换 kit 时同步在本机 preset 或当前 shell 配置 PATH。CTest 运行脚本按实际构建目标和依赖根目录注入运行时路径，不依赖旧 Release 导入库。不要使用损坏的 Qt 6.7.2 kit。

OpenSSL/zlib include 仍仅应用于需要的源文件，不把 MSYS2 头文件全局加入编译路径。运行时依赖 POST_BUILD 的复制行为保持不变。

## 编译归属

- `Oran7Core`：配置、日志基础/异步池及主题 profile 等共享服务，EXE 与 QML 插件链接同一 DLL。
- `ORAN7_QML_SOURCES`：显式列出的导出类型只编译进 `Oran7UI_Impl`，不再同时进入 EXE；QML URI、注册名与 `Oran7UI/Impl` 输出位置不变。
- P2-4 未顺手删除旧辅助类；后续 P2-1 已核查并删除无调用/注册的 `FramelessWindow.*`，见 [[REFACTOR - P2-1清理记录]]。
- 主程序继续链接 libdatachannel 静态目标；同目录未使用的共享版本不再默认重复构建。WHIP 功能没有扩展。

## 检查范围与隔离

`tests/` 是可复用检查的正式源码位置，包含网络代际、播放状态/队列、曲目模型、NCM DTO/协议、本机 transport、曲库、歌词、解析器、配置迁移、渲染/采集桥、录制会话与 backend、真实软件编码、离屏工作台/歌单/下拉动画，真实 QML 插件加载，以及 P2-2 日志并发/滚动/脱敏、P2-3 DPAPI 凭据迁移与失败恢复。

默认 36 个 CTest 项（含 logging、credentials、启动背景门控与 QML 资源导入检查）使用替身设备、临时文件、离屏软件渲染和本机回环服务，不读取真实登录凭据、不连接网易云/B 站/真实 WHIP、不采集桌面或麦克风。配置/profile 如需由真实插件创建，仅在检查二进制相邻的 `tests/Config`；截图等位于检查 binaryDir 的 `tests/artifacts`，不写入源码树。

P2-7 新增 render-sync：通过两个 WARP 软件设备与实际 QRhi/生产节点验证合成纹理的渲染与同步，不创建可见主窗口、不使用硬件 GPU。真实设备性能检查、真实桌面切换与主程序启动属于独立设备验收，不纳入默认 CTest。旧 build 下的检查/备份保留作历史材料；不要将其作为正式源码依赖。旧模型夹具已按 P1-7 API 更新；旧录制弹窗检查由新版工作台检查替代，`videoinfo` 的历史 before.cpp 不是现行生产代码检查，未迁入。

`.clang-format` 规定四空格、Allman 大括号、保留 include 顺序；仅格式化本次触及或新增的文件，不批量重排全库。

## 打包

当前工作区实际脚本名为 `Oran7MediaClient-ReleasePackage.bat`，仍接受 `-idir/-odir`，默认输入目录不变。旧说明中的 `oran7_pkt.bat` 在此工作区不存在。P2-4 不执行会清空输出目录的打包脚本，不改用户发布目录。

打包必须保留主 EXE、Oran7Core.dll、Oran7UI_Impl.dll 和 `Oran7UI/Impl/Oran7UI_Implplugin.dll` 等已有依赖。可用现有部署流程；本次以产物/依赖检查验证路径兼容，实际发布包启动由用户验收。

P2-6 需要同 kit 的 Qt LinguistTools（lrelease/lupdate）；普通构建将 translations/oran7_en_US.ts 编译并嵌入 Core。显式目标 oran7_update_translations 只用于维护翻译目录；不会在默认构建时改 TS。

## Ninja 构建日志版本

现有 Release 缓存选择 Chocolatey Ninja 1.13.2（日志 v7），Qt 自带 Ninja 为 1.12.1。对该目录直接调用旧版 Ninja，即使只带 -n，也可能清空不兼容的构建日志。请使用 cmake --build --preset release/checks，由 CMake 按每个目录的 CMAKE_MAKE_PROGRAM 选择工具；不要混用 Ninja 版本。首次新配置仍可通过 ORAN7_NINJA 指定工具。
