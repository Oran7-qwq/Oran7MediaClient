# 界面语言与字体

## 通用基础设置（2026-11-12）

原“语言与字体”展开分组标题改为“通用基础设置”，保留语言、字体与基础字号，追加 `Oran7SwitchToggleItem` 开发者模式开关。`Oran7UiPreferences` 提供只读 developerMode、developerModeChanged 与 selectDeveloperMode(bool)，成功保存后才发布，失败恢复配置快照，开关恢复后端绑定。

`general.developerMode` 由 default.json 默认 false，归一化补齐缺键并拒绝非 bool 值；使用已有 schemaVersion=2，不改变 appearance 字段。运行期唯一原子开关位于 Oran7Core 的 GlobalHelper.cpp，EXE/插件及后台线程共享；仅 preferences 修改它，没有新增全局单例或 QML 副本。原 DEVELOPER_MODE 宏已删除，内存/播放诊断按运行时值读取，无需重启。偏好服务退出时清除运行状态，不覆盖已保存值。仅启用诊断，不改变播放策略、主题完整性校验、凭据保护或启动流程。

P2-6（2026-10-26）仅支持中文（zh_CN）与 English（en_US），默认中文。下拉式 GUI 的主界面设置（原 Oran7MainUiSettings）顶部的“通用基础设置”（原“语言与字体”）独立折叠组提供语言、通用字体、基础字号和开发者模式选择，修改即保存、即时生效，启动时先恢复偏好再创建主 QML。

## 所有权与配置

- main 栈对象 Oran7UiPreferences 拥有 QTranslator，借用 Oran7AppConfigManager，QPointer 观察 QQmlEngine；声明在 engine 之前，随引擎之后销毁，不新增全局单例。类实现仅 Oran7Core 编译；主程序通过 CppOwnership 上下文属性向 QML 借用。
- appearance.language、appearance.fontFamily、appearance.fontPixelSize 加入现有 schemaVersion=2；旧配置归一化自动从 default.json 补齐。默认 Microsoft YaHei UI / 16 px，不支持的语言回退默认，字号范围 10～24。凭据分离和未知字段保留策略不变。
- 只提交当前最新快照的偏好字段；保存失败恢复内存快照，保持当前翻译器/字体，并通过 lastError 显示错误，不先切换后丢失持久化。
- 使用稳定语言代码选择，显示标签为原生“中文”/“English”；语言不写主题 profile，不随着主题切换改变。

## 翻译机制

使用 Qt QTranslator + TS/QM。translations/oran7_en_US.ts 是英文译文来源；默认中文直接显示源文案。构建通过 Qt LinguistTools lrelease 编译 QM 并嵌入 Oran7Core 的 :/i18n/oran7_en_US.qm，不需要额外部署翻译文件。

QML 使用 qsTranslate("Oran7Ui", "中文原文")，切换语言安装/移除翻译器并调用 QQmlEngine::retranslate；主界面、设置、导航、列表、歌词状态、视频信息、登录与录屏工作台的本地 UI 文案采用共同上下文。协议/模式标识、页名、文件路径、曲目元数据、账号昵称和用户输入保持原值；日志/注释、操作系统原生对话框和外部服务返回的原始错误不强制翻译。

动态录制消息仍保存源文案，在底部状态显示绑定里翻译，状态比较不依赖当前语言。显示器元数据与显示标签分离，标签在绑定里重算，切换语言不刷新设备、不重置草稿、不触发参数保存。ListElement 导航保留稳定源文案，委托在显示时翻译。

缺少译文时 Qt 返回源文案；缺少英文资源时保留中文并报告错误。动态后端状态的翻译只匹配已登记的完整本地文案；FFmpeg/Windows/网络返回的诊断原文保留，不对拼接后的外部错误内容猜测翻译。

## 字体入口

通用界面通过带 fontChanged 通知的 Oran7UiPreferences.font 使用偏好中的字体及 fallback（Microsoft YaHei UI、Microsoft YaHei、Segoe UI）。缺少配置字体时选择已安装的后备字体，不自动覆盖磁盘上的原选择，保留以后恢复字体的可能。

下拉设置与录屏工作台的基础字号默认跟随应用字体；已有明确指定的组件字号、歌词主题字号和视觉层级继续保留。LeftPage 的 Oran柒 / MediaClient 品牌标题固定使用“华文琥珀”，不跟随主题与通用字体；其他装饰文字保留现有主题字体角色。

## 维护

1. 新增本地 UI 文案用 qsTranslate("Oran7Ui", "中文原文")，动态状态文案登记 translations/Oran7UiMessages.h；不要翻译模式值或用户数据。
2. 执行 `cmake --build --preset release --target oran7_update_translations` 更新 TS。该目标显式调用才改源文件，普通构建只编译已有 TS。
3. 在 Qt Linguist 或编辑器中填写英文并完成条目；保留参数、单位、换行和通配符。不要运行 build/refactor-work 下的一次性迁移脚本。
4. 构建 release / checks 并运行 CTest。localization 检查验证 TS、占位符及实际 QML 静态翻译入口覆盖，真实语言选择框、重启恢复、保存失败回滚、字体 fallback 和录制草稿不变。

## 验收与边界

检查仅使用临时配置、检查目录下的主题 profile、替身录制服务和离屏窗口，不启动主程序，不采集设备，不连接真实账号。设置截图的 fixture 关闭软件后端不支持的 GPU 阴影/渐变，保留实际控件和布局；不据此宣称 D3D11 效果已运行验收。

用户验证：中文/English 即时切换与重启恢复、不同 DPI 和窄窗口下的文案、真实中文/英文及缺字回退、字体/字号选择。底部录制操作英文使用短标签 Record，维持现有布局。

## 2026-10-26 设置外观补充

语言/字体/基础字号封装为 Oran7UiPreferencesGroup，内部复用 Oran7SettingItem + Oran7ExpandItem，默认收起。两个选择器使用 Oran7PreferenceChoice：整行卡片、悬停/焦点/选中反馈、滚动列表、字体实际字形预览，以及 160～210 ms 淡入缩放、110 ms 关闭动画。弹层挂到窗口 Overlay，按可用空间调整位置和宽度，避免被折叠区裁切。

基础字号使用已有 Oran7NumSettingGroup/Oran7ValueSlider，10～24 整数，松开提交保存，保存失败恢复显示标签。弹层计入设置焦点状态，收起组或隐藏控件时关闭；ESC 优先关闭当前偏好菜单，同次按键不会再切换整个设置容器。箭头/选中标记用几何图形绘制，避免字体缺少符号时出现方框。

相关离线检查增加实际鼠标选择、滑块提交、展开内容边界、弹层打开/收起/焦点计数、ESC 优先级及中英截图；未启动主程序。

## 字号即时更新与颜色菜单补充

Qt 6.7.3 的 Qt.application.font 在 QML 中标记为 CONSTANT，不能用于动态字号绑定。界面改为读取偏好服务的 QFont 通知属性，基础字号松开滑块即可更新已有文字，无需重建界面或重启。隔离组件没有偏好服务时才回退到 Qt.application.font。

Oran7ColorSettingGroup 的方形预览现在通过 Oran7ColorSelectItem 复用 Oran7PreferenceChoice 弹层，菜单首项为“自定义”，随后提供鸢尾紫、晴空蓝、靛青蓝、冰川青、薄荷绿、青柠绿、琥珀金、落日橙、珊瑚红和樱花粉十种固定预设；每项显示色块、可翻译名称和色值，保留手动输入。内部主题色阶生成机制不变，已有主题/自定义颜色不自动替换。每次打开颜色菜单从首项展示。浮层不改变设置组高度，支持选中反馈、滚动、淡入缩放和点击外部/ESC 关闭，隐藏设置组同步关闭。设置菜单统一登记当前句柄，ESC 优先关闭浮层。取色窗口自行完成关闭动画并释放焦点计数。
