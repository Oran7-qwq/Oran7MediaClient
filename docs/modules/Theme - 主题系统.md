---
tags: [module, theme]
---

# Theme：主题系统

P2-5（2026-10-26）：保留 `Oran7UI.Impl` 的共享主题实例、URI 与只读 Map 接口；主题引擎及表达式计算仅在 `Oran7UI_Impl` 编译，profile 文件管理仍在 `Oran7Core`。本阶段不修改页面布局。

## 数据与类型

- `Oran7Theme` 拥有 Primary 与 9 个组件的 QVariantMap；组件 Map 只包含自己的 token，不复制整份 Primary。
- `Oran7ThemeEvaluator_p.h` 是内部计算器：原生 JSON bool、整数/实数和字符串保持类型，普通字符串保留空白。`#...` 为颜色表达式，结果是 QColor；`@name` 是引用；`$function(...)` 是函数，支持嵌套。
- 引用按依赖求值，不依赖 JSON 键排序；组件先解析本地引用，找不到时可引用 Primary。生成器输出的 `name-1` 等按需计算。
- 无效表达式/缺失引用/循环依赖有诊断：组件与 Index 均回退源默认值；没有有效回退的新增键不发布。深度限制 64、单表达式长度限制 8192。
- 原有带引号的数字字符串继续作为字符串，保持旧源文件兼容；新增原生数字不会被统一转字符串。

## 计算与通知

1. 构造临时结果，先解析 Index 默认值与 profile，再解析组件默认值与覆盖。
2. 比较结果的键、值及 QVariant 类型；只有变化的分组替换数据。
3. 全部分组先提交，再发通知，绑定不会因通知顺序读到一半新、一半旧的数据。

自有分组通过 `GROUP(Name)` 登记成员 Map 与编译期信号回调，删除旧字符串拼接/反射发射信号及无消费者的 custom-theme 注册 API、旧表结构。新增分组需要同时登记头文件属性、GROUP、Index 资源引用和 CMake 资源。

- `saveComponentToken` / `installComponentToken` 只计算指定组件；只有内容或类型变化才发该组 Changed。
- `saveIndexToken`、深色切换、reload 会重新计算所有组，但仅通知变化组。
- 粒度仍为组件 Map：修改其中一个键，会让依赖该 Map 的所有绑定重新求值；没有增加全局 revision 广播，也没有实现逐键通知。
- 同值 profile 保存跳过写盘。Map 通知与 profile 文件变更信号是不同层次。

## 深色模式

`Oran7Theme.darkMode` 是唯一权威，Light/Dark/System 为 0/1/2，保存在当前 profile 的 `darkMode`。System 读取 Qt 6.7.3 的 `QStyleHints::colorScheme` 并监听变化；Unknown 按浅色处理。有效深色未变化时不发 isDarkChanged。

设置开关和 `Oran7MainUiSetting.isDarkMode` 直接绑定 `Oran7Theme.isDark`。旧 `Oran7MainGUI.isDarkMode` 只保留派生读取兼容；旧键的保存入口转换为唯一模式 setter。默认采用原 GUI 的深色模式。旧 profile 的该布尔键迁移到根 darkMode；源 JSON 已移除这个状态及废弃的设置窗口开关。

## profile 差量与迁移

目录沿用 `<exe>/../Config`，文件名 `profile-*.json`，避免自动搬动用户文件。新文档：

```json
{
  "profileVersion": 2,
  "darkMode": 0,
  "__component__": {
    "Oran7ProgressSlider": { "__style__": { "moveThreshold": 5 } }
  }
}
```

- 源 JSON 始终提供默认值，profile 只保存不同的覆盖和未知扩展；空默认 profile 只有 profileVersion。
- 无版本旧全量 profile 首次加载时，移除与当前源默认值完全相同的项，保留差异及未知字段。旧文件不能表达“相同值是用户特意选择还是自动复制”，迁移将相同项解释为跟随默认值；原件先备份到 `.json.bak-<UUID>`。
- 迁移/保存用 QSaveFile 原子提交。备份或提交失败保留原件；本轮可用内存差量，后续重试。损坏/未来版本文档禁止覆盖，损坏文档不能通过切换入口选中。
- 保存全局变量写入正确的 __init__.__vars__，普通样式进入 __style__；加载时覆盖在首次计算前生效。
- install* 为临时调整，reload/切换清除；save* 为持久覆盖。切换不保留前一个 profile 的覆盖或临时值。
- 删除当前 profile 先成功切到默认 profile，再删除；默认 profile 禁止删除。复制/导入/导出使用差量，旧导入兼容迁移，未知格式拒绝。
- profile 版本与主 config.json 的 schemaVersion=2 是两个独立协议。

## 资源与所有权

主题源继续按既有 CMake 两个资源块嵌入（插件全量、EXE 子集），本阶段不合并资源。QML 工厂返回 CppOwnership，EXE 导入插件的同一实例，既有 post routine 在引擎销毁后清理。

## 验证

`tests/theme` 使用临时 profile、检查目录下的真实主题实例及离屏 QML，覆盖类型/空白往返、表达式与循环回退、分组通知计数、同值静默、类型变化、深色/系统模式、重载恢复、旧 profile 备份迁移、新默认键、导入导出及坏文件保护。真实插件实例/所有权继续由 qml-module 检查覆盖。

用户运行验收：深浅切换、颜色局部编辑、切换 profile、关闭重启恢复与旧 profile 首次迁移；本次不启动主程序或操作用户真实 profile。
