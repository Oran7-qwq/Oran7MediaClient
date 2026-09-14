---
status: 🚧 骨架
updated: 2026-09-08
---

# 架构决策记录（ADR）

> ADR（Architecture Decision Record）：每条记录一个**影响深远、事后容易忘了为什么**的决策。格式见 `0000-template.md`。编号递增，永不改号；状态可选 `提议 / 已接受 / 已否决 / 已取代`。

## 写作要点

- 只记录「当时为什么这么选」——代码里看不到的东西。
- 每条独立文件：`docs/ADR/NNNN-简短标题.md`。
- 正文包含：背景 → 决策 → 后果 → 备选方案 → 关联（双链）。
- 写完后在本文件「决策清单」表中登记一行。

## 决策清单

| 编号 | 标题 | 状态 | 日期 |
|---|---|---|---|
| 0000 | （模板，勿填） | - | - |
| 0001 | 匿名单例拆分进 Oran7Core 共享库 | 待写 | - |
| 0002 | ncm 走双 HTTP 通道（系统 curl.exe / Schannel） | 待写 | - |
| 0003 | B站弹幕强制 uid=0 匿名 + protover=3（brotli） | 待写 | - |
| 0004 | 强制 QT_MEDIA_BACKEND=windows（Media Foundation） | 待写 | - |
| 0005 | D3D11 场景图 + QSG_RENDER_LOOP=basic + GL 3.3 透明窗口 | 待写 | - |
| 0006 | 迁移 Qt 6.7.3 工具链，废弃 6.7.2 | 待写 | - |
| 0007 | MSYS2 OpenSSL / zlib 作为外部依赖（libwinpthread/libssp 混链） | 待写 | - |
| 0008 | B站弹幕文本递归提取（extra.content/content 兜底） | 待写 | - |
| 0009 | 全局崩溃 minidump 机制 | 待写 | - |
| 0010 | Oran7Sever 遗留 TCP 服务弃用 | 待写 | - |

> ⚠️ 决策清单里的「待写」条目只是提示。写 ADR 前先核对当前代码，确认决策是否仍成立、是否已被取代。

## 建议新增决策候选

- QtQuicCppExtend 文件双重编译（EXE + 插件）的代价与规避
- 网易云登录/加密链路为什么必须本地实现而不是用第三方库
- 本地媒体库（SQLite userLocal.db）与播放列表排序存储方案

## 模板

见 [[0000-template]]。复制 `0000-template.md` 为新文件并填空即可。