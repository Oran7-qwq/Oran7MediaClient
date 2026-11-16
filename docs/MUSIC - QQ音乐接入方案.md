---
status: 官方网页扫码登录已由用户验收；搜索、喜欢列表、播放与收藏业务首版已接入，待真实账号验收
updated: 2026-11-12
---

# QQ 音乐接入

## 当前范围

整体沿用网易云音乐的分层与交互习惯；账号、请求、曲目身份及列表数据分别管理。官方网页扫码登录已由用户确认成功，本轮继续接入搜索、喜欢列表、播放地址、上下曲、歌词、收藏和整合列表。

已加入侧边导航「从 QQ 音乐搜索」，复用导航选中/悬停双滑块。QQ 音乐页具有搜索框、扫码登录入口、「我喜欢的音乐」「搜索结果」两个页签及刷新入口；列表继续使用 `Oran7MusicPlaylistView`。QQ 页属于音乐页面，底部播放栏和页面底部避让与本地/网易云一致。

登录浮层提供官方网页扫码流程说明、关闭/ESC、进入退出动画及窄窗口适配。点击「打开网页扫码登录」后启动独立 Edge/Chrome 窗口，等待官网脚本就绪后自动打开登录弹窗；使用 QQ 或微信扫码并确认后，客户端验证账号并显示昵称。登录成功自动关闭本次登录浏览器，退出登录清理 QQ 音乐会话、列表和请求，并停止当前 QQ 歌曲。搜索、刷新及列表播放等操作连接独立业务服务，不借用网易云的会话或列表。

## 界面与后端边界

`Oran7QQMusicStack` 只持有搜索输入和提示等界面状态。RightPage 显式注入 `accountSession`、`musicService`、`likedModel` / `searchModel`，分别借用 `Client.qqMusic`、`Client.qqMusicService`、`Client.tracks.qqLiked` / `qqSearch`；QML 不创建曲库副本。

页面向外发出具名意图：`searchRequested(keywords)`、`likedRefreshRequested()`、`loginRequested()`、`loginCancelled()`、`logoutRequested()`、`trackPlayRequested(model, index)`、`trackLikeRequested(track)`、`trackIntegrateRequested(track)`。登录命令连接账号服务，搜索/喜欢/刷新连接业务服务；播放经统一 musicQueue，整合经 Client 门面进入 TrackLibrary。底部控制栏按当前曲目的平台分派喜欢与整合操作。

`Oran7QQMusicLoginPopup` 只呈现外部账号/登录状态，关闭正在登录的浮层时发出取消意图。账号与曲库加载状态分别管理，登录后预加载和进入页面刷新共享同一缓存状态。QQ 云端拖拽排序待策略确定后开放。

## 音乐业务首版

- Client 独占 `Oran7QQMusicService`，再由服务持有 `Oran7QQMusicApi`；均无 QObject parent，仅 EXE 编译，不增加全局单例。账号事件使旧请求、列表、地址缓存及歌词失效。
- 搜索首版返回第一页 30 首。`music.search.SearchCgiService/DoSearchForQQMusicDesktop` 必须使用同名信封键、移除顶层 comm；普通请求结构可能返回伪空列表。请求和响应按 UTF-8 处理。已通过匿名公开请求确认中文搜索和歌词返回有效内容，检查不使用用户 Cookie。
- 喜欢列表由 C++ 分页暂存，200 首/页、最多 100000 首，全部完成后原子发布；中途失败保留此前完整模型。30 秒成功 TTL，启动预加载和页签进入不重复请求。
- `Oran7TrackLibrary` 独占 qqLiked/qqSearch 模型，成员关系沿用模型哈希索引。Oran7Track/roles/序列化补充 provider、providerId、mediaMid、nativeId；QQ 的网易云兼容 songId 始终为 0。稳定入口为 `qqmusic://song/<MID>`，临时 CDN 地址不写整合列表与最近歌曲配置。
- 统一队列识别 qqmusic owner，支持顺序/随机/单曲循环、上一首/下一首、点击暂停与列表焦点；不要求网易云登录。整合列表保留完整平台信息，多选整合也保留。重启仅恢复聚焦元数据，按 MID 重新解析播放地址。
- 地址解析经歌曲详情补足媒体 MID、vkey 接口取得实际可用音源；按返回结果选择无损/MP3/AAC，不伪造权限或地址。成功地址缓存 20 分钟、最多 64 条；原生打开失败仅刷新重试一次；错误经现有队列做有界处理。
- QQ 歌词走独立获取、临时 LRC 与取消代际，仍转发 Client 原有歌词信号，不以 QQ 数字 ID 请求网易云歌词。
- 喜欢/取消喜欢使用官网 ag-1 签名封装：Windows CNG AES-GCM，源码保留 qmweb-sign 的 MIT 版权许可。写前查询详情核对 MID、数字 ID 和实际歌曲类型，不硬编码 songType；采用 dirId + v_songInfo 的最小收藏参数。写操作不自动重试，外层/业务码及 retCode 成功才更新本地成员关系，随后读回完整云端快照；共享两秒冷却，超时/失败通过读回校准。
- 网络请求全部异步，12 秒期限、4 MiB 单次响应限制、禁止凭据跨站重定向。取消、切歌、退出与账号切换丢弃旧结果；不在 GUI 线程等待网络。凭据只以值快照传递，不输出到 QML 或日志。

首版不含 QQ 私人推荐模式、云端歌单管理、云端拖拽排序、翻页搜索和音质选择设置；现有网易云推荐模式继续属于网易云。

## 登录实现与边界

- `src/cpp/Oran7MediaClient/qqmusic/` 内的 `Oran7QQMusicSession` 由 Client 的 `unique_ptr` 独占，无 QObject parent，QML 借用 `CppOwnership`；不是全局单例，不增加 EXE/插件共享业务状态。
- `Oran7QQMusicWebLogin` 为每轮创建 `QTemporaryDir`，以随机 CDP 端口启动官方网页；只读取本轮 `DevToolsActivePort`，连接固定 `127.0.0.1` 并强制 `NoProxy`，不附着用户原有浏览器。官方网页负责显示和刷新二维码。
  入口修正（2026-11-12）：`https://y.qq.com/n/ryqq` 实测返回 404，现使用 `https://y.qq.com/`（实测 200）。隔离假浏览器同时检查生产启动参数，避免错误地址被替身测试掩盖。
  登录弹窗修正：CDP 仅附着 HTTPS `y.qq.com` 的页面，通过 `Runtime.evaluate` 等待 `.mod_top_login .top_login__link` 的点击处理器就绪后打开官网登录弹窗；已经打开则不再点击。最多自动等待 20 秒，网页结构变化时保留手动入口及明确提示，不影响后续扫码回读。假浏览器检查 target/session 路由并以 JS 引擎执行生产表达式，验证页面未就绪时等待、就绪后仅点击一次。另以独立匿名无头浏览器加载当前官网并执行相同表达式，确认实际出现 QQ OAuth 与微信扫码 iframe；未启动主程序、未读取已有浏览器环境、未扫码或提交真实账号授权。
- CDP 的 `Storage.getCookies` 仅提取 QQ 根域/音乐域白名单字段，过滤其它站点、过期值和非法头字符；普通 QQ 的 `skey/p_skey` 不能单独视作 QQ 音乐登录。
- 使用 `music.UserInfo.userInfoServer/GetLoginUserInfo` 异步验证，外层和业务层均成功且有有效数据后才提交登录。10 秒验证期限；启动浏览器期限 20 秒、等待扫码期限 180 秒。拒绝中间票据后继续等待网页票据变化，不把超时当成功。
- 凭据走 `auth.qqMusic` / `qqmusic_auth`，以独立 `qqmusic` DPAPI 分组保存。公开 JSON 只保存密文引用，不向 QML 暴露 Cookie。保存失败回滚内存，不发布登录成功。
- 启动异步验证已保存凭据；网络异常或凭据过期不冒充已登录，也不启动浏览器。取消、退出和 shutdown 先作废请求代际，再 abort；迟到响应不能恢复账号。退出成功提交空凭据代际，旧配置实例不能覆盖复活账号。
- 普通取消与成功交接通过异步关闭浏览器、定时回收进程和后台临时目录清理，不在 GUI 线程等待进程退出；快速重开排队等待旧环境回收。只有最终 shutdown 允许有限等待。
- 日志仅记录请求编号、恢复标记与业务返回码，QQ 票据字段纳入统一脱敏，不记录响应体和凭据。

隔离检查使用本地 HTTP + 假 CDP 浏览器子进程和虚构 Cookie，覆盖登录、验证、加密恢复、退出、取消、快速重开、拒绝、超时、异常响应、重定向及保存失败。没有启动主程序或访问真实账号；QQ/微信实际扫码、官网风控和网络环境由用户验收。

本轮 Release 构建和 14 项相关离线检查通过；最后补充的收藏身份核对与真实 songType 检查也已单独复验通过。业务专项使用本地 HTTP 服务，覆盖搜索信封、分页原子发布、上下曲、地址缓存与重试、歌词取消、签名加密收藏与失败读回、身份不匹配拒绝写入、账号切换与迟到结果隔离。未启动主程序，真实账号的喜欢列表、播放权限及收藏读回留用户验收。

检查入口为 `ctest --preset checks -R 'qqmusic|state.p1_checks|track_model_checks|policy_checks|playlist_qml_checks|config_checks|facade_checks|lyrics.lyrics_checks|resolver_checks|credentials_checks|logging_checks|localization_checks|resources.qml_imports'`。

## 后续接入顺序

### 客户端内扫码的可行性

可采用类似现有 B 站弹窗的交互：直接显示二维码、等待扫码、等待手机确认、验证账号、成功/过期/刷新。此项已调研，尚未替换当前网页登录。

- **QQ 扫码**：`ssl.ptlogin2.qq.com/ptqrshow` 返回二维码图片和本轮 qrsig；按 ptqrtoken 轮询 `ptqrlogin`，确认后完成 check_sig、QQ OAuth 授权，再经 `QQConnectLogin.LoginServer/QQLogin` 换取音乐票据。普通 QQ 登录成功仍不能直接当作音乐账号成功。
- **微信扫码**：官方 `open.weixin.qq.com/connect/qrconnect` 提供二维码，按 UUID 轮询确认；授权 code 交给 `music.login.LoginServer/Login` 换音乐票据。
- **QQ 音乐 App 扫码**：现有开源实现也有相应链路，但涉及 MQTT 状态推送，复杂度更高，建议放在 QQ/微信之后。
- 复用 B 站弹窗的状态呈现思路；QQ/微信请求、临时 Cookie、授权跳转必须独立管理，不能直接套用 B 站登录 API。成功后的验证与独立 DPAPI 保存继续复用现有 QQ 会话服务；网页登录可保留为遇到风控时的备用入口。
- 跳转按官方 HTTPS 域名校验，取消/过期作废本轮代际，不记录 qrsig、授权码、票据或二维码原始内容。是否能完成真实账号授权仍需用户扫码验收，生成二维码不等于完整登录验证成功。

1. **已验收**：官方网页扫码登录。
2. **首版已实现**：独立会话及音乐 API 异步传输。
3. **首版已实现**：搜索、喜欢列表分页与缓存、成员哈希查询。
4. **首版已实现，待真实账号验收**：地址解析、歌词、喜欢/取消喜欢、整合、上下曲与重启恢复。
5. 后续按使用反馈推进推荐、云端歌单与界面扩展。

QQ 音乐的歌曲 MID、媒体 MID 和数字歌曲 ID 需要独立表示，不能直接套用网易云 `songId`。临时播放链接不作为持久身份，跨平台的整合列表必须携带平台标记及原始 ID。播放质量和权限以平台返回结果为准。

## 调研参考

- [QQMusicApi 模块源码](https://github.com/L-1124/QQMusicApi/tree/main/qqmusic_api/modules)：搜索、歌曲、歌词与歌单的请求结构。
- [simple-music 的 QQ 音乐接入记录](https://github.com/Yyyangshenghao/simple-music/blob/master/docs/qq-music-api.md)：官方网页登录、歌曲身份与 Web 接口差异的实现参考。

上述为协议调研参考，没有引入这些项目的运行时依赖；实现使用现有 Qt Network/WebSockets 与项目 DPAPI 存储。
