---
status: ✅ 已核对
updated: 2026-10-24
---

# 资源管理（RESOURCES）

> 本文回答：QML/图片/主题/着色器怎么嵌入、运行时依赖怎么拷贝、配置文件在哪。
> 铁律：**qrc 没有 glob，新文件必须手动注册**。

## 1. 资源嵌入

| 文件 | 前缀 | 内容 | 入口 |
|---|---|---|---|
| `qml.qrc` | `/` | `main.qml` + `src/qml/**`（含 `src/qml/Basic/qmldir`） | `qrc:/main.qml` |
| `res.qrc` | `/` | `image/*`、`sound/*`、`app.rc`（P2-1 删除停用的 `bgra2rgba_*.hlsl`） | `qrc:/image/...`、`qrc:/sound/...` |
| `qt_add_resources`（CMake） | `/` | `config/themeJson/*.json` + `config/default.json` | `:/config/themeJson/...` |

- 新增 `.qml` / 图片 / 声音 → 手动加入对应 `.qrc`，否则运行时"找不到资源"（QML 加载失败时 `objectCreated` 回调会 `exit(-1)`）。
- 相对路径不会自动解析成 qrc，必须写 `qrc:/...`。
- P2-1 已删除不在 qrc、无入口的 `Oran7ColorPickerWindowExample.qml` 和 `WindowControlButton.qml`，并清理主窗口中未实例化的同名内联定义；标题栏实际使用的内联按钮保留。

## 2. 主题 JSON 双份嵌入（易踩坑）

`config/themeJson/*.json` 在 `CMakeLists.txt` 里被 **两个 `qt_add_resources` 块**重复嵌入，都用前缀 `/`：

| 资源块 | 归属 | 内容 |
|---|---|---|
| `qt_add_resources(Oran7UI_Impl ...)`（`:205-220`） | QML 插件 | `Index.json` + **9 个组件 JSON** + `default.json` |
| `qt_add_resources(Oran7MediaClient ...)`（`:286-296`） | EXE | `Index.json` + `Oran7MainGUI` / `Oran7CaptionBar` / `Oran7ProgressSlider` / `Oran7MusicPlaylistView` + `default.json` |

两份清单**故意不同**（历史原因），同一路径注册两次，后加载者生效。规则：

- 新增主题 JSON → **至少加进插件块**；只加 EXE 块会导致 `import Oran7UI.Impl` 的那份找不到文件。
- 修改 `Index.json` 的 `__component__` 映射 → 两个块都要能解析到目标文件。
- `Oran7Theme::reloadTheme()` 先读 `:/config/themeJson/Index.json`，失败才回退文件系统 `<exe>/../Config/themeJson/Index.json`。

## 3. 着色器

| 文件 | 构建方式 | 用途 |
|---|---|---|
| `shaders/video.vert` / `video.frag` | `qt_add_shaders` 编成 `.qsb`（前缀 `/`） | `D3D11VideoRenderNode` 的四边形采样：std140 `mvp/opacity/pad`，片元 `binding=1` 采样纹理 |
| `shaders/gradientText.frag` | 同上 | 渐变文字（`Oran7GradientMask` 等） |
| `shaders/bgra2rgba_vs.hlsl` / `bgra2rgba_ps.hlsl` | **不编译**，只作为原始源进 `res.qrc` | 采集侧的 BGRA→RGBA 全屏 blit；对应代码路径当前不可达（BGRA 走 `CopyResource`） |

新增着色器：普通 GLSL 放 `shaders/` 并加进 `qt_add_shaders`；需要运行期编译的原始源放 `res.qrc`。

## 4. 运行时依赖拷贝

POST_BUILD（仅 WIN32）自动复制：

- `ffmpeg6.0/bin/*.dll` → exe 目录（FFmpeg + SDL2）
- `SQLite/` → exe 目录下 `SQLite/`
- `C:/msys64/mingw64/bin/zlib1.dll` → exe 目录

打包时还需（`oran7_pkt.bat` 负责）：`windeployqt` 产物、MinGW 运行库、MSYS2 `libcrypto-3-x64.dll` / `libssl-3-x64.dll` / `libwinpthread-1.dll`、`qt.conf`、**`Oran7UI/Impl/` 插件目录**。

## 5. 配置与用户数据布局

```
<exe>/../Config/
├── config.json           # 用户配置（Oran7AppConfigManager）
├── default.json          # 可选：文件系统兜底默认值
├── profile-<name>.json   # 主题 profile（Oran7ThemeProfileManager）
└── themeJson/            # 可选：主题源文件回退目录
```

- 从构建目录运行时 = `build/Config/config.json`。
- P2-5：`profile-*.json` 为 profileVersion=2 差量，源 JSON 提供默认值；旧全量 profile 先备份再原子迁移。
- 默认配置 `config/default.json` 同时以资源 `:/config/default.json` 嵌入。

## 6. 第三方资源

| 目录 | 用在哪 | 是否需要随包分发 |
|---|---|---|
| `ffmpeg6.0/` | 解码/编码/采集/封装 + SDL2 | 是（DLL） |
| `SQLite/` | QSQLITE 驱动 | 是（DLL） |
| `3rdparty/qwindowkit` | 无边框窗口 | 否（静态链接进 EXE） |
| `3rdparty/libdatachannel` | WHIP 推流 | 否（静态） |
| `3rdparty/qrcodegen` | 网易云二维码 | 否（源码编入） |
| `3rdparty/brotli` | B 站弹幕解压 | 否（源码编入） |
| `C:/msys64/mingw64` | OpenSSL / zlib | 是（DLL 需拷到 exe 旁或加 PATH） |

## 待办

- [ ] 合并双份主题资源注册（只保留插件块，EXE 侧通过链接插件或用 `:/` 同路径即可）。
- [ ] `qml.qrc` 改为 CMake 生成（`file(GLOB ...)` + `qt_add_qml_module`），彻底避免手动登记漏项。
- [ ] 清理未进 qrc / 未使用的 QML 文件。
- [ ] 把 `bgra2rgba_*.hlsl` 与对应死代码一起删除。
- [ ] 给 `themeProfile` 加"源文件为默认 + profile 为差量"的加载策略，让改 JSON 能生效。
