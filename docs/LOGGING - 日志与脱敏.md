# 日志与脱敏（P2-2）

实施日期：2026-10-26。日志服务 `Oran7LogSink` 只编译进 Oran7Core，不新增 QML 单例。宏、Qt/QML、FFmpeg 与 WebRTC 的输出统一经过该服务；不再逐行向业务线程池投递日志任务。

## 输出与文件

- 保留 CLIENT/INFO/WARNING/ERROR/NETWORK/CONFIG 宏与既有级别；宏增加源文件模块前缀。Qt 分类名、FFmpeg/WebRTC 前缀继续保留。
- 控制台附加、UTF-8 和颜色保留；Oran7ConsoleLogger 不再安装另一套消息处理器。实际控制台和文件 I/O 均由日志专用 std::thread 执行。
- 默认目录为 `QStandardPaths::AppLocalDataLocation/Logs`，与播放器配置、录制输出分开。文件名 `oran7-YYYY-MM-DD-NNNNNN.log`，UTF-8。
- 按日期或 8 MiB 分段，最多保留 10 个本服务文件。仅处理严格匹配上述命名格式的文件，不清理其他用户文件；重启向未满末段追加。
- 日志目录用 QLockFile 非阻塞互斥。其他进程正在写入时，本进程降级为控制台，避免互相覆盖/轮换；不等待锁。

## 队列、失败与退出

- FIFO 默认最多 2048 条；单条正文最多 8192 个 UTF-16 字符，进入队列前完成脱敏。脱敏输入也限制为 65536 字符，避免异常正文无限增长；文件分段很小时额外限制单条输出长度。
- 满队列采用“丢弃新消息”，包括警告/错误；不挤出旧消息、不阻塞生产者等待 I/O。写线程随后输出汇总条数；accepted/completed/dropped/peakQueued 等统计可查询。
- 文件不可写、轮换/删除失败时，仅提示一次并关闭本次会话的文件出口，控制台继续；不递归写 Qt 错误、不在每条消息上重复重试。控制台回调失败也不终止文件写入或工作线程。
- 生产者只做有限格式化/脱敏和短临界区入队；磁盘、控制台和文件删除不在 GUI/业务线程执行。
- main 中的 scope guard 在 QML 引擎和 QGuiApplication 析构后关闭日志服务，覆盖初始化提前返回；正常关闭先拒绝新消息，再排空已接收队列并 join。晚于关闭的静态析构日志丢弃并计数，不重建线程或恢复同步输出。
- Qt fatal 最多尝试等待 200 ms 后由 Qt 自行终止。FFmpeg fatal 只映射为错误日志，不调用 qFatal。崩溃 dump 保留原机制，异常终止不承诺排空全部日志。

## 脱敏边界与源头审计

所有出口使用同一份脱敏后的记录：Cookie/Set-Cookie、Authorization/Bearer/Basic、密码/账号、常用 token/CSRF/会话/签名字段、JWT、SDP ICE 凭据，以及带标记的 body/payload/params 等隐藏。对敏感字段保守隐藏该行余下内容；HTTP/WS/RTMP 等 URL 仅保留协议、主机、端口，用户信息、路径、查询、fragment 均隐藏。换行被转义，正文不能伪造下一条日志。

脱敏不能替代调用处控制：不要新增原始响应、Cookie、头部、SDP、任意对象或二进制包转储。无法凭字符串识别无标签、任意编码的秘密，因此本批同时检查真实输出入口：

| 模块 | 审计与调整 |
|---|---|
| 网易云 transport/API/浏览器登录 | 保留 requestId/tag/path/status/bytes 等摘要；不打印 curl 错误正文、Cookie/响应体；移除登录昵称输出。现行请求 ID、代际和协议不变。 |
| B 站认证、直播和弹幕 | 移除认证昵称、直播 API 正文片段和完整直播请求/候选地址输出；保留错误码、状态、编码和评分。网络错误/弹幕 WebSocket 地址经过统一出口再脱敏。 |
| 播放与媒体探测 | FFmpeg 全局回调接入统一出口，关闭原始 av_dump_format 元数据转储；ffprobe/ffmpeg 子进程错误经日志宏脱敏。解码器直接 printf 改用原日志宏。 |
| 录制、WHIP、信令 | muxer 的直接 printf 改用日志宏；移除信令登录账号。原有状态码/body_size/SDP size 摘要保留，libdatachannel 警告/错误回调接入统一出口；不启动或扩展推流。 |
| 陈旧 TCP/Server | 仅将密码、用户名与原始十六进制数据包输出改为状态/长度；模块继续默认停用，无数据库或协议变更。 |
| 配置、主题和 QML | 不输出配置全文；保留主题 token 等非认证参数诊断。QML console 由同一 Qt 消息处理器接管。Sonic 内部数值诊断不包含网络数据/凭据，未改第三方算法。 |

本批只处理日志，不加密配置、不销毁历史日志/明文备份；凭据持久化迁移仍属于 P2-3。

## 检查与验收

`ctest --preset checks -R logging` 使用临时目录和虚构凭据，检查真实宏/Qt/QML/FFmpeg 入口、控制台与文件一致脱敏、递归抑制、8 线程 8000 条提交、队列满、flush 超时、并发关闭、日期/大小滚动、重启追加、保留上限、目录竞争、不可写路径及控制台异常。

完整离线检查现为 25 项。主程序、真实控制台、实际播放/录屏与退出体验由用户运行验收；本批不代跑主程序、不连接真实账号或推流服务。

**验证结果**：Release 构建成功，再次增量显示 `ninja: no work to do`；25/25 项 CTest 通过（最终 60.30 秒），其中 logging 含 89 项断言。编译数据库确认 Oran7LogSink.cpp 仅编译进 Oran7Core 一次。记录位于 `build/refactor-work/P2-2/`。
