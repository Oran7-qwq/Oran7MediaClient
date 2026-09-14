---
status: 🚧 骨架
updated: 2026-09-08
---

# 资源管理（RESOURCES）

> 本文回答：QML/图片/主题/着色器怎么嵌入、运行时依赖怎么拷贝、配置文件在哪。
> 铁律：**qrc 无 glob，新文件必须手动注册**。

## 1. 资源嵌入（qml.qrc / res.qrc）

- `qml.qrc`：全部应用 QML（`qrc:/main.qml` 入口）。
- `res.qrc`：图片（`image/`）、声音（`sound/`）、着色器源、`app.rc`。
- 新增 `.qml` / 图片 / 声音文件 → 手动加入对应 `.qrc`，否则运行时报资源缺失。

<!-- TODO: 补 qrc 目录前缀约定（prefix "/" 下 qml 放 src/qml/ 等） -->

## 2. themeJson 双份嵌入（易踩坑）

`config/themeJson/*.json` 在 `CMakeLists.txt` 里被 **两个 `qt_add_resources` 块**重复嵌入，均使用前缀 `/`，且清单**故意不同**：

| 资源块 | 归属 | 内容 |
|---|---|---|
| `qt_add_resources(Oran7UI_Impl ...)` | QML 插件 | **全量 9 个主题 JSON + default.json** |
| `qt_add_resources(Oran7MediaClient ...)` | EXE | 子集 5 个 + default.json |

- 新增主题 JSON → **至少加入插件块**；EXE 块按需加。
- `Oran7Theme::reloadTheme()` 先读 `:/config/themeJson/Index.json`，失败回退文件系统 `../Config/themeJson/Index.json`。

<!-- TODO: 列出两个块当前各含哪些文件（与 CMakeLists.txt 核对） -->

## 3. 着色器（shaders/）

- `qt_add_shaders` 编译为 QSB 资源：`video.vert` / `video.frag` / `gradientText.frag`。
- `bgra2rgba_*.hlsl` 是 D3D11 渲染器的**原始源**，不经 CMake 编译，仅打包进 res.qrc。

<!-- TODO: 每个 shader 用途（video 视频渲染 / gradientText 渐变文字） -->

## 4. 运行时依赖拷贝（POST_BUILD，仅 WIN32）

构建完成后自动复制到 exe 目录：

- `ffmpeg6.0/bin/` → exe 目录（FFmpeg + SDL2 DLL）
- `SQLite/` → exe 目录 `SQLite/`
- `C:/msys64/mingw64/bin/zlib1.dll` → exe 目录（B站弹幕 zlib）

<!-- TODO: 补 Release 打包时还需手动带上的东西（windeployqt、OpenSSL DLL、qt.conf、Oran7UI 插件目录）——已有 PACKAGING_GUIDE.md 可双链 -->

## 5. 配置文件布局

- 用户配置：`<exe>/../Config/config.json`（构建目录运行时即 `build/Config/config.json`）。
- 默认配置：嵌入资源 `:/config/default.json`。
- 主题配置档案：`profile-*.json`，存 QStandardPaths AppDataLocation。

<!-- TODO: 补 config/ 源码目录与运行时 Config/ 目录的关系、wallpaperResource 用途 -->

## 6. 源码目录中的资源源文件

- `config/`：`default.json`、`config.json`、`themeJson/`、`wallpaperResource/`
- `image/`、`sound/`、`shaders/`、`ffmpeg6.0/`、`SQLite/`

<!-- TODO: 全部核对一遍是否都被 qrc 覆盖，未覆盖的说明用途 -->