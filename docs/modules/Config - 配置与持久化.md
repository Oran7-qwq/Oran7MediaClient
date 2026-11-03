---
status: ✅ P1-5 代码完成，运行待验收
updated: 2026-10-24
---

# Config：配置与持久化

配置入口为 `GlobalSingleton/Oran7AppConfigManager`；纯值结构、迁移与校验在 `src/cpp/Oran7Config/`。两者只编译进 Oran7Core，EXE 与 QML 插件共享一份配置存储。资源 `config/default.json` 是默认值唯一来源。

## 结构与权威

Manager 的归一化 JSON 是唯一内存存储。`snapshot()` 返回 `Oran7AppConfig` 值快照，业务修改后 `updateConfig(snapshot)` 提交；快照用于当前 GUI 调用，不长期缓存或跨异步任务写回。

| 分组 | 类型 | 当前使用方 |
|---|---|---|
| window | Oran7WindowConfig | ConfigBinding 的尺寸/位置保存与恢复 |
| appearance | Oran7AppearanceConfig | Theme 读取/修改 themeProfile |
| player | Oran7PlayerConfig | ConfigBinding 音量 |
| capture | Oran7ScreenCaptureConfig | 抓屏设置加载/保存，设备枚举仍在控制器 |
| auth | Oran7AuthConfig，内含网易云/B站分组 | CookieStore 与 BilibiliAuthManager |
| library | Oran7LocalLibraryConfig | 本地顺序、排序字段、整合列表 |
| lastMedia | Oran7LastMediaConfig | 最后音乐入口与元数据；播放源/VIP 字段保持原格式 |
| recentFiles / recentFilesMaxCount | 字符串列表/整数 | 最近文件 API |
| recording（P1-6） | Oran7RecordingConfig | 文件/WHIP 模式、输出参数、音频、ICE 与可选信令配置 |

新增类型均带 Oran7 前缀，不新增全局单例。旧 user 段和未知键仍保留在文档中；列表行沿用原 JSON，不在本阶段改写曲目持久化格式。ConfigBinding 不保存第二份音量/播放状态，仍只负责配置映射。

## 版本、迁移与默认值

- 根 `schemaVersion=2`。缺少版本视为 v0，接受 v1，归一化后升级 v2；旧版只有 last_used_file_isvip 时，补齐缺失的 last_media_isvip。迁移幂等。
- 沿用 window、playerVolume、last_media_*、local_music_sort、integratedMusic、netease_auth、bilibili_auth、screen_capture 等原键。`Oran7ConfigKeys.h` 集中维护持久化键，业务使用分组字段。
- 按 default.json 递归补齐缺项和错误类型；数组整体保留，绝不与默认数组按下标合并。保留未知根键/嵌套键。
- 数字字段要求整数。窗口宽高 1～32768，坐标允许负值；音量 0～100；抓屏 fps 1～240、输出索引非负；最近文件上限 1～50。越界回退 default.json，主题名为空也回退。最近文件数组过滤非字符串。
- 默认资源补齐网易云凭据、最近曲目信息、VIP 和空整合列表。移除 Theme/抓屏等调用点自带默认值及旧 validateConfig 的另一套默认值。
- 较新版本或非法版本字段：使用当前默认值运行，整个存储实例禁止写盘及重置覆盖，lastError 保留原因。缺失/不完整的默认资源也禁止生成用户配置。

## 存储与加载

路径继续为 `<exe>/../Config/config.json`。默认优先读 `:/config/default.json`，资源无法打开时沿用 Config/default.json 文件兜底；测试通过显式路径与同一默认文档创建独立 Manager，不调用应用单例。

`ins()` 首次构造即完成一次加载，使主题插件首次读取也能拿到用户配置。后续 `loadConfig()` 返回本次加载状态，不重复读盘；整合列表读写也不再重新 load，避免丢失待保存音量等内存修改。此 API 不用于监听用户外部编辑；修改磁盘文件后重启生效。

首次运行自动创建默认文件。普通字段在内存中迁移/修复，正常保存时写回；P2-3 发现旧明文凭据时会在启动加载期间先验证密文并立即迁移。JSON 损坏时使用默认值运行，原文件先保持不动；下次保存必须成功备份原字节后才能恢复。文件无法读取时禁止覆盖。

每个实例首次覆盖已有文件前，用唯一名称 `config.json.bak-<UTC时间>-<随机后缀>` 备份原字节；一次会话只备份一次，旧备份不自动删除。备份失败取消保存。写入使用 QSaveFile，完整写入并 commit 成功后才清除脏标记；失败保留内存修改供重试。不同于自动读取 .bak，本阶段只提供可人工恢复的原件备份。

## 接口与保存时机

| 方法 | 语义 |
|---|---|
| snapshot / updateConfig | 读取分组值快照 / 统一校验后置脏，不自动保存 |
| loadConfig / saveConfig | 单次加载 / 有脏数据时备份并原子保存 |
| lastError / isWritable | 最近读写错误 / 当前实例是否允许写盘 |
| getValueQVariant / setValueQVariant | 保留通用兼容入口，键来自常量；setter 仍走统一校验，无调用方默认值 |
| resetToDefaults / validateConfig | 使用同一默认文档重置 / 使用同一归一化规则校验 |
| 最近文件、最后入口、本地顺序旧接口 | 保留兼容；空本地顺序现在也可保存 |

播放元数据、整合列表、最近文件/最后入口、抓屏设置、登录/登出等仍沿用各模块原保存时机；音量和窗口几何在原退出流程保存。P2-3 后 B站 saveCookies 立即提交加密保存。抓屏应用/失败回滚未改变；P1-6 已将录制/推流启停独立异步化，见 [[Capture - 录屏与推流]]。窗口位置保存允许负坐标；P3-1 后按名称定位同引擎持有的主窗。普通几何由 Oran7WindowGeometryState 持有，C++ 读取主窗显式 rect 快照；关闭/保存边界补采集，最大化/最小化不覆盖普通尺寸，不再读设置样式单例。

## 验证与后续

独立 Release 检查使用真实 Oran7Core 配置实现、临时文件和无窗口 QML 对象，覆盖旧配置、深合并、类型/范围、未知键/列表/凭据保留、损坏/未来版本保护、备份/保存失败、待保存音量、曲目恢复与负坐标。主程序启动和重启恢复由用户验收。

P2-3 已实现 DPAPI 独立密文与原子引用提交；内存快照保留兼容字段，明文普通配置移除凭据，见 [[CREDENTIALS - 凭据加密与迁移]]。不会激活陈旧 TCP/Server，也不改用户旧 SQLite；P1-9 前置加载窗口仍只有计划。
