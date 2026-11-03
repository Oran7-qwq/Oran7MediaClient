---
status: ✅ 已核对
updated: 2026-10-24
---

# Bilibili：B 站接入

> 源码：`src/cpp/GlobalSingleton/BilibiliAuthManager.*`（认证）
> 　　　`src/cpp/Oran7MediaPlayer/BilibiliDanmakuClient.*`（弹幕）
> 　　　`src/cpp/Oran7MediaPlayer/BilibliLiveRoomAddressCatch.*`（直播流地址）
> 三者互相独立，只共享"Cookie 字符串"和"登录态"。

## 1. 认证（`Oran7BilibiliAuthManager`，QML `BilibiliAuth`）

### 状态

| 成员 | 语义 |
|---|---|
| `isLoggedIn` | 启动时 nav 校验通过或登录成功 |
| `userName` | nav 的 `data.uname` |
| `qrCodeUrl` | 登录 URL（**注意：直接当图片加载会被判为已扫码**） |
| `loginState` | 扫码状态机 |
| `m_uid` | nav 的 `data.mid`（`dedeUserId()` 的权威来源） |

### Cookie 生命周期

```
loadCookies()                    （main.cpp:366，启动）
 ├─ 读 config bilibili_auth.{sessdata,dede_user_id,bili_jct,user_name}
 ├─ URL 解码（%2C / %2A）——历史 bug 修复点，不解码会导致重启后校验失败并清登录态
 └─ 有 SESSDATA → validateStoredCookies()
     └─ GET api.bilibili.com/x/web-interface/nav（用 rawCookieString 绕过 isLoggedIn 门）
        ├─ code==0 → isLoggedIn=true, uname, mid
        └─ 否则 → 只清内存，不清配置（下次启动仍读到死 Cookie）

startLogin() → 二维码 → 轮询（86303）
 ├─ Cookie 双路来源：data.url 查询串 + Set-Cookie 头
 └─ 成功后 saveCookies() 写 config
```

对外 C++ 接口：`cookieString()`、`rawCookieString()`、`ffmpegCookieString()`、`dedeUserId()`。
`ffmpegCookieString()` 被播放内核用于 `bilivideo.com` 域（`Oran7FFplayerCore.cpp:209-218`）。

### 已知问题

- nav 失败只清内存不清配置 → 死 Cookie 每次启动都重试一遍。
- `logout()` 不清 `m_uid` → 退出登录后 `dedeUserId()` 仍返回旧 mid，弹幕客户端会用错 uid。
- 登录二维码由**外部** `api.qrserver.com` 生成（把登录 URL 发给第三方）；网易云那边是本地 `qrcodegen` 生成，两套不一致。
- 没有任何周期性校验（`Oran7NetworkMonitor` 只校验网易云）。

## 2. 弹幕（`BilibiliDanmakuClient`，QML 类型）

### 接口

| 成员 | 语义 |
|---|---|
| `isConnected` | 认证握手完成 |
| `active` | 用户期望保持连接 |
| `viewerCount` | 心跳回包人气值 |
| `start(roomId)` / `stop()` | 可重复调用；`m_session` 自增让旧 HTTP 回调失效 |
| `danmakuReceived(text, uname, color, timestamp)` | 普通弹幕（`color==0` 归一为 `0xffffff`） |
| `superChatReceived(text, uname, price, color)` | 醒目留言 |
| `errorOccurred(QString)` | 错误 |

### 连接流水线（严格串行，全在 GUI 线程事件循环）

```
start(roomId)
 ├─ m_session++ / 重置计数 / 决定用真实 uid 还是匿名
 ├─ resolveRoomId: room/v1/Room/room_init → 真实 room_id（短号必须转换！）
 ├─ warmUp: GET www.bilibili.com → 往共享 cookie jar 填 buvid3/buvid4/b_nut/b_lsid
 ├─ fetchWbiKeys: x/web-interface/nav → wbi_img.img_url/sub_url → wbiMixinKey()
 ├─ fetchDanmuInfo: xlive/web-room/v1/index/getDanmuInfo（WBI 签名 wts + w_rid）
 │    └─ code==-352 → 降级 room/v1/Danmu/getConf；再失败 → 报错不重试
 ├─ openSocket(wss host_list, /sub)
 ├─ sendAuth{uid, roomid, protover=3, platform=web, type=2, key=token, buvid}
 ├─ op=8 认证回包 → 立即心跳 + 30s 心跳定时器 + 15s 看门狗
 └─ op=5 通知 → brotli/zlib 解压 → 递归解析 16 字节包头 → DANMU_MSG / SUPER_CHAT_MESSAGE
```

### 为什么必须这样（踩坑结论）

| 约束 | 原因 |
|---|---|
| 必须 `room_init` 换真实 id | 短号认证会挂到空频道（viewers=1、只有系统消息） |
| 必须预热 cookie jar | 否则 `getDanmuInfo` 稳定返回 `-352` |
| 必须 WBI 签名 | 同上，缺 `wts/w_rid` 直接风控 |
| 必须 `uid=0` 匿名 | 真实 uid 会被 TLS 层掐断（客户端指纹风控） |
| 必须 `protover=3`（brotli） | `protover=2`（zlib）被服务端抽样更狠，弹幕量差 3-5 倍 |
| 弹幕量天生很少 | 匿名连接被服务端**抽样**，实测 0-4 条/30s，无法绕过 |
| 文本提取需递归兜底 | 格式随版本变，固定下标会漏（`BilibiliDanmakuClient.cpp:769-821`） |

### 重连策略

`scheduleReconnect` 退避 `min(3000+n*7000, 60000)`；`tryReconnect` 轮换 host（最多 4 次）后重新取 token；真实 uid 握手失败会**降级匿名**一次；看门狗 60s 无包则 abort。

### 诊断手段

每次心跳日志打印 `m_danmuReceived/m_danmuEmitted`，用来证明"少弹幕"是服务端抽样而非客户端 bug。

## 3. 直播流地址（`BilibiliRoomAddressCatch`，QML 类型）

```
getRoomInfo(roomId)                          （BilibliLiveRoomAddressCatch.cpp:5）
 ├─ 新建 QNetworkAccessManager（每次一个，见问题）
 ├─ GET xlive/web-room/v2/index/getRoomPlayInfo
 │    protocol=0,1  format=0,1,2  codec=0,1  qn=10000  ptype=8
 ├─ live_status != 1 → urlsError()
 └─ getAvailableStreams()
      ├─ 遍历 stream → format → codec，只取 url_info[0]
      ├─ 跳过 mini* 低码率 HEVC
      ├─ 打分：hevc +200 / avc +100；fmp4 +30 / ts +20 / flv +10；http_stream +10
      ├─ 降序 → m_avliStrAdr[0] 为最优
      └─ emit avliStrAdrChanged + urlsReady
```

QML 侧拿 `avliStrAdr[0]` 调 `Client.requestPlayVideo(url)`，播放内核按域名注入 Cookie。

## 4. 与其他模块的关系

| 关系 | 说明 |
|---|---|
| 播放内核 | 读 `Oran7BilibiliAuthManager::ffmpegCookieString()` 注入 HTTP 头 |
| QML | `BilibiliLoginPopup.qml`（登录）、`VideoPlayerStack.qml`（房间号 → 地址 → 播放 + 弹幕层） |
| 网络监测 | **无**关系（B 站 Cookie 没有周期性校验） |

## 待办

- [ ] 二维码改为本地 `qrcodegen` 生成（与网易云统一，去掉第三方泄漏面）。
- [ ] `logout()` 清 `m_uid`；nav 失败时同步清理配置。
- [ ] 加入周期性 Cookie 校验（复用 `Oran7NetworkMonitor` 的节拍）。
- [ ] `BilibiliRoomAddressCatch` 复用一个 `QNetworkAccessManager`。
- [ ] 弹幕：把"协议魔数 + 文本提取阶梯"抽成可单测的 `DanmakuParser`。
- [ ] 弹幕连接状态接入 UI（当前只有 `isConnected` 属性，没有错误提示面板）。
