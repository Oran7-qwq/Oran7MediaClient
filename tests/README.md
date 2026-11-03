# Oran7 离线检查

构建、CTest 命令、路径覆盖和隔离边界见 [构建与验证](../docs/BUILD%20-%20构建与验证.md)。

在项目根目录执行 `cmake --preset checks`、`cmake --build --preset checks`、`ctest --preset checks`。检查源码不依赖 `build` 中的旧 fixture 或旧 Release 导入库。

默认检查不启动主程序或采集真实设备。`transport` 和 `recording/http` 仅绑定本机回环服务器，`recording/media` 使用临时合成音视频。设备相关性能与主 GUI 验收仍由用户独立执行。

现有 36 项检查；`credentials` 使用当前 Windows 用户的真实 DPAPI，但数据均为临时目录内的虚构凭据，不访问配置单例或用户登录数据。不同 purpose、损坏、提交中断和重新登录/退出路径可离线复现。`startup/background_checks` 用真实 QVideoSink 与合成 CPU 帧检查背景就绪/超时回退，不加载真实视频；软件场景图下仅验证底色/图片/透明图层像素，视频纹理与原生 DWM 留用户验收。

新增检查应使用 Oran7 前缀类型、虚构凭据及临时文件；在根 tests/CMakeLists.txt 中注册分组。禁止在默认检查中读取用户配置、连接真实账号或启动主程序。

`resources.qml_imports` 直接核对 qml.qrc 的成员与相对导入，目录必须在嵌入资源中有内容；仅在磁盘存在的空目录不能通过检查。

`theme` 通过真实插件与离屏 QML 验证类型、表达式、按变化分组通知、深色单源及 profile v2 迁移；文件管理使用临时目录，实际主题实例只读写检查二进制相邻的 tests/Config。

`localization` 使用临时配置、真实翻译器和实际 QML 设置控件，检查中英即时切换/恢复/保存失败回滚、目录与占位符覆盖、字体 fallback、录制草稿保持。软件截图只在 fixture 关闭 GPU 效果，真实 D3D11 和 DPI 仍由用户验收。

字号回归检查覆盖真实滑块鼠标提交和既有 QML Text 的实时字体通知；颜色检查覆盖方块点击、预设色选择、布局高度不变、隐藏/ESC 关闭及自定义取色窗口取消/确认后的焦点计数，保存离屏菜单截图。

`render-sync` 使用两个 WARP 软件设备、真实 QRhi 和生产渲染节点，检查合成帧像素、多 pass 单帧一致性、共享纹理/零超时忙槽、AVFrame 租约/代际、裁剪透明度与宿主设备订阅。仅该 fixture 显式使用 RHI；不打开主程序窗口或采集真实设备。

`capture-conversion` 复用生产 worker/转换器，使用 WARP 合成 BGRA 与 array slice 验证可见尺寸、三槽/克隆租约、跨设备零超时忙锁、尺寸重建与销毁后的旧帧。CPU 回退注入合成下载结果，仅替换硬件下载边界；后续缩放/NV12/色彩元数据使用生产实现。worker 检查在创建 DDA 前取消或失败，验证线程内销毁和 Feed 关闭，不抓桌面。真实 FFmpeg 硬件下载、NV12/P010/RGBA VideoProcessor 与吞吐仍需设备验收。

P3-2 的 capture-recovery 直接编译生产控制器，116 项断言覆盖非阻塞停止、慢驱动与慢析构、单会话交接、首帧确认、编号/替代/取消、迟到错误、磁盘失败、回滚失败、超时预算、渲染项销毁与最终 join。recording-workspace 增加编号与草稿保护/忙状态检查；不启动主程序或真实采集。

`startup` 直接使用生产 Oran7StartupCoordinator 与启动 QML，主窗口为隔离的最小软件窗口，不加载真实 main.qml/Client 或用户配置。68 项断言覆盖异步孵化错误、双窗交叉渐变/中断、唯一外部加载画面、活动条完整向右循环及接续可见长度、原生透明度预热/恢复与撤销、就绪撤销重新盖罩、首帧背景/遮罩/实际页面像素、配置门槛、同引擎持有的 load 启动窗与孵化主窗、早退/迟到/重复通知、超时及图形/QML/恢复失败；截图存入 startup_checks。config 同时验证具名 load 主窗与引擎 parent 持有的孵化主窗几何恢复。真实主程序 DWM 曝光与性能属于用户验收。

`startup/startup_cancel_checks` 实际进入隔离 app.exec()，覆盖启动窗显示、主界面加载、预热和错误状态下，按钮点击/原生 close 共 8 种退出路径。保留与生产相同的 queued quit 连接，防止只验证 cancel 信号却遗漏顶层窗口否决 Quit 的问题；不运行主程序或真实退出后台任务。

`startup/native_activity_checks` 使用隐藏原生父 HWND 与 WARP 调用生产 DirectComposition 后端，验证纹理/动画提交、停启、尺寸重建及父窗销毁，不使用硬件 GPU、不采集桌面。主线程暂停后只断言设备/提交有效，不把它当作可见动画流畅度证据；可见合成、DPI 和淡出效果由用户验收。离屏 QML 检查明确使用降级动画。

原生活动条检查补充 owned popup 的类名/owner、无激活/工具窗样式、client→screen 坐标与隐藏状态；不再假定子 HWND 能显示在 Qt 的透明合成树之上。底层 QML 条始终保留，不由 nativeActive 控制可见性。

配置检查使用 QGuiApplication + 离屏软件窗口加载生产 Oran7WindowGeometryState，覆盖启动禁写、延时更新、立即退出补采集、临时配置落盘、最大化保护及主窗隐藏前捕获高度；不加载真实主 GUI 或用户配置。

`startup/window_minimum_checks` 使用生产 StartupCoordinator 与 Oran7WindowAgent，原生 Windows 平台、软件渲染、零透明度最小窗口，验证首次建立 HWND 后 564/600/640 客户区高度保持不变，以及原生最小拖拽限制与 minimumHeight=564 一致。临时 QML 不加载主 GUI/用户配置，不采集桌面。另定向验证 QT_SCALE_FACTOR=1.25/1.5。此项专门覆盖纯 offscreen 平台无法复现的系统标题栏去除时序。
