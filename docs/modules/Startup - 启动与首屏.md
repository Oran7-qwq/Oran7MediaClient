---
status: ✅ P3-1 首版代码/离线通过，真实启动待验收
updated: 2026-10-27
---

# Startup：启动窗口与首屏交接

Oran7StartupCoordinator 位于 src/cpp/Oran7Startup，仅 EXE 编译。由 main 独占并以 Oran7Startup 上下文属性借给 QML，不新增单例。持有者声明在引擎之前，保证 QML 引擎析构期间服务仍存活。启动窗仍由 ApplicationEngine.load 持有，主窗用异步孵化创建，Ready 后设引擎为 QObject parent 并标记 CppOwnership；预编译组件也由引擎持有，协调器以 QPointer 观察窗口。

## 阶段与交接

```text
ShowingSplash → 启动窗口真实首帧 → Initializing（服务/类型注册）
→ LoadingUi（异步编译主 QML，QQmlIncubator 在 GUI 分帧创建对象）
→ Restoring（隐藏/零透明度建立 HWND，完成自定义标题栏，再恢复配置/正常几何）
→ PreparingDisplay（主窗零透明度曝光预热，内部纯色保护遮罩）
→ 初始页面有效 + 配置已应用 + 最终背景就绪 + 遮罩首帧 → 撤遮罩
→ 真实页面提交下一帧 → 320ms 交叉渐变 → 再提交一帧
→ Ready（隐藏唯一的外部启动窗、激活主窗）
```

主窗 __Oran7Window__ 与启动窗 __Oran7StartupWindow__ 分别定位，Oran7ConfigBinding 优先查具名 rootObjects，再查引擎直属具名子对象，兼容手工孵化的主窗。它不假定第一个根对象是主窗。保留 D3D11/basic/透明窗口与 Windows Multimedia 后端；基本渲染循环下在 GUI 的帧提交边界交接，后续若更换线程渲染循环需重新审计这项契约。

Oran7StartupWindow/Surface 使用轻量内嵌 QML，无外部图片/网络/视频/主题插件依赖；品牌字体“华文琥珀”固定，其他字体/语言来自已恢复的 Oran7UiPreferences。加载画面仅存在于独立启动窗口，主窗不再重复放置 Surface。活动条优先由原生合成器持续向右，左侧副本接续绕回，循环保持可见长度；原生层不可用时使用 QML 降级动画，不提供虚构百分比或人为等待。主窗纯色保护遮罩与外部启动画面直接按窗口宽高绘制，不依赖首次曝光前可能为零的 contentItem 尺寸。

## 就绪、配置与退出

- Ready 不依赖曲库全扫、在线收藏、网络登录或抓屏首帧；已有业务继续后台执行。所选主窗背景在遮罩下准备，视频首帧最多等 5 秒，失败/超时固定回退图片（图片错误则保留主题底色）。
- main.qml/RightPage 汇总初始页面存在及尺寸；Component.onCompleted 不单独作为完成依据。
- 隐藏时完成正常窗口尺寸/位置恢复和 Oran7WindowGeometryState.initialize；首版支持按已有 maximized 值显示。启动期间禁用尺寸防抖保存，防止默认几何写回。
- 背景视频在 PreparingDisplay 开始播放，Oran7BackgroundFrameMonitor 观察真实 QVideoSink 的有效帧，不能用 hasVideo 元数据代替；切源重置代际，晚到旧回调不能放行。Oran7MainWindowBackground 只暴露选定图层，视频预热期间不显示图片，回退后不被迟到帧抢回。
- DWM 仅按主题 DWMBlurEffectEnabled 应用，不无条件开启；原生样式应用完成也参与就绪门控。图片/视频模式有不透明底色，DWM 模式保留透明。桌面歌词和下拉设置仍以 displayAllowed 门控。
- 背景或页面就绪条件变化会作废旧的遮罩帧；条件重新满足后必须再提交完整遮罩帧，然后撤罩、再提交实际帧才交接。
- 主窗 show 前保存目标 opacity 并设为 0，保持原生窗口曝光及场景图渲染；实际页面帧完成后以 320ms InOutCubic 从 0 淡入目标透明度。加载窗在主窗渐变达到 25% 后开始淡出，动画完成且主窗再提交一帧才隐藏加载窗、进入 Ready。就绪撤销会停止渐变，主窗归零、加载窗恢复不透明再重新预热。失败/取消同样停止动画，不能由迟到完成回调复活。该步骤保护 QML 首帧前的原生底色，不能用内部 QML 遮罩替代。
- 30 秒超时及 QML/服务/恢复/场景图错误进入错误终态，启动窗自身不可用则请求退出。关闭按钮调用外部窗口 close，onClosing 调用幂等 cancel 且接受关闭；禁止 event.accepted=false，避免 Qt Quit 请求关闭顶层窗时被反向否决。旧编译/帧回调不再复活窗口。成功后解除运行期帧观察。
- aboutToQuit 先取消启动/关闭 Client，再冻结业务任务；只有配置恢复且交接成功才保存业务及窗口状态，认证保存先于实例清理。启动中退出不因保存而创建新的认证实例。
- [Startup] 日志记录实际阶段及毫秒。主界面现在通过 QQmlIncubator::Asynchronous 与启动窗提供的 incubationController 在 GUI 分帧创建，8ms 定时器仅观察状态，不调用 processEvents 或 forceCompletion。失败/取消清除未完成孵化。此调整减少整棵 QML 对象树一次创建造成的活动条冻结；单个 C++ 构造、同步初始化及 Component.onCompleted 内长回调仍不可被孵化器抢占，不能保证任意同步操作下动画都不停顿。

## 验证

Release 构建、完整 32 项 CTest 通过（78.72 秒）；最终 startup 42 项断言验证软件离屏生产窗口、像素、同引擎多根对象、配置/页面/首帧门槛、重复/迟到通知、早退、超时、缺窗口与图形错误。配置测试覆盖启动根排在前面时，尺寸只应用到具名主窗。6 个 QML 文件语法解析通过。材料位于 build/refactor-work/P3-1/，startup.png 为隔离软件窗口截图。

未运行主程序、访问真实账号或采集设备。真实冷/热启动、透明窗口 DWM 首次曝光、最大化、多屏/负坐标、不同 DPI/主题与后台视频由用户验收。

2026-10-27 背景闪烁修正：以上背景门控替代首版“交接后才播放视频”的策略。新增 background_checks 使用临时图片及合成 CPU 视频帧，验证有效帧、旧代际拒绝、超时回退锁定及底色/图片/透明路由像素；startup_checks 补充就绪撤销时重新盖罩的路径。软件场景图不支持 QtMultimedia 视频纹理绘制，该部分及真实 DWM 交接由用户运行验收。材料见 build/refactor-work/P3-1-background/。

本次验证：Release 构建通过；完整 CTest **33/33** 通过（77.63 秒），其中 startup 44 项断言、background 29 项断言；主 QML 与新背景组件语法解析通过。

2026-10-27 原生呈现与活动条修正：Release 构建通过，完整 CTest 33/33 通过（87.46 秒），startup 52 项断言；覆盖只保留外部加载画面、完整活动条循环、零透明度预热/恢复及恢复中就绪撤销。3 个相关 QML 语法解析通过。未运行主程序，Windows 原生白屏是否消除仍需用户复验。材料见 build/refactor-work/P3-1-presentation/。

2026-10-27 平滑交接与分帧创建：Release 构建通过，完整 CTest 33/33 通过（80.44 秒），startup 66 项断言。检查新增双窗中间透明度、渐变中失败/取消、孵化 required 属性失败；config 覆盖引擎 parent 持有主窗的几何恢复。材料见 build/refactor-work/P3-1-smooth/。未启动主程序，实际渐变观感与加载条流畅度待用户验收。

2026-10-27 活动条方向调整：按用户要求改为一直向右，1600ms 线性循环，左侧副本接续绕回，避免整条出界后的空档。startup 67 项断言验证跨循环向右位移与可见长度；启动/背景/资源 3 项定向检查、QML 语法解析及 Release 构建通过。

2026-10-27 启动取消退出修正：旧 onClosing 在取消后仍拒绝 close，导致 queued QGuiApplication.quit 被顶层启动窗否决；Cancelled 又忽略重复取消，因此界面可悬停但不退出。X 现在走正常窗口 close，原生关闭事件接受并取消启动；不强制终止进程、不绕过后台资源清理。新增真实 app.exec 回归，旧实现 2 秒 watchdog 超时，修复后 8 种阶段/入口组合通过。材料见 build/refactor-work/P3-1-cancel/。

本次验证：Release 构建、完整 CTest 34/34（97.18 秒）、启动窗 QML 语法解析通过；取消检查仅运行隔离窗口的真实 app.exec，不运行主程序/真实后台任务，不读写用户配置。

## 原生合成活动条（2026-10-27）

仅靠分帧创建不能保证 QML 活动条连续刷新：basic 渲染循环仍与主线程初始化共享执行时间。Oran7StartupActivityItem 现在以无激活/无任务栏入口的 owned popup HWND 承载 Oran7StartupActivityBackend 的 DirectComposition 视觉树。1×1 BGRA 预乘表面经缩放形成底轨/短条，BeginDraw 更新严格使用返回的 atlas 偏移；1600ms 线性位移与 AddRepeat 一次提交后由 Windows 合成器运行，不需 GUI 定时器逐帧更新。左侧副本保持绕回接续；只在启停、布局或透明度改变时提交更新。依据见 [Microsoft DirectComposition 动画](https://learn.microsoft.com/en-us/windows/win32/directcomp/animation)。

它不使用 Qt QRhi/场景图设备，不接触播放/抓屏邮箱；主程序继续使用 D3D11/basic。资源由 unique_ptr/ComPtr 及 HWND RAII 管理，尺寸/DPI 变化重建，停止或隐藏停止动画，原生根效果同步加载窗透明度。原生初始化/提交失败或非 windows 平台记录失败/保持 QML 路径；底层 QML 活动条始终保留，不能把 Commit 成功等同于可见并隐藏它。离屏检查不会意外创建可见原生窗口或硬件设备。

这消除了活动条对 GUI 每帧刷新的依赖；加载文案、关闭按钮和窗口渐变本身仍由 GUI 线程处理，不承诺主线程被同步调用阻塞时也能立即响应输入。

本轮验证：Release 构建通过；完整 CTest 35/35 通过（82.68 秒）；startup 68 项断言、native activity 12 项断言；QML 语法解析通过。未启动主程序，原生层可见动画/DPI/淡出表现待用户验收，材料见 build/refactor-work/P3-1-compositor/。

2026-10-27 原生条不可见修正：原版在 nativeActive 后隐藏 QML 条，但接口成功不能证明该层在 Qt 透明窗口之上可见。现保留底层 QML 活动条，原生层改为 owned popup（不激活、无任务栏入口），坐标由宿主 client 转为 screen，跟随移动/隐藏/错误及取消。隐藏宿主和停止状态立即同步，避免原生层残留。新增 WARP 检查 owned 关系、窗口样式、屏幕坐标和隐藏状态；不把这些检查当作真实可见像素证明。材料见 build/refactor-work/P3-1-activity-visible/。

2026-10-27 主窗口高度持久化修正：普通几何从设置单例移到专用 Oran7WindowGeometryState，具名主窗只读暴露 normalWindowGeometry；ConfigBinding 保存边界请求 captureNormalGeometry 后读 rect，避免不同单例入口和未触发的防抖缓存。onClosing 在隐藏前采集，最大化/最小化/启动预热不覆盖普通尺寸。配置键与退出保存流程保持兼容。材料见 build/refactor-work/P3-window-geometry/。

本轮几何验证：Release 构建、完整 CTest 35/35（84.57 秒）、3 个 QML 语法解析通过；配置检查共 90 项断言。未运行主程序，实际高度恢复待用户验收。

## 最小高度重启后增高（2026-10-27）

本次复现确认 JSON 的高度读写本身正确。问题是 Oran7StartupCoordinator::attachMain 原先先调用 options.restore，随后 show 才首次创建 HWND。QWindowKit 接入 HWND 时在 WM_NCCALCSIZE 中移除系统标题栏，外框保持原尺寸而客户区增高。100% 缩放下隔离用例实测：最小拖拽客户区 564；恢复 564 后首次建立原生窗变为 595，即 +31，minimumHeight 本身仍为 564。

修正位于 attachMain 的 Restoring 分支：先在隐藏/零透明度下 window->create()，让 HWND/QWindowKit 标题栏初始化完成，再调用 restore 按客户区口径设置尺寸。配置键、最小高度 564 和持久化快照不变，不使用固定像素减法。QWindowKit 的标题栏实现无需修改。

新增原生软件窗口回归在修复前失败（564→595），修复后 564/600/640 首次显示保持原值，125%/150% 额外缩放定向检查通过。材料见 build/refactor-work/P3-minimum-height/；不启动主程序、不采集桌面或读取真实配置。

本轮原生高度验证：Release 构建通过，完整 CTest 36/36（86.34 秒），新增原生窗口 20 项断言及 125%/150% 定向检查通过。实际主程序重启体验仍由用户验收。
