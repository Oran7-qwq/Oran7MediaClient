---
status: ✅ P1-4 代码完成，运行待验收
updated: 2026-10-24
---

# Ncm：网易云音乐接入

源码 `src/ncm/`，提供浏览器登录、搜索、喜欢列表、漫游与播放地址解析。网络监测和共享异步调度在 GlobalSingleton；播放器地址策略在 Oran7MediaClient/Oran7NetworkSongResolver。

## 分层与所有权

```
NcmSession（组合根）
 ├─ unique_ptr<CookieStore>
 ├─ unique_ptr<CurlHttpClient>
 ├─ unique_ptr<NcmUserApi / NcmSongApi / NcmPlaylistApi>
 └─ unique_ptr<WebLoginController>

GUI API → NcmProtocol / NcmRequest → 异步系统 curl
Oran7AsyncManager → NcmAsyncHandlers → blockingCurl（worker）
                    └─ Oran7NcmTrackMeta / Oran7NcmJsonMapper
```

Session 子对象不设 QObject parent，依赖逆序销毁，QML 通过 CppOwnership 借用。API 以 QPointer 借用传输。协议层处理 WEAPI 加密和请求头，传输层只接收 NcmRequest。QML 注册保留 `Oran7NetEaseAuth` 中的 NcmSession、NcmWebLogin、NcmPlaylistApi。

P1-4 已删除未使用的 HttpClient、NcmAuthApi、QrLoginController、Oran7MusicSearchService，以及 Playlist API 中旧喜欢拉取/漫游实现；移除旧注册与 image://ncmqr provider。浏览器登录仍是现行路径，第三方 qrcodegen 源码保留但不再编译。

## 唯一歌曲数据与解析

`Oran7NcmTrackMeta` 是歌曲 DTO，`Oran7NcmJsonMapper::track` 是搜索、喜欢、漫游及启动详情补全的共同解析入口。接受 songInfo 嵌套或平铺记录，兼容 ar/artists、al/album、dt/duration，保留毫秒时长；fee==1 表示 VIP。

统一 QVariantMap 输出：songId、title、artist、album、cover、durationMs、isVip、url、path、source，另有兼容用的 timesize 秒字符串。模型使用 durationMs，旧配置仍按原格式保存。JSON 原始字段不再直接传到 QML；模型自身保留旧角色别名以兼容已有视图。

`completeMissing` 只补同 ID 的缺项；详情带 fee 时更新 VIP。`playbackUrls` 过滤空 URL 和非法 ID。搜索、喜欢、漫游共用解析，但保留各自请求步骤与降级规则。

## 请求 ID 与 API 契约

NcmRequest 包含 id、tag、url、body、headers、isPost、disableHttp2；`nextId()` 为 GUI/worker/Resolver 原子分配非零唯一 ID。tag 仅标识业务类型或 URL path，不包含 query、Cookie 或 token。

| 层 | 结果契约 |
|---|---|
| CurlHttpClient | requestFinished(requestId, tag, httpStatus, body, setCookie) / requestError(requestId, tag, message) |
| NcmSongApi | resolveUrls(ids, requestId=0)；默认自动分配，结果只有 urlsResolved(requestId, map) / apiError(requestId, message) |
| NcmUserApi | fetchAccount(requestId) / verifyLogin(requestId)；accountFetched(requestId, nickname)、verifyResult(requestId, valid)、apiError(requestId, message) |
| NcmPlaylistApi | setLike(songId, like) / reorderLikedSongs(ids) 返回 requestId；likeResult(requestId, songId, like, success, message)、reorderResult(requestId, success, message) |

每个 API 仅接收已登记且 tag 匹配的请求，消费一次；点赞保存每次操作的 songId/like，避免 last-state 串歌。Playlist/User 保存凭据快照，账号变化丢弃旧结果；排序只通知当前请求且歌单仍匹配的结果。此规则保障结果归属，不保证并发写操作在服务端的执行顺序。

Session 对外仍发 verifyResult(int)，内部按最新校验 ID 转发；浏览器只在 Captured 阶段接收自己的账号读取 ID。同步失败前先登记上下文，重复/旧/其他 API 回复不能完成当前操作。

## 传输与风控约束

- NCM 账号、播放地址、写操作，以及线程池搜索/喜欢/漫游使用 Windows 系统 curl（Schannel）。歌词、B 站、网络监测及启动详情等既有 QNAM 通道保留。
- curl 保持 `--http1.1 --ssl-no-revoke --globoff`，URL 使用 FullyEncoded，POST body 通过临时文件传递。Cookie 手工注入；写操作 CSRF 同时放表单与 query。
- `os=pc; appver=8.10.35` 只给 enhance/player/url，不能通用注入 WEAPI。保留原 UA/Referer 与 WEAPI 加密：双 AES-CBC + RSA，表单逐字段百分号编码。
- 异步超时 10 秒，阻塞请求 timeoutMs 转为 curl 秒参数；HTTP 4xx/5xx 与进程失败分别由上层判断。
- QTemporaryDir 管理请求/响应临时文件；异步启动失败只回调一次，销毁时终止剩余子进程后清理目录。
- 日志仅记录请求 ID、业务 tag/path、状态、长度、退出码；不记录响应体、Cookie/token、原始 stderr。

## 工作流

- 搜索：匿名 api/search/get → 匿名 song/detail 补封面 → 已登录且有 VIP 时请求真实 URL。封面/URL 阶段失败沿用兜底；最终返回统一 DTO 列表。
- 喜欢：account/get → user/playlist（specialType==5，名称兜底）→ v6/playlist/detail 的有序 trackIds → 每批 100 个详情。有效空数组是成功；缺少 trackIds 或非空列表全部详情失败报错，避免误清空缓存；部分详情缺失沿用原跳过策略。
- 漫游：WEAPI intelligence/list，解析 songInfo 或平铺结果；缺标题/歌手/封面的曲目补详情，合并后丢弃仍无标题的记录。
- 上述阻塞任务走共享 Oran7AsyncManager 网络组，GUI 只传 Cookie/csrf 等值快照；worker 不读取 Session/QML QObject。
- 浏览器登录：Idle → StartingBrowser → WaitingLogin → Captured → Success/Error；通过 Edge/Chrome CDP 读取 Cookie，沿用原超时与清理流程。

## 播放与加载策略

Client.playNetworkSong 委托 Oran7NetworkSongResolver。非 VIP 外链先播、后台解析只更新缓存；VIP 等本次尝试 ID 对应的 urlsResolved。缓存 20 分钟，失败 3 秒后重试一次；打开失败另有一次缓存补救预算。用户请求代际和 HTTP 尝试 ID 分开，切歌/停止/退出失效旧请求；普通查询也使用唯一 ID，不存在无关联广播。

FFmpeg 对网易域名使用 Session 的 ffmpegCookieString。歌词由 Oran7LyricsService/Oran7OnlineLyricsFetcher 独立处理。

P1-7：喜欢列表启动预加载、首次页签进入和刷新共用 Client.cloudMusic（Oran7CloudMusicService）的加载状态；成功缓存 30 秒，账号变化隔离旧回调。收藏/漫游读取由该服务调用既有 handler，后台只读凭据值；漫游队列由 Client.musicQueue（Oran7PlaybackViewModel）持有。全部点赞入口共享 2 秒节流，乐观修改后置脏，失败由服务重新对账。搜索关键词和搜索请求代际仍在 QML。

## 验证与后续

Release 构建及独立检查覆盖解析回退、毫秒精度、请求乱序/重复/同步失败/账号变化、实际 handler 编排、Resolver 重试与本机真实 curl 传输。测试不使用用户凭据、不访问公网、不启动主程序或浏览器；登录与真实网易云服务行为待用户运行验收。

P2-2 已统一日志出口与脱敏；P2-3 已将 CookiePersistence 替换为 Oran7DpapiCookiePersistence，凭据由配置管理器分离加密，见 [[CREDENTIALS - 凭据加密与迁移]]。并发 curl 进程限制、自动重登策略仍需按实际范围核查。CookieStore 仍是 QNetworkCookieJar 派生数据容器，不安装到 QNAM；EAPI 仍为空实现。
