---
status: 已接受
date: 2026-10-22
related: [[Bilibili - B站接入]]
---

# ADR-0003：B 站弹幕必须匿名（uid=0）+ protover=3

> 状态：`已接受`（实测结论，实现于 `BilibiliDanmakuClient.cpp`）

## 背景

要让直播弹幕可用，需要依次过四道关：

1. **房间号**：用户输入的是短号，直接用短号认证会挂到空频道（`viewers=1`、只有系统消息）。
2. **`getDanmuInfo` 风控**：裸请求稳定返回 `-352`。
3. **WebSocket 认证**：用真实 uid 时 TLS 层被服务端掐断；`protover=2`（zlib）能连但弹幕被重度抽样。
4. **报文格式**：`protover=3` 用 brotli 压缩，且解压后仍可能是"多个 16 字节包头包"的嵌套结构。

## 决策

- 先 `room/v1/Room/room_init` 把短号换成真实 `room_id`；
- 先 GET `www.bilibili.com` 预热共享 cookie jar（拿 `buvid3/buvid4/b_nut/b_lsid`），再做 WBI 签名（`wts` + `w_rid`）请求 `getDanmuInfo`；
- **认证一律 `uid=0`（匿名）**，真实 uid 仅在握手失败后尝试一次并回退；
- **`protover=3`（brotli）**，项目内置 `3rdparty/brotli` 解码；
- 文本提取保留 4 套历史布局 + 递归兜底（`extra.content`/`content`）。

## 后果

### 正面

- 连接成功率与弹幕量最大化（实测 `protover=3` 比 `2` 多 3-5 倍）。
- 不依赖登录态也能看弹幕。

### 负面 / 代价

- 匿名连接被服务端**抽样**，实测 0-4 条/30s，这是服务端策略，无法在客户端绕过。
- 引入 `brotli` 构建依赖（项目因此 `LANGUAGES C CXX`），且只有 `BilibiliDanmakuClient.cpp` 需要 zlib。
- 整套协议是逆向结论，B 站随时可能变；WBI 置换表、文本布局都是维护热点。
- 需要"预热 + 签名"两段前置 HTTP，连接耗时会比理想情况高。

## 备选方案

1. **登录后弹幕**：理论上更稳定，但真实 uid 会被 TLS 掐断（客户端指纹），除非完整模拟浏览器指纹栈，成本极高。
2. **第三方弹幕服务/开放平台**：覆盖房间有限，且引入外部依赖。
3. **`protover=2`（zlib）**：实现简单，但抽样更重，体验差。

## 关联

- 连接流水线与踩坑：[[Bilibili - B站接入]] §2
- 抽成可单测的 `DanmakuParser`：[[REFACTOR - 重构路线]] P2-4
