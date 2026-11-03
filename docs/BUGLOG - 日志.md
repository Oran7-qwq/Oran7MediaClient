---
status: 📝 持续
updated: 2026-10-24
---

# Bug 日志与修复记录（BUGLOG）

> 记录「复现现象 → 根因 → 修复 → 验证」的完整链路。格式见下文模板。
> 原则：**每个 bug 一条记录**，按日期倒序；根因与踩坑写进对应模块文档或 ADR，这里只做日志索引。

## 使用方式

1. 复制下方「模板」到文件顶部 `## Bug 记录` 之后，填入内容。
2. 编号递增：`BUG-2026-001`。
3. 修复后把 `状态` 改为 `已修复`，补上 `验证` 行。
4. 若根因属于历史决策或易踩坑，另在 [[ADR/README]] 或 [[MODULES - 模块]] 对应模块记录一句话并双链回来。

## Bug 记录

<!-- 新记录加在下面这条模板之前 -->

### BUG-2026-001：抓屏"应用参数"无响应，`screenCaptureApplyFinished` 永不发射

- **日期**：2026-10-22
- **状态**：`已修复，用户已验收`
- **严重度**：中
- **现象**：打开"录屏设置"改 fps/显示器/鼠标开关后点应用，窗口一直停在"正在应用中"，既没有成功提示也没有回滚提示；抓屏失败时日志里也没有对应错误。
- **根因**：`Oran7MediaClient` 构造函数内部调用 `InitSignalsAndSlots()`（`src/cpp/Oran7MediaClient/Oran7MediaClient.cpp:95`），其中对 `m_screenCap` 建立了两个连接（`:204-211`）；但 `m_screenCap` 是在 `new Oran7MediaClient(...)` **返回之后**才由 `Oran7ApplicationContext::AppContext_CreateClientObject()` 赋值的（`src/cpp/GlobalSingleton/Oran7ApplicationContext.cpp:40-43`）。构造函数执行时 `m_screenCap == nullptr`，两次 `QObject::connect(nullptr, ...)` 静默失败（只有一条 "invalid nullptr" 告警），于是 `Oran7ScreenCaptureController::errorOccurred` / `captureApplyFinished` 永远到不了 `Oran7MediaClient`，QML 的 `Connections { target: Oran7MediaClient; onScreenCaptureApplyFinished }` 也就永远不会触发。
- **修复**：2026-10-24 已在 Oran7MediaClient 构造函数体内、InitSignalsAndSlots 前创建控制器（父对象为 this），并删掉 Oran7ApplicationContext 补建分支。退出先停止采集，Oran7MediaClient 析构体内销毁控制器。
- **验收**：用户于 2026-10-24 确认 P0 验收通过、无额外事故。原验收项：改 fps → 1s 内出现"抓屏参数已应用"；断开目标显示器再应用 → 出现"参数应用失败，已回滚"，且预览恢复；抓屏报错时 `ERROR_LOG` 有输出。
- **关联**：[[Capture - 录屏与推流]]、[[Core - 单例与胶水层]]、[[REFACTOR - 重构路线]] P0-1

---

### BUG-0000：<一句话标题>

- **日期**：2026-09-08
- **状态**：`待修复` / `已修复` / `无法复现`
- **严重度**：致命 / 高 / 中 / 低
- **现象**：
  <!-- 复现步骤 + 期望行为 + 实际行为，贴关键日志/截图 -->
- **根因**：
  <!-- 定位过程（断点、日志、Minidump）+ 真正的原因，附代码位置 -->
- **修复**：
  <!-- 改了什么，涉及文件 -->
- **验证**：
  <!-- 怎么确认修好了；如有回归用例写清命令 -->
- **关联**：
  <!-- 双链：模块 / ADR / 其它 bug -->

---

## 模板（复制用）

```
### BUG-2026-XXX：<一句话标题>

- **日期**：
- **状态**：待修复 / 已修复 / 无法复现
- **严重度**：致命 / 高 / 中 / 低
- **现象**：
  <!-- 复现步骤 + 期望 + 实际 -->
- **根因**：
  <!-- 定位过程 + 真正原因 + 代码位置 -->
- **修复**：
  <!-- 改了什么 -->
- **验证**：
  <!-- 如何确认修好 -->
- **关联**：
  <!-- 双链：模块 / ADR / 其它 bug -->
```

## 调试工具备忘

- 崩溃 minidump：`Oran7MediaClient_crash.dmp`（exe 同目录，全内存，可用 cdb/windbg 打开）。
- 日志：启动时附加控制台窗口「Oran7MediaClient Logs」。
- B站弹幕/网易云等网络问题优先查 **Cookie 与 WAF 通道**（详见 [[ADR/README]]）。

<!-- TODO: 把历史修过的 bug 从代码注释 / 聊天记录回填到这里 -->