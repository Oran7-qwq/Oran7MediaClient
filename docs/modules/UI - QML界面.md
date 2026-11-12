---
status: ✅ 已核对
updated: 2026-10-25
---

# UI：QML 界面层

> 源码 `src/qml/` + `main.qml`　资源 `qml.qrc`
> 全部 UI 与相当一部分业务策略。与 C++ 的接线有三类：直接 `import` 已注册 URI、QML 单例、组件间属性/信号。

## 目录结构

```
main.qml                          ApplicationWindow：透明 D3D11 窗口、背景视频/图片、标题栏、三栏布局、设置容器
src/qml/
├── Basic/                        qmldir + BasicConfig.qml（全局 QML 单例）
├── Components/
│   ├── Oran7GlobalComponents/    28 个通用组件（播放、歌词、弹幕、模糊、滑条、渲染宿主…）
│   └── Oran7SettingUiWindowItems/ 11 个设置行积木
├── LeftPage/                     左侧导航
├── RightPage/
│   ├── TopMenuBar/               BilibiliLoginPopup（SearchPopup 已于 P1-7 删除）
│   ├── MainStackView/            MainStackView + 4 个页面栈
│   └── RightPage.qml             渐变背景、pageManager、navigateTo
├── BottomPage/                   底部播放条 + 全屏歌词窗
└── Settings/
    ├── GlobalSettings/           Oran7MainUiSetting.qml（QML 单例）
    └── SettingWindows/           3 个设置面板 + Oran7SettingsPanel 动画基类 + 容器窗口
```

只有一个 `qmldir`（`src/qml/Basic/qmldir`），其余靠相对路径 `import "../../Basic"` —— 没有模块边界。

## QML 单例（状态集中点）

| 单例 | 注册方式 | 负责什么 |
|---|---|---|
| `BasicConfig` | `main.cpp:180` **和** `Basic/qmldir` | 播放状态（`isPlaying`）、播放模式、三张列表模型、当前歌曲元信息、播放归属、焦点链、页面枚举 |
| `Oran7MainUiSetting` | `main.cpp:181` **和** `qmldir` | 设置窗口开关/几何缓存/动画时长、`isDarkMode`（派生）、硬编码配色、`openVideoInfo` |

两者都是"双注册"（运行时 `qmlRegisterSingletonType` + `qmldir` 的 `pragma Singleton`），容易出现"两个实例"的错觉——实际是同一个类型、同一个实例，但 import 方式不同。

## 页面与路由

视频空间 V0 已于 2026-11-11 接入。VideoPlayerStack 保留路由入口，Oran7VideoLibraryPage 管浏览态，Oran7VideoPlaybackToolbar 管播放操作。视频渲染实例在浏览／播放切换和全屏期间复用，直播请求按对象和代际取消。阶段范围与验收见 [[VIDEO - 视频空间改版方案]]。

- 容器：`RightPage.qml` 内 `MainStackView`（`StackView`），`initialItem` 是**动态创建**的 `VideoPlayerStack`。
- 页面枚举：`BasicConfig.videoPlayerPage/localMusicPage/searchMusicPage/screenCapturePage` + `getUrl()`。
- `pageManager.navigateTo(url, name)`：先扫栈内同 `pageName` 的页 → `pop(existing, Immediate)`，否则 `push(Immediate)`。**没有转场动画，也没有真正的返回栈**；`popPage()` 定义了但无人调用。
- 导航入口只有左侧栏（`LeftPage` 的 4+2 个 `ListModel` 项）→ `BasicConfig.push*StackInto_RightPageMainStackView()`。
- P2-1 已删除空壳 `SettingStack.qml` 及其旧路由，设置由独立顶层窗口 `Oran7SettingsContainerWindow` 承担。
- P0 已消除上述隐式创建上下文依赖：RightPage 接收 captionExpanded，派生 musicPageActive；主窗口绑定底栏状态，BottomPage 使用 root.Window.window 读取窗口尺寸。不能仅凭跨文件引用断言一定发生 ReferenceError。

P1 首批：BasicConfig.isPlaying/playMode/playerVolume 已改为 Client.playback 的只读投影，页面通过 Oran7MediaClient 命令修改；歌曲元信息只读绑定 currentTrack；P1-7 后播放归属和队列由 Client.musicQueue 持有。

## 下拉设置窗口的开关时序

ESC 与左侧设置按钮统一进入 Oran7SettingsContainerWindow。容器负责 Closed → Preparing → Opening → Open → Closing → Closed，准备和动画期间不插入重复开关请求。打开前先在隐藏状态重置三个面板，再以窗口透明、内容隐藏的方式提交空白帧；收到 frameSwapped 且打开延时结束后才展示并启动展开。准备超时或外部取消会统一收尾，迟到定时器不能重新打开窗口。

三个面板继承 Oran7SettingsPanel，只动画 revealProgress，height 始终绑定 savedNormalHeight × revealProgress。关闭完成后进度、透明度和高度归零；容器等待全部面板 closed 后才隐藏原生窗口、解除忙状态和恢复主窗口焦点。子面板不再直接修改全局窗口开关，避免先完成的面板提前隐藏整个窗口。

2026-10-25 离屏检查通过 46 项：覆盖不同面板时长、ESC/按钮入口、重复开关、再次展开高度单调、零时长和准备中取消。使用真实容器和动画基类、替身面板内容与原生窗口代理；实际 Windows DWM/D3D11 视觉效果仍由用户启动验收。

录屏工作台及底部工具栏的颜色、字体、行高和开关尺寸由自身 property 提供，通过显式属性传递给设置积木；不读取 Oran7MainUiSetting 的样式。共享积木保留下拉设置的默认值，工作台调用时显式覆盖，避免下拉 GUI 配色影响录屏页。

## 播放相关状态在 QML 的分布

| 状态 | 位置 | 备注 |
|---|---|---|
| `isPlaying` | `BasicConfig.qml:71` | 只读投影 active，实际状态由 C++ 确认 |
| `playMode` | `BasicConfig.qml:76` | 只读绑定 C++ 的 0/1/2/3，队列策略由 Oran7PlaybackViewModel 持有 |
| 当前歌曲元信息 | `BasicConfig.currentMedia*` | 只读绑定 Oran7PlaybackState.currentTrack |
| 播放归属 | `Client.musicQueue.owner` | BasicConfig 只读投影，模型/trackId 在 C++ 持有 |
| 焦点 | `globalPlayingFocus` + `lastFocusedItem/currentFocusedItem` + `activeOtherItemCount` | 手写注册表 |
| 音量 | `playerVolume` | 只读绑定 C++，启动恢复立即同步 |

## 音乐意图与列表组件（P1-7）

LocalMusicStack/SearchMusicStack 只将点击与双击转给 Client.musicQueue.activate(model,index)，底栏/快捷键仍发 Client.playNext/playPrevious。Oran7PlaybackViewModel 统一归属、顺序/随机/单曲/漫游和失败推进；Client 组合曲目快照、歌词/配置及播放命令，QML 不维护播放队列副本。

Oran7MusicPlaylistView 保留公开属性/信号、索引同步和 ListView；显式注入 view/listView/reorderController 给拆分组件。页签/搜索/多选入口在 Oran7PlaylistHeader，行 UI 在 Oran7PlaylistDelegate，多选菜单在 Oran7PlaylistSelection，列标题在 Oran7PlaylistColumns，拖拽提示/自动滚动/排序算法在 Oran7PlaylistReorderController。当前没有独立排序弹窗，因此没有新建空壳 SortMenu。

收藏入口只调用 Client.cloudMusic.fetchLiked/toggleLike，BasicConfig 的收藏属性为只读投影，信号为兼容转发。搜索框的关键词及搜索请求代际属于搜索页；焦点、列表过滤和动画仍在 QML。

## 组件清单（按用途）

| 分组 | 组件 | 用途 |
|---|---|---|
| 窗口 | `Oran7CaptionBar` | 标题栏（最小化/最大化/关闭、拖拽、命中测试，基于 QWindowKit） |
| | `Oran7AnimatedWindow` | 开发者信息"展开窗口"动画载体 |
| | `Oran7BlurCard` | 多层模糊卡片 |
| | `Oran7GradientMask` | 渐变遮罩 + 内容 alpha |
| 播放 | `Oran7MusicPlayControls` | 底栏：封面/信息/上一首/播放/下一首/模式/进度/音量/岛屿开关 |
| | `Oran7MusicPlaylistView` | 约 560 行门面：页头、委托、多选、列标题、拖拽排序分别由 Oran7Playlist* 组件承载 |
| | `Oran7ProgressSlider` / `Oran7PlayerVolumeComponent` / `Oran7ValueSlider` | 进度/音量/通用滑条 |
| | `Oran7WaveBarChart` | "正在播放"三柱动画 |
| 歌词 | `Oran7LyricsView` / `Oran7LyricsIsland` / `Oran7LyricsFloatingWindow` / `Oran7ShaderTextMask` | 全屏歌词、灵动岛、桌面歌词、着色器文字 |
| 视频 | `Oran7VideoRendererItem` | D3D11 渲染宿主 + 视频信息浮层 |
| | `Oran7DanmakuLayer` | 多轨滚动弹幕 + SC 横幅 |
| 录屏 | `Oran7ScreenCaptureComponent` | 采集预览宿主 + 信息浮层 |
| 登录 | `Oran7NetEaseLoginPopup` / `BilibiliLoginPopup` | 两家登录弹窗 |
| 通用 | `Oran7RoundedImage` / `Oran7ColorPickerWindow` / `Oran7FileDialog` / `Oran7DelayTimer` / `Oran7AuthorBriefItem` | 图片、取色、文件、延时、作者简介 |
| 设置积木 | `Oran7SettingItem` / `Oran7ExpandItem` / `Oran7SwitchToggleItem` / `Oran7TextFieldItem` / `Oran7OptionSettingGroup` / `Oran7NumSettingGroup` / `Oran7ColorSettingGroup` / `Oran7ColorSelectItem` / `Oran7OpenFolderItem` / `Oran7SetingTitleItem` / `Oran7ColorCheckBox` | 设置面板的行级组件 |

清理结果：SearchPopup 已于 P1-7 删除；P2-1 删除两个空设置页、孤立取色示例和磁盘按钮副本。Oran7ESelectRect、Oran7Loader、Oran7NetEaseLoginButton 均有实际调用，保留；详见 [[REFACTOR - P2-1清理记录]]。

## QML 里承担的业务策略（重构点）

| 策略 | 位置 | 问题 |
|---|---|---|
| 播放决策链 | 5 处 `dealClicked` | 重复、易漂移 |
| 自动下一首/播放模式 | `BasicConfig.qml:83-93`、`LocalMusicStack.qml:320-343` | 模式由 Client.playback.mode 持有，队列推进仍在 QML |
| 网易云收藏缓存（30s TTL + dirty 标志） | `SearchMusicStack.qml:987-1002` | 缓存策略在 JS |
| 点赞冷却（2s）与乐观更新 | `SearchMusicStack.qml:37,577-600` | 失败无回滚 |
| 漫游队列预取 | `SearchMusicStack.qml:45-135` | 业务算法在 QML |
| Cookie 失效 → 登出跳转 | `main.qml:192-204` | 实际依赖无人发射的 `cookieExpired` |
| 焦点仲裁 | `BasicConfig` + `Oran7MainUiSetting` 引用计数 | 手写注册表 |

## 主题使用现状

- 取 token 用 `Oran7Theme.<组件>.<token>`（map 属性 + map 下标）。
- 颜色仍有组件本地样式；P2-6 已集中通用字体到应用偏好，品牌/装饰字体走主题角色。
- 跨组件取 token：播放列表用 `Oran7CaptionBar` 的 token。
- P2-6：本地 UI 文案使用共同 Oran7Ui 上下文的 qsTranslate，中英即时切换入口在主界面设置；用户内容与协议键不翻译。

## 待办

- [x] P1-7：C++ Oran7PlaybackViewModel 统一点击、队列和漫游，不再保留重复 dealClicked。
- [x] P1-7：BasicConfig 播放/曲库/收藏状态只读投影，保留 UI 瞬时状态与兼容转发。
- [x] P1-7：Oran7MusicPlaylistView 拆为门面及 Header/Delegate/Selection/Columns/ReorderController。
- [x] P1-7：删除 SearchPopup 及 qrc/注释入口；其余陈旧文件按 P2 范围另行处理。
- [ ] 修跨文件 id 引用（改用属性/信号传参）。
- [ ] 建立组件内 token 纪律：禁止硬编码颜色，禁止跨组件取 token。
- [x] P2-6：中英翻译资源/加载/持久化，通用字体偏好与主题艺术字体角色，含 fallback；真实 DPI/字体显示待用户验收。
- [ ] 给 `src/qml` 建 `qmldir` 与 `QT_QML_SINGLETON_TYPE`，摆脱相对路径 import。
