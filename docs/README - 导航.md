# Oran7MediaClient 项目文档

> 本目录是 Obsidian vault 根目录（`docs/.obsidian` 即 vault 配置）。用 Obsidian 打开 `docs/` 文件夹即可浏览全部文档，支持双链、关系图谱、标签检索。

## 文档地图

| 文档                    | 内容                                                    | 状态     |
| --------------------- | ----------------------------------------------------- | ------ |
| [[ARCHITECTURE - 架构]] | 整体架构：运行时产物、依赖方向、启动流程                                  | 🚧 骨架  |
| [[MODULES - 模块]]      | 模块分工 + 各模块对外接口（原 INTERFACES 已并入）：C++ 模块、ncm 分层、QML 模块 | 📝 进行中 |
| [[RESOURCES - 资源]]    | 资源管理：qrc、themeJson 双份嵌入、shaders、运行时拷贝                 | 🚧 骨架  |
| [[BUGLOG - 日志]]       | Bug 日志与修复记录                                           | 🚧 骨架  |
| [[ADR/README]]        | 架构决策记录（ADR）索引                                         | 🚧 骨架  |

## 写作规范

1. 全仓库注释与用户可见文本为**中文**，本文档同样用中文。
2. 每个 `.md` 顶部写 Obsidian **properties**（frontmatter），至少含 `status`、`updated`。
3. 文档之间用 Obsidian 双链 `[[文件名]]` 引用，不用完整路径。
4. 指向源码用相对路径 + 行号，例如 `src/cpp/GlobalSingleton/ApplicationContext.cpp:20`。
5. 待办统一用 `<!-- TODO: ... -->` 标记，方便全局搜索。
6. 重要历史决策写进 [[ADR/README]]，不要在功能文档里长篇复述（用双链指向即可）。
7. C++↔QML 接口不单独成文：新增/改动 `qmlRegister*` 或 Oran7UI.Impl 导出类型，写进 [[MODULES]] 对应模块卡的「对外接口」。

## 待办（全局）

- [ ] 逐份文档填充正文并核对到最新代码
- [ ] ARCHITECTURE 补架构图（可用 Obsidian 绘图插件或 mermaid）
- [ ] 建立 git 仓库，方便多设备同步 Obsidian 库（当前非 git 仓库）