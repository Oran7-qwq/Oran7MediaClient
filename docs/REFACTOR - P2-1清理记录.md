# P2-1 死代码与资源清理记录

实施日期：2026-10-26。范围为重构路线 P2-1 待核查清单；不将“没有直接 C++ 调用”作为唯一删除依据。

## 删除对象与依据

| 对象 | 核查结果与处理 |
|---|---|
| `QtQuicCppExtend/FramelessWindow.h/.cpp` | 无创建、include 或有效注册；main.cpp 仅残留注释注册，主窗口实际使用 Oran7WindowAgent。删除两文件与注释注册；CMake glob 自动移除编译条目。 |
| `GlobalSingleton/Oran7WindowApi.h/.cpp` | 仅自身定义，没有调用；不在 QML 模块 SOURCES，也没有运行期注册。删除两文件；保留正式导出的 Oran7Api。 |
| `Oran7MediaPlayer/ImageScaler.h` | 无 include、初始化或调用；解码器仅遗留前向声明与一直为空的成员。删除文件及这两处残留，现行缩放仍使用已有 FFmpeg 路径。 |
| `SettingStack.qml` | 空页面；只有 BasicConfig 的旧页面枚举/URL，无导航调用。删除文件、qrc 条目、旧枚举/URL 和未读写的 isSettingStack；设置入口仍进入 Oran7SettingsContainerWindow。 |
| `Oran7MediaPlayerSetting.qml` | 空 Item，无创建和引用，仅占用 qrc。删除文件与资源条目。 |
| `Oran7ColorPickerWindowExample.qml` | 孤立示例，未嵌入资源、无路由或动态创建入口。删除文件，实际取色窗口保留。 |
| 磁盘 `WindowControlButton.qml` 与 main.qml 内联同名定义 | 磁盘版本不在 qrc，无引用；主窗口内联定义也无实例。删除两份；Oran7CaptionBar 内自用的定义及三个实际按钮保留。 |
| `blitBgraToRgbaShared/ensureSwizzlePipeline/ensureTmpBgra` | 唯一入口已被注释；删除函数、声明、专属临时纹理与着色器状态。 |
| `vpBgraToNv12Shared` | 唯一调用位于第二个 BGRA 判断；前一个完全相同的条件已拷贝并 return，第二分支不可达。删除该分支与函数/声明。 |
| `ensureEncodeNv12Tex` | 无调用。仅删除函数/声明，保留正在使用的 m_encodeNv12Tex 及 downloadGpuNv12 完整链路。 |
| `bgra2rgba_vs.hlsl/bgra2rgba_ps.hlsl` | 只由上述停用的 swizzle 路径加载；删除文件及 res.qrc 条目，同时移除其专用资源读取/HLSL 编译辅助函数、头文件依赖与主程序不再需要的直接 d3dcompiler 链接。 |

共删除 11 个文件。修改前副本与删除清单位于 `build/refactor-work/P2-1/`，不写入应用资源或发布包。

## 核查后保留

- `Oran7Api`：正式 `Oran7UI.Impl` QML 单例，具有 invokable 窗口 API；P2-4 的实际插件检查覆盖类型和所有权，保留公开接口。
- `Oran7WindowAgent` 与标题栏内联按钮：主窗口正在使用的窗口控制链路。
- `Oran7Loader`：VideoPlayerStack 使用；`Oran7ESelectRect`：Oran7PlaylistDelegate 使用；`Oran7NetEaseLoginButton`：SearchMusicStack 使用。旧文档的未使用清单不适用于现行源码。
- 现行 BGRA/RGBA 预览拷贝、NV12/P010 VideoProcessor 预览转换，以及 downloadGpuNv12 和软件转换回退均保留；未调整色彩、帧率、码率、纹理同步或设备生命周期。
- `Oran7RecordingSettingsPopup`：按此前约定保留源码和资源，包含暂缓的推流设置；工作台仍不打开它。
- TCP/Server 继续陈旧停用，WHIP 不扩展；不删除第三方源码、SQLite 数据、用户配置、录像和历史 build 材料。
- D3D11、basic 渲染循环和 Windows Qt Multimedia 后端不变。

## 验证

核对 C++ 引用、运行期注册、QML 导出清单、页面路由、动态创建字符串、qrc、CMake 与检查工程。以 Release 构建、24 项正式离线 CTest 及清理后的引用/资源完整性核查作为自动验证；实际窗口、播放器、下拉设置和录屏由用户启动验收，不代跑主程序。

结果：主程序 Release 与检查工程构建成功，24/24 项 CTest 全部通过（64.09 秒）；Release 再次增量构建显示 `ninja: no work to do`。qrc 无缺失文件，当前 src/qml 下所有 QML 均已嵌入资源；删除对象无残留源码引用，已删除的两个 C++ 实现不再出现在编译数据库。日志和静态核查结果见 `build/refactor-work/P2-1/ctest.log`、`release-final.log`、`release-incremental.log` 与 `audit.json`。

## 运行验收补修（2026-10-26）

用户启动发现 Oran7SettingsContainerWindow 仍导入已清空的 PagesSettings 目录。已移除这条无效导入；先前文件名核查与文件系统动画夹具未覆盖 qrc 目录边界，现新增资源相对导入 CTest，并用磁盘空目录负向夹具确认检查能够拒绝。Release 构建、资源检查和设置窗口动画检查通过，未代跑主程序。
