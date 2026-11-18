---
tags: [module, theme]
---

# Theme：主题系统

分阶段实施及离线试点见 [[THEME - 主题引擎与JSON组织重构方案]]。本文以下内容描述当前随应用加载的默认源、用户 profile 格式及运行行为。

`Oran7UI.Impl` 提供共享主题实例及只读 Map。主题引擎及表达式计算编入 `Oran7UI_Impl`，profile 文件管理编入 `Oran7Core`。

## 生产主题源

- `Index.json` 声明 v1 源版本、两个公共文件和九个已注册组件。清单中的资源路径相对于清单解析。
- `foundations.json` 定义字体、字号、圆角和动画时长。五个旧时长字段和进度滑块的 `moveThreshold` 沿用历史默认类型，同时允许写入有效的原生整数。
- `colors.json` 定义浅色及深色基色、生成色板和全局语义色。应用旧色值覆盖时，引用它的语义色会重新求值。
- 九份 `components/*.json` 各自声明 `__name__` 和令牌。音乐列表同时暴露当前模式的语义键和明确的 Light／Dark 别名。列表仍按列表背景亮度选取兼容色，以保留自定义背景的文字反差。
- `Oran7ThemeSchema.cpp` 登记必填字段、类型、数值范围和枚举。新源完整验证后再替换已发布值。自定义旧源仍可用 `installIndexTheme`、`installComponentTheme` 载入。

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

`Oran7Theme.darkMode` 是唯一权威。Light、Dark、System 分别为 0、1、2，保存在当前 profile 的 `__globals__.darkMode`。System 读取 Qt 6.7.3 的 `QStyleHints::colorScheme` 并监听变化，Unknown 按浅色处理。有效深色未变化时不发 isDarkChanged。

设置开关和 `Oran7MainUiSetting.isDarkMode` 直接绑定 `Oran7Theme.isDark`。旧 `Oran7MainGUI.isDarkMode` 继续作为派生值，旧键的保存入口调用同一个 setter。默认采用原 GUI 的深色模式。迁移优先读取旧根级 `darkMode`，缺省时读取组件的旧布尔值。

## profile 差量与迁移

目录沿用 `<exe>/../Config`，文件名为 `profile-*.json`。

```json
{
  "__profileVersion__": 3,
  "__globals__": { "darkMode": 0 },
  "__primary__": { "__tokens__": { "durationFast": 88 } },
  "__component__": {
    "Oran7ProgressSlider": { "__tokens__": { "moveThreshold": 5 } },
    "Oran7MusicPlaylistView": {
      "__modes__": { "dark": { "rowAccent": "#d3bb78" } }
    }
  }
}
```

- 求值时依次应用源默认值、profile 通用覆盖、当前模式覆盖和临时预览。保存会移除与实际继承值相同的覆盖，空 profile 只含 `__profileVersion__`。未知扩展随文档往返，求值只接受源声明的字段。
- 首次读取无版本全量和 v2 差量时，按旧 `__style__` 优先级和登记的模式键表迁移。旧原件先按字节备份为 `.json.bak-<SHA-256>`，然后由 QSaveFile 原子替换。内容相同的备份可用于迁移重试。
- 遇到损坏、不可读或未来版本的文档，保留原件。旧格式转换成功但备份或提交失败时，可继续使用内存差量。普通保存、重置和缺省字段继承不生成备份。
- `resetIndexToken`、`resetComponentToken` 和 `resetComponentTheme` 在写入成功后更新画面。默认重置通用覆盖和两个模式，传入 `currentModeOnly` 时仅删除当前模式覆盖。识别为旧模式别名的字段重置其明确指定的模式。`resetToDefaultTheme` 清空当前 profile 的覆盖和临时预览。
- 导入旧文档会创建 v3 差量副本，重名自动加后缀。导出和复制保留已规范化的覆盖与扩展。删除当前 profile 前先成功切换到默认 profile。
- profile 版本与主 config.json 的 schemaVersion=2 是两个独立协议。

## 资源与所有权

生产主题源由 QML 插件统一登记。共享核心中的 `legacy-profile-defaults.json` 冻结旧发布值，供无版本及 v2 profile 迁移使用。维护新版源时保留这一历史基准。QML 工厂返回 CppOwnership，既有 post routine 在引擎销毁后清理。

## 验证

`tests/theme` 使用临时配置，逐组比对新旧源两种模式的公开键、求值结果与 QVariant 类型，并检验源资源、用户覆盖、通知和无效值回退。旧 profile 迁移、故障保护及独立进程重启恢复继续由该检查覆盖。插件实例由 qml-module 检查覆盖，设置面板、列表、桌面歌词和录制工作台沿用各自的离屏检查。

用户运行验收：深浅切换、颜色局部编辑、切换 profile、关闭重启恢复与旧 profile 首次迁移；本次不启动主程序或操作用户真实 profile。
