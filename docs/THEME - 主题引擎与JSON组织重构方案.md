---
status: ✅ T0–T5 已实施
updated: 2026-11-18
tags: [theme, refactor, plan]
---

# 主题引擎与 JSON 组织重构方案

## 1. 决策与范围

本方案整理 `Oran7Theme` 的职责、主题源文件结构、明暗模式合成与旧 profile 兼容。生产主题现已采用分层 v1 源和 v3 profile。实际窗口观感由用户验收。

暂不将此前筛查出的直播工作台、视频空间、账号弹窗等模块批量接入主题，也不重新设计页面外观。先改善已有主题引擎及文件规范，再决定后续模块的接入范围。

已确认的接口原则：

- 全局通用的外观基础属性，如 `darkMode`、只读派生的 `isDark`、动画总开关，直接由 `Oran7Theme` 暴露 `Q_PROPERTY`，不为它们另建组件组。
- 可独立分类的模块外观，通过 `ORAN7_PROPERTY_READONLY(QVariantMap, Oran7MusicLyricsIsland)` 形式暴露，并配套默认 JSON、`GROUP`、Index 登记及 CMake 资源。
- 子组件可以接受所属模块显式传入的样式参数，不要求每个 QML 文件单独建立主题 Map。
- 模块主题统一复用既有求值、变化通知、profile 差量保存及原子提交链路，不另建配置存储器。
- 新主题源和新 profile 的根级结构键统一采用 `__key__` 命名，延续现有双下划线习惯；保留 `__component__` 作为组件清单／覆盖集合的名称。
- 编码码率、录制帧率、音效增益、认证信息等归业务配置。窗口坐标、锁定、鼠标穿透等归窗口偏好／状态，不作为新主题字段。

全局字体与字号当前由 `Oran7UiPreferences` 管理，不能在本轮增加第二个权威。其与主题字体令牌的长期分工另行明确；先保持通用字体实时更新、语言独立保存及品牌字体隔离。

## 2. 当前实现与主要问题

### 2.1 重构前可沿用的基础

- `Oran7Theme` 已有 `Primary` 与 9 个组件 Map，组件只输出自身 token，不复制全部全局值。
- `Oran7ThemeEvaluator` 按引用依赖求值，支持生成器、有限函数、无效表达式回退和深度限制。
- 结果先统一提交，再仅通知实际变化的分组；通知粒度仍为 Map。
- `Oran7ThemeProfileManager` 已有 profileVersion=2、差量保存、同值跳过写盘、迁移及坏文件保护。
- `darkMode` 已统一为 Light／Dark／System，`isDark` 是有效状态投影。

以上能力保留，不重新实现另一套主题引擎。

### 2.2 组织问题

| 问题 | 当前依据 | 维护影响 |
|---|---|---|
| 变量与样式区边界模糊 | `tokens()` 将 `__init__.__vars__` 与 `__style__` 合并，后者覆盖同名键 | 保存时需要反查字段所在区，同名项可能遮蔽新值 |
| 明暗模式表达不统一 | 全局 `浅色\|深色` 字符串、组件 Light／Dark 后缀、QML 条件分支同时存在 | 默认值重复，页面需要了解主题内部结构 |
| 色阶承担过多语义 | 页面直接消费 `colorPrimaryBase-6` 等编号 | 难判断颜色用途，换一个基础色容易破坏文字／背景搭配 |
| Index 职责集中 | 同时含基础变量、大量派生值、字体角色及组件资源索引 | 修改公共值、模块定义和资源登记混在同一文件 |
| 外观与窗口偏好混存 | 灵动岛 JSON 包含颜色、字号，也包含坐标、启用、锁定和穿透 | 切换外观 profile 可能连带改变窗口使用习惯 |
| 字段校验不足 | 已有 JSON／版本／表达式检查，缺少完整的组件字段类型与范围约束 | 拼写错误、错误类型或无效范围难在写入边界定位 |

相关源码：`src/cpp/QtQuicCppExtend/Oran7Theme.cpp`、`Oran7ThemeEvaluator_p.h`、`src/cpp/GlobalSingleton/Oran7ThemeProfileManager.cpp`，以及 `config/themeJson/`。

## 3. 目标文件分层

```text
config/themeJson/
├─ Index.json                    # 源格式版本、全局文件引用、组件清单
├─ foundations.json              # 公共间距、圆角、动画等基础令牌
├─ colors.json                   # 全局语义色及明暗定义
└─ components/
   ├─ Oran7MainGUI.json
   ├─ Oran7CaptionBar.json
   ├─ Oran7MusicPlaylistView.json
   └─ ...                       # 先迁移已有组件，不扩张接入范围
```

目录只是组织手段，核心是下述职责和格式统一：

1. **Index** 只描述源文件组成，不继续堆叠样式值；组件引用仍与现有注册 Map 一一对应。
2. **Foundations** 提供公共设计值；它们通过既有 `Primary` 供模块引用。全局控制属性仍直接暴露在 `Oran7Theme`，不因文件拆分新增组件 Map。
3. **Colors** 定义颜色用途，例如 `textPrimary`、`textSecondary`、`surfacePanel`、`borderSubtle`、`accent`，集中处理明暗值。
4. **Components** 描述模块自己的外观，既可引用全局值，也可保留模块专用色彩，避免所有模块被迫使用同一强调色。
5. **用户 profile** 是对默认源的差量覆盖，继续位于现有 Config 目录；不把默认源复制到每个 profile，不自动迁移存储目录。

皮肤的素材与样式预设可在后续使用模块覆盖层组织，本轮不额外引入皮肤继承系统或批量移动现有素材。

## 4. 新源格式建议

以下 JSON 展示当前解析器使用的源结构。完整的生产字段、引用关系及现有配色以 `config/themeJson/` 中的资源为准。

命名约定：根级键统一使用双下划线前后包围，例如 `__schemaVersion__`、`__tokens__`、`__modes__`。嵌套的结构容器也沿用同样规则，使 profile 中的组件覆盖与独立组件文件保持一致；模式名 `light`／`dark`、组件名以及 `rowTextColor` 等实际属性名保持原样，不增加双下划线。`__...__` 为文档结构保留命名，不作为普通样式 token 发布。

本约定只约束本方案的新主题格式，不修改现有主 `config.json`，也不要求直接改写尚未迁移的旧 profile。

### 4.1 Index：资源清单

```json
{
  "__schemaVersion__": 1,
  "__foundations__": ":/config/themeJson/foundations.json",
  "__colors__": ":/config/themeJson/colors.json",
  "__component__": {
    "Oran7MusicPlaylistView": ":/config/themeJson/components/Oran7MusicPlaylistView.json"
  }
}
```

示例只列一个组件，正式迁移必须保留全部既有注册组。组件名称必须与 C++ Map、`GROUP` 和资源对应；源文件的 `__name__` 标识必须与 `__component__` 清单中的名称一致。

### 4.2 全局文件：基础值与语义色

`foundations.json` 示例：

```json
{
  "__schemaVersion__": 1,
  "__tokens__": {
    "radiusSmall": 6,
    "durationFast": 150,
    "durationNormal": 250
  }
}
```

`colors.json` 示例：

```json
{
  "__schemaVersion__": 1,
  "__tokens__": {
    "accent": "#6979bc"
  },
  "__modes__": {
    "light": {
      "textPrimary": "#24304d",
      "textSecondary": "#65708a",
      "surfacePanel": "#f4f6fa",
      "borderSubtle": "#dfe4ed"
    },
    "dark": {
      "textPrimary": "#e7edf5",
      "textSecondary": "#a3afbf",
      "surfacePanel": "#1e252f",
      "borderSubtle": "#334154"
    }
  }
}
```

公共令牌由明确的字段清单校验；全局文件之间不允许靠加载先后意外覆盖同名字段。`darkMode` 的保存位置属于 profile 全局控制项，不混入 Colors 的色值定义。

### 4.3 组件：统一 `__tokens__` 与 `__modes__`

```json
{
  "__schemaVersion__": 1,
  "__name__": "Oran7MusicPlaylistView",
  "__tokens__": {
    "listViewFontPixelSize": 16,
    "rowAccentColor": "@accent"
  },
  "__modes__": {
    "light": {
      "rowTextColor": "@textPrimary",
      "rowBackgroundColor": "#ede7db"
    },
    "dark": {
      "rowTextColor": "@textPrimary",
      "rowBackgroundColor": "#353a40"
    }
  }
}
```

- `__schemaVersion__`、`__name__` 属于元数据，不发布到 QML Map。
- `__tokens__` 表示两种模式共用的默认值；`__modes__.light/dark` 只写需要改变的字段。
- 同一字段可以有公共默认值和模式覆盖，这是显式优先级，不再使用变量区／样式区隐式竞争。
- 第一版保持字段为扁平键值；不同时引入多层 `colors.text.primary` 路径解释器。
- 颜色、尺寸、透明度等仍可使用受限引用／函数；生成色阶可保留，但新界面优先消费用途明确的颜色字段。
- 原生数字和布尔保持类型，不再新增数字字符串。图片 URL、枚举字符串及允许为空的自定义项，按对应字段语义处理。
- 新源不再使用 `浅色|深色` 或要求页面自己从 Light／Dark 后缀中选择。旧写法由兼容适配器读取。

QML 继续使用既有 Map 访问方式，例如 `Oran7Theme.Oran7MusicPlaylistView.rowTextColor`。旧公开键及其语义不得在格式迁移时直接删除或擅自改义。

## 5. 合成、引用和发布规则

### 5.1 优先级

必须分别归一化默认源和用户覆盖，再合成，避免默认 `__modes__.dark` 反过来覆盖用户写入公共 `__tokens__` 的值。

```text
当前默认值 = 默认 __tokens__ + 当前模式的默认覆盖
当前用户覆盖 = profile __tokens__ + 当前模式的 profile 覆盖
最终原始值 = 当前默认值 + 当前用户覆盖 + 临时预览覆盖
最终运行值 = 对最终原始值统一求值、校验及兼容别名输出
```

全局基础值先组成 `Primary`；组件以本地值为先、`Primary` 为引用来源。第一版不新增任意跨组件引用，避免扩大依赖图和重算边界。

组件引用的全局语义色应在最终合成后求值，不将派生结果固化进 profile。Light／Dark／System 仍由全局属性决定有效模式，不在组件中复制 `isDark` 状态。

### 5.2 通知与保存

- 保留结果键、值、类型比较，以及先提交全部快照再通知的机制。
- 局部编辑只更新相应组件；全局值／明暗变化可以重算相关组，但只通知结果发生变化的组。
- 继续保持 Map 粒度，不在本轮改成逐键 QObject 或全局 revision 广播。
- `install*` 仍是临时预览；reload／切换清除。`save*` 保存成功后再发布持久状态，失败保留旧快照并反馈错误。
- 实施时保留现有调用入口，通过内部适配转换新文档；不要求所有设置页面同步改写保存 API。
- 提供恢复单字段、整个组件默认值的明确入口：删除相应覆盖后重新求值，不把当前默认值复制成固定覆盖。公共恢复与仅恢复当前模式需语义明确。

## 6. profile 与版本兼容

### 6.1 三种版本独立

| 文档 | 当前／建议版本 | 含义 |
|---|---|---|
| 主 `config.json` | 现有 schemaVersion=2，保持不变 | 业务及通用偏好配置 |
| 主题默认源 | 当前 `__schemaVersion__=1`，安装接口继续接受无版本旧源 | 主题源格式 |
| 用户主题 profile | 当前 `__profileVersion__=3`，兼容旧 `profileVersion=2` 和无版本全量 | 用户差量覆盖格式 |

版本号是不同协议，不因数字相同或不同相互推断兼容性。

目标 profile 示例：

```json
{
  "__profileVersion__": 3,
  "__globals__": {
    "darkMode": 2
  },
  "__component__": {
    "Oran7MusicPlaylistView": {
      "__tokens__": {
        "listViewFontPixelSize": 18
      },
      "__modes__": {
        "dark": {
          "rowTextColor": "#ffffff"
        }
      }
    }
  }
}
```

全局设计令牌的用户覆盖使用独立 `__primary__.__tokens__`／`__primary__.__modes__` 节点；`__globals__` 只包含类级基础属性，二者不混用。此示例省略空节点。

### 6.2 迁移原则

1. 新旧源先转换到统一的内部表示，再沿用求值与发布流程；新增的项目自有辅助类统一使用 `Oran7` 前缀。
2. 无版本旧全量 profile、现有 v2 差量 profile 均需支持；转换保留字段的实际覆盖语义，不根据当前显示颜色猜测用户意图。
3. 旧 `__vars__`／`__style__` 的同名冲突按旧读取优先级处理，不能在迁移中反转。旧 Light／Dark 后缀需要显式字段映射，不能只凭字符串后缀批量猜测。
   旧根键 `profileVersion`、`darkMode` 按版本显式映射为 `__profileVersion__`、`__globals__.darkMode`；`__component__` 集合名保留，集合内部转换为 `__tokens__`／`__modes__`。不得通过给所有键批量添加下划线来迁移，以免改动实际属性名或未知扩展。
4. 迁移首先保持当前观感、QVariant 类型和重启恢复结果；新的语义命名通过别名逐步提供，不直接替换所有旧键。
5. 旧有“覆盖值等于默认值则裁掉”的差量语义继续兼容；不承诺从历史文件恢复“用户特意固定同值”的意图，也不在首版引入固定值标记协议。
6. 仅格式迁移时按现有策略备份旧文件，用 QSaveFile 原子提交；普通保存、缺省字段继承不生成备份。
7. 损坏、不可读及未来版本文件禁止自动覆盖。未知扩展保留用于往返，但不未经校验直接发布成新主题字段。
8. 导入、导出、复制和重置统一使用同一规范化／校验链路，避免导入形成旁路。

### 6.3 外观与窗口偏好的边界

本轮先记录灵动岛坐标、锁定、鼠标穿透、启用与置顶等遗留项，不在试点时顺带搬迁。后续需要单独设计一次性迁移和唯一权威：用户偏好已有值优先，主题切换不反复覆盖；旧键只读兼容，不双向同步两份状态。

音效 DSP、录制／推流参数继续使用业务配置。启动加载窗口的轻量独立外观、原品牌字体与用户明确保留的阴影实现不纳入本轮改造。

## 7. 字段规范与诊断

- 采用 schema 或显式字段描述声明类型、必填／可选、枚举、有限数值范围及允许的表达式结果类型。范围按组件实际契约登记，不凭字段名统一猜测。
- 明确颜色值与资源 URL／普通字符串的区别，不以所有字符串都能转成 QColor 为前提。
- 默认缺键由源继承；未知键、拼写错误应有诊断。保留未知扩展不等于执行未知字段。
- 诊断至少含源文件、组件名、字段路径、原因和所用回退，例如 `components/Oran7MusicPlaylistView.json / __modes__.dark.rowTextColor：颜色引用无效，保留默认值`。
- 保留循环引用、深度、长度和非有限数值检查，错误默认源不能清空已发布的有效主题；启动期无旧快照时使用已验证的内置默认回退。
- 错误用户字段回退到相应默认值，并保留原文件便于修复；不能靠“保存时删掉错误数据”掩盖问题。
- 首版可用离线 schema 检查配合运行时字段校验，不要求引入大型第三方 schema 库。

## 8. 分阶段实施

| 阶段 | 内容 | 交付／退出条件 |
|---|---|---|
| T0 | 冻结既有字段、明暗结果、通知及保存契约；建立旧键映射表 | 分清外观／偏好／业务字段，记录兼容基线 |
| T1 | 新源结构读取、类型校验与内部归一化；继续兼容旧源 | 新旧格式可并存，既有页面无需同步改写 |
| T2 | 选一个现有模块试点，优先音乐列表；同批补足其最小公共令牌依赖 | 新结构读写／明暗切换闭环，当前视觉与旧键行为等价 |
| T3 | profile v3 差量与旧 profile 迁移，统一恢复默认／导入导出 | 临时目录中迁移、失败保护与重启恢复通过 |
| T4 | 分批迁移其余已有 9 组中的模块，并完成 Index／公共源拆分 | 每批保持接口、类型、实际外观和资源完整性 |
| T5 | 清理确认无消费者的旧格式分支，更新规范 | 旧用户文件仍可迁移；未接入的新模块另行排期 |

T1／T2 可先在隔离测试使用新格式，T3 完成前不把新 profile 当作默认写入格式。实际批次可合并，但不得先写入尚无完整迁移保障的新文件格式。

### 实施记录

- **T0** 冻结已发布的 `Primary` 和 9 个组件 Map、QVariant 类型、明暗值、差量保存和分组通知。旧源先按原区合成，`__style__` 覆盖同名 `__vars__`。Index 的 `colorTextBase`、`colorBgBase` 按原明暗顺序读取，`Oran7MainGUI.isDarkMode` 保留全局状态的只读投影。
- **T1** 插件内新增源适配层，读取分拆的 v1 Index 或旧 Index，并允许两种组件源共存。新源按明确的字段、类型和范围校验，候选源全部校验通过后才切换。用户覆盖在现有默认值上求值，错误值回退，未知扩展留在 profile。
- **T2** 音乐列表试点显式映射七个 `row*Base` 色值和七个行样式令牌的 Light／Dark 兼容键。保留 `rowBackgroundColor`、可为空的 `rowTextColor`、`rowAccentColor` 和 7～20 像素的字号。试点源已进入生产资源，检查直接读取该资源。
- **T3** 用户 profile 保存为 v3 差量。无版本和 v2 文件在首次读取时转换，原件按内容摘要备份后再原子替换。导入、导出、复制和恢复默认共用规范化和原子写入。离线检查在临时目录覆盖两类旧格式、备份和提交失败、重试及独立进程重启恢复。
- **T4** 生产资源拆为 Index、基础令牌、颜色和九个组件，字段校验覆盖全部分组。明暗对照检查逐项保留旧公开键、求值结果与 QVariant 类型。新增语义色沿用现有色值，工作台皮肤 ID 与桌面歌词偏好继续由各现有入口读取。
- **T5** 生产主题统一由 QML 插件嵌入，移除了旧默认组件的重复副本、EXE 中的主题子集和重复试点夹具。共享核心冻结旧默认值供无版本和 v2 profile 迁移，公开安装接口继续接受自定义旧源及旧内置组件路径。

### 已登记的业务与窗口边界

| 现有字段 | 兼容基线 |
|---|---|
| `Primary.fontFamilyBase`、`fontSizeBase`、字体生成序列 | 保留各旧主题消费者，通用界面字体和字号继续由 `Oran7UiPreferences` 管理 |
| `Oran7ScreenCapture.workspaceSkin` | 保留实际配置值（例如 `violet` 对应「雪松幽境」）和当前皮肤 |
| `Oran7VideoPlayer.ScaleMode`、`Oran7ScreenCapture.ScaleMode` | 保留各页显示偏好及保存接口 |
| `floatingEnabled`、`floatingLocked`、`floatingClickThrough`、`floatingAlwaysOnTop`、`floatingPositionVersion`、`floatingCenterX`、`floatingX/Y` | 遗留窗口状态，保留读取和保存位置，单独排期迁移 |
| 录制码率／帧率、推流码、均衡器增益 | 继续由各业务配置管理 |

## 9. 验收标准

- [x] 类级全局属性与模块 Map 分工符合本方案，`isDark` 由 `Oran7Theme` 暴露。
- [x] 新主题源／profile 的根键及结构容器统一为 `__key__` 命名，保留兼容的属性名、模式名及 QML 接口。
- [x] Index、C++ 属性、`GROUP`、源文件标识和 CMake 资源一一对应，完整默认源由插件嵌入。
- [x] 旧源与新源等价输入得到相同最终值、类型及兼容键，引用按依赖求值。
- [x] 用户公共覆盖优先于默认模式值，模式专用覆盖仅影响对应模式。
- [x] 字段修改、全局值变化、Light／Dark／System 切换后的通知范围正确，同值不重复通知。
- [x] 默认引用、局部同名、缺失／循环引用、错误类型、越界值及未知扩展行为有隔离检查。
- [x] 临时预览、保存、字段／组件恢复默认、profile 切换和重启恢复可往返。
- [x] 无版本旧全量、v2 差量、v3、新增默认字段、坏文件／未来版本、写入失败和迁移备份失败均在临时目录检查，不读取真实用户 profile。
- [x] 保留 `tests/theme`、`tests/qml-module`、资源及必要页面离屏检查，使用 Release 配置。
- [ ] 主程序的真实主题切换和窗口观感由用户运行验收。

## 10. 关联文档

- [[Theme - 主题系统]]：当前已实现行为，以源码为准。
- [[Config - 配置与持久化]]：业务配置与文件保护边界。
- [[LOCALIZATION - 界面语言与字体]]：通用字体、语言和品牌字体边界。
- [[RUNTIME_STATE - 运行时状态]]：状态与偏好的唯一权威。
- [[UI - 直播工作台皮肤设计]]：已验收皮肤，格式迁移不附带改版。
