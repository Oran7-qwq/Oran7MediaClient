---
status: 已接受
date: 2026-10-22
related: [[Core - 单例与胶水层]], [[ARCHITECTURE - 架构]]
---

# ADR-0001：跨 EXE / QML 插件共享的单例必须下沉到 `Oran7Core`

> 状态：`已接受`（实现于 `CMakeLists.txt:146-157`）

## 背景

程序有两个 C++ 二进制：`Oran7MediaClient.exe` 与 QML 插件 `Oran7UI_Implplugin.dll`，后者由 `import Oran7UI.Impl` 加载。`src/cpp/QtQuicCppExtend/` 的源文件**同时被编进两者**。

`Oran7Async` 的线程池与"退出中"标志如果放在头文件的函数内 `static`，每个模块会各持一份：退出时 EXE 的池排空了，插件那份仍在等待工作线程 → `QWaitCondition: Destroyed while threads are still waiting`；日志/配置也会出现两份状态。

## 决策

把**必须全局唯一**的单例实现放进共享库 `Oran7Core`，通过 `WINDOWS_EXPORT_ALL_SYMBOLS` 导出；这些文件从 EXE 的 glob 中排除。当前成员：`Oran7GlobalHelper`、`Oran7Async`、`Oran7AppConfigManager`、`Oran7ThemeProfileManager`。

## 后果

### 正面

- 线程池、配置、主题 profile 在进程内唯一，退出时序可控。
- 新增共享单例有明确落点，不必再改链接结构。

### 负面 / 代价

- 每新增一个共享单例都要同时改两处（`ORAN7_CORE_SOURCES` + EXE 排除正则），漏了就静默变成两份。
- `Oran7Core` 只链接 `Qt6::Core/Gui/Qml`，依赖 Quick/Network 的单例放不进来。
- `Oran7Theme` **没有**遵守这条规则（仍在 `QtQuicCppExtend` 里双编译），导致它有两个实例、`cleanup()` 清错对象——这正是本决策的反例。

## 备选方案

1. **全部塞进一个巨型静态库**：链接简单，但 QML 插件必须是动态模块，静态库会被复制进每个二进制，问题更严重。
2. **用 `QSettings`/注册表做跨模块通信**：把状态外置，能绕过单例问题，但引入 I/O 与类型转换成本，且无法共享线程池。

## 关联

- 反例与修复：[[REFACTOR - 重构路线]] P0-7
- 运行时产物：[[ARCHITECTURE - 架构]] §1
