# AGENTS.md

Windows-only Qt 6.7.3 (MinGW) C++17 + QML media player. Not a git repo — no commit/PR workflow. Comments and user-facing text are in Chinese; keep that style.

## Build
- CMake + Ninja, Qt Creator-style build dirs: `build/Desktop_Qt_6_7_3_MinGW_64_bit-{Debug,Release}`. Toolchain is hardcoded (CMakeLists.txt + `.vscode/settings.json`): Qt at `D:/Qt/6.7.3/mingw_64`, GCC 11.2 at `D:/Qt/Tools/mingw1120_64/bin`, CMake at `D:/Qt/Tools/CMake_64/bin/cmake.exe`, Ninja at `D:/Qt/Tools/Ninja/ninja.exe`.
- ⚠️ The Qt 6.7.2 kit (`D:/Qt/6.7.2/mingw_64`) is broken — it only contains HuskarUI/libmysql, no `qmake.exe` and no `lib/cmake/Qt6`, so `find_package(Qt6)` fails. Use the 6.7.3 kit and its `build/Desktop_Qt_6_7_3_MinGW_64_bit-*` dirs. Old `Desktop_Qt_6_7_2_*` build dirs are stale.
- Rebuild an existing dir: `D:/Qt/Tools/Ninja/ninja.exe -C build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug`. Fresh configure: `D:/Qt/Tools/CMake_64/bin/cmake.exe -S . -B build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug -G Ninja -DCMAKE_PREFIX_PATH=D:/Qt/6.7.3/mingw_64 -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1120_64/bin/gcc.exe -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1120_64/bin/g++.exe -DCMAKE_BUILD_TYPE=Debug`.
- ⚠️ **只编译 Release，不编译 Debug**（用户要求）：rebuild 用 `ninja.exe -C build/Desktop_Qt_6_7_3_MinGW_64_bit-Release`，fresh configure 把上面命令的 `-DCMAKE_BUILD_TYPE=Debug` 换成 `Release`、`-B build/Desktop_Qt_6_7_3_MinGW_64_bit-Release`。启动测试由用户自己跑，不要代跑冒烟测试。
- Parallel builds of the heavy `Oran7ScreenCapture` TUs can hit `cc1plus.exe: out of memory` — rerun with `ninja -j 2` to finish.
- No tests or lint. Verify by building and running the exe. POST_BUILD copies `ffmpeg6.0/bin` and `SQLite/` beside the exe automatically.

## Hard external deps (absolute paths in CMakeLists.txt; build fails without them)
- MSYS2 OpenSSL at `C:/msys64/mingw64` (headers/libs). Required for ncm weapi AES/RSA crypto and linked directly as `libwinpthread.a` + `libcrypto.dll.a`; `libssp.a` comes from Qt's MinGW. Only ncm crypto files get the OpenSSL include dir to avoid global header shadowing.
- MSYS2 zlib at `C:/msys64/mingw64` — linked as `libz.dll.a`; `zlib1.dll` is copied beside the exe by POST_BUILD. Only `BilibiliDanmakuClient.cpp` gets the MSYS2 include dir (zlib.h).
- Bundled: FFmpeg 6.0 in `ffmpeg6.0/` (also carries SDL2), `SQLite/` DLLs, `3rdparty/qwindowkit` (static QML-only, no widgets), `3rdparty/libdatachannel` (WHIP/WebRTC publish, subdir build), `3rdparty/qrcodegen` (nayuki QR, bundled), `3rdparty/brotli` (official google/brotli v1.1.0 decoder, ~10 .c files compiled into the EXE — CMakeLists is `LANGUAGES C CXX` because of them).
- The app links `libcrypto`/`libz` from MSYS2, so `C:/msys64/mingw64/bin` must be on PATH (or the DLLs beside the exe) at runtime.

## Code layout / wiring
- `src/cpp/GlobalSingleton/` — app singletons + `Client` (the main QML-facing object; `ApplicationContext::instance().client()`). Only `GlobalHelper`, `Oran7Async`, `AppJsonConfigManager`, `Oran7ThemeProfileManager` compile into the shared lib `Oran7Core` (one copy shared by EXE and QML plugin). Any new singleton that must be shared across EXE + plugin must be added to `ORAN7_CORE_SOURCES` in CMakeLists.txt. All other `src/cpp` files are globbed into the EXE (`CONFIGURE_DEPENDS` — new files are picked up on rebuild, no re-configure needed).
- `src/cpp/QtQuicCppExtend/` — classes exported to QML as module `Oran7UI.Impl` (`import Oran7UI.Impl`). New exported classes must be added to the `SOURCES` list of `qt_add_qml_module(Oran7UI_Impl ...)`. These files are ALSO globbed into the EXE, so they compile twice (EXE + plugin DLL) — expect duplicated-symbol/macro pitfalls and compile both copies when editing them.
- `src/ncm/` — NetEase Cloud Music client (crypto/network/protocol/api/session). QML singletons registered in `main.cpp` under URI `Oran7NetEaseAuth` (NcmSession, NcmQrLogin, NcmWebLogin, NcmPlaylistApi) and `Oran7MusicSearch`.
- `src/cpp/Oran7MediaPlayer/` — FFmpeg playback core (`FFPlayer`); `BilibiliDanmakuClient` (B站直播弹幕 WebSocket 客户端, registered in `main.cpp` as type `BilibiliDanmakuClient`); `Oran7VideoGPURender/` — D3D11 video rendering; `Oran7ScreenCapture/` — screen recording + WHIP publish; `Oran7Sever/` — legacy TCP server, NOT started from `main.cpp`; `Oran7Utils/` — shared defs.
- **B站弹幕客户端踩坑（`BilibiliDanmakuClient`，全部实测过，改动前先读）**：a) 用户输入的房间号要先经 `room/v1/Room/room_init` 解析成真实 room_id —— 用短号认证会挂到空频道（viewers=1、只有系统消息、无弹幕）；b) `getDanmuInfo` 会被风控返回 -352，必须**预热 cookie jar**（先 GET `www.bilibili.com` 收 buvid3/buvid4 等，共享 QNetworkCookieJar）+ **WBI 签名**（nav 的 wbi_img → mixin key → wts/w_rid，表在 `wbiMixinKey`）才能过；c) 认证必须用 **uid=0（匿名）** —— 真实 uid 会被服务器 TLS 掐断（B 站客户端指纹风控），且匿名连接弹幕被服务器**抽样**（实测 ~0-4 条/30s，这是服务端限制、无法绕过）；d) 认证 `protover=3`（brotli 满速）比 2（zlib，被抽样更狠）拿到的弹幕多 3-5 倍，用 `3rdparty/brotli` 解；e) 弹幕文本提取需递归兜底找 `extra.content`/`content`（格式随版本变，固定下标会漏）。QML 侧 `Oran7DanmakuLayer.qml` 渲染多轨道滚动弹幕。
- `BilibiliAuthManager` 提供 `dedeUserId()`（启动时从 nav 的 `data.mid` 捕获，登录 URL 里的 DedeUserID 常常是空的）。

## QML / resources
- All app QML is embedded via `qml.qrc` and loaded from `qrc:/main.qml`. New `.qml` files must be added to `qml.qrc` manually (no glob); same for images/sounds in `res.qrc`.
- Theme JSONs `config/themeJson/*.json` are embedded TWICE — two `qt_add_resources` blocks in CMakeLists.txt, both under prefix `/`. They deliberately DIFFER: the `Oran7UI_Impl` block carries the full list (9 theme JSONs + `default.json`), the EXE block only a subset (Index/MainGUI/CaptionBar/ProgressSlider/MusicPlaylistView + `default.json`). Add new theme JSONs to the plugin block (and to the EXE block only if needed); `:/config/themeJson/Index.json` is loaded by `Oran7Theme::reloadTheme()` with a `../Config/themeJson/` filesystem fallback.
- `shaders/` are compiled by `qt_add_shaders` (`video.vert/frag`, `gradientText.frag`). `bgra2rgba_*.hlsl` are raw sources for the D3D11 renderer, not compiled by CMake.
- QML plugin output dir `Oran7UI/Impl/` (with `Oran7UI_Implplugin.dll`) is generated inside the build dir.

## Runtime quirks (main.cpp)
- Forces Direct3D11 scene graph, `QSG_RENDER_LOOP=basic`, transparent window (default alpha buffer), GL 3.3 compat surface. Changing these can break the windowless/translucent UI.
- `QT_MEDIA_BACKEND=windows` (Media Foundation) is forced — must stay, the bundled FFmpeg DLLs clash with Qt Multimedia's ffmpeg plugin.
- Crashes write a full-memory minidump `Oran7MediaClient_crash.dmp` next to the exe.
- User config lives at `<exe>/../Config/config.json` (i.e. `build/Config/config.json` when run from the build dir); defaults come from embedded `:/config/default.json`. Theme profiles (`profile-*.json`) go to QStandardPaths AppDataLocation.

## Packaging / release
- **`oran7_pkt.bat`** is the current packaging script (replaces the stale `package_release.bat`/`.sh`, which still point at the broken 6.7.2 kit and are NOT updated). Usage: `oran7_pkt.bat -idir <build dir> -odir <output dir>` — `-idir` defaults to `build/Desktop_Qt_6_7_3_MinGW_64_bit-Release`, `-odir` to `C:\Users\funny\Desktop\Oran7MediaClient-Release\Oran7MediaClient`. Run without args to be prompted interactively for both paths (Enter accepts the default). If a previous run recorded its dirs in `oran7_pkt.ini` (beside the script), it asks `[Y/N]` to reuse them (Enter = Y). It runs windeployqt, copies MinGW/MSYS2 OpenSSL DLLs, writes `qt.conf` + README, and copies the `Oran7UI/` plugin dir. Qt path is fixed at `D:/Qt/6.7.3/mingw_64`. Manual walkthrough in `PACKAGING_GUIDE.md` (already updated to 6.7.3).
- The QML plugin `Oran7UI/Impl/Oran7UI_Implplugin.dll` is the most-forgotten piece — missing it means the app won't start. Full walkthrough in `PACKAGING_GUIDE.md`.
- **Launcher** `C:\Users\funny\Desktop\Oran7MediaClient-Launcher-build\Oran7MediaClient-Launcher.exe` launches the raw build-dir exe. It MUST prepend `D:/Qt/6.7.3/mingw_64/bin`, `D:/Qt/Tools/mingw1120_64/bin`, `C:/msys64/mingw64/bin`, and the exe dir to the child's PATH before `CreateProcessW` — the raw build dir has no Qt/MSYS2 DLLs beside the exe, so without PATH the app dies with `0xC0000135` (DLL not found). It falls back Release→Debug.