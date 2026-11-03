#ifndef BILIBILIDANMAKUCLIENT_H
#define BILIBILIDANMAKUCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QByteArray>

class QNetworkReply;

/**
 * @brief B站直播弹幕客户端
 *
 * 通过 B 站开放弹幕协议（WebSocket 二进制帧）接入直播间弹幕服务器：
 *   1. 调用 getDanmuInfo API 获取弹幕服务器 host/token（已登录则注入 Oran7BilibiliAuthManager cookie）
 *   2. 连接 wss://{host}:{wss_port}/sub，发送认证包（protover=1 → 服务器直接下发未压缩 JSON 弹幕）
 *   3. 每 30s 发送心跳保活；断线后自动重连（重新拉取 token）
 *   4. 解析 op=5 通知：普通弹幕（DANMU_MSG）与醒目留言（SUPER_CHAT_MESSAGE）
 *
 * QML 用法（注册类型，仿 BilibiliRoomAddressCatch）：
 *   BilibiliDanmakuClient { id: dm; onDanmakuReceived: ... }
 *   dm.start(房间号) / dm.stop()；属性 isConnected / active
 */
class BilibiliDanmakuClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged FINAL)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(int viewerCount READ viewerCount NOTIFY viewerCountChanged FINAL)
public:
    explicit BilibiliDanmakuClient(QObject *parent = nullptr);

    // 开始/停止监听指定直播间弹幕（重复 start 会先断开旧连接）
    Q_INVOKABLE void start(const QString &roomId);
    Q_INVOKABLE void stop();

    bool isConnected() const { return m_connected; }
    bool isActive() const { return m_active; }
    int viewerCount() const { return m_viewerCount; }

signals:
    // 普通弹幕：text=内容 uname=用户名 color=RGB 十进制(0xRRGGBB) timestamp=时间戳
    void danmakuReceived(const QString &text, const QString &uname, int color, qint64 timestamp);
    // 醒目留言：text=内容 uname=用户名 price=金额(元) color=RGB 十进制
    void superChatReceived(const QString &text, const QString &uname, int price, int color);
    void connectionChanged(bool connected);
    void activeChanged(bool active);
    void viewerCountChanged(int count);
    void errorOccurred(const QString &message);

private:
    void setConnected(bool connected);
    // 把用户输入的房间号（可能是短号）解析成真实 room_id（弹幕认证必须用真实号）
    void resolveRoomId();
    // 先访问 bilibili.com 主页，让 cookie jar 收集 buvid3/buvid4/b_nut/b_lsid 等，
    // 这是 getDanmuInfo 通过风控(-352) 的关键
    void warmUp();
    // 从 nav 接口取 WBI 密钥并计算 mixin key，然后请求弹幕服务器信息
    void fetchWbiKeys();
    void fetchDanmuInfo();
    void openSocket(const QString &host, quint16 wssPort);
    void openSocketAt(int index);
    // 组装 Cookie 头：cookie jar（buvid3/buvid4/b_nut 等）+ B站登录 Cookie
    QString cookieHeaderFor(const QUrl &url) const;
    void sendAuth();
    void sendPacket(quint16 protover, quint32 operation, const QByteArray &body);
    void sendHeartbeat();
    void scheduleReconnect();
    void handlePacket(const QByteArray &packet);
    void parseJsonBody(const QByteArray &body);
    void handleDanmuMsg(const QJsonObject &obj);
    void handleSuperChat(const QJsonObject &obj);

private slots:
    void onSocketConnected();
    void onBinaryMessageReceived(const QByteArray &message);
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void watchdogTimeout();
    void tryReconnect();

private:
    QWebSocket *m_socket = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    QTimer *m_heartbeatTimer = nullptr;   ///< 30s 心跳
    QTimer *m_watchdogTimer = nullptr;    ///< 15s 检查一次最后收包时间，超 60s 判定假死并重连
    QTimer *m_reconnectTimer = nullptr;   ///< 断线自动重连延迟

    QString m_roomId;                 ///< 真实房间号（解析后）
    QString m_inputRoomId;            ///< 用户输入的房间号（可能为短号）
    QString m_token;
    QString m_wbiMixinKey;            ///< 已算好的 WBI mixin key
    bool m_wbiReady = false;          ///< WBI key 是否已获取
    bool m_useLegacyEndpoint = false; ///< getDanmuInfo 被风控(-352)时回退到旧接口 getConf
    QVector<QPair<QString, int>> m_hosts;  ///< 弹幕服务器列表 (host, wss_port)
    int m_hostIndex = 0;              ///< 当前尝试的主机下标
    int m_connectFailures = 0;        ///< 连续连接失败次数（用于切主机后重新拉 token）
    bool m_active = false;                ///< start/stop 语义上的"希望保持连接"
    bool m_connected = false;             ///< 已完成认证握手
    bool m_fetching = false;              ///< 正在拉取弹幕服务器信息（防并发）
    int m_viewerCount = 0;
    int m_reconnectAttempt = 0;
    int m_session = 0;                    ///< 会话代际，start/stop 时自增，作废在途回调
    qint64 m_lastPacketMs = 0;            ///< 最后收到任意数据包的时间(ms)

    // 诊断计数：收到多少 DANMU_MSG / 实际发出（定位"被过滤"发生在服务端还是提取）
    int m_danmuReceived = 0;
    int m_danmuEmitted = 0;
    int m_danmuLastReceived = 0;
    int m_danmuLastEmitted = 0;

    // 真实 uid 策略：匿名(uid=0) 连接被服务器抽样（实测 0~3 条/30s），
    // 登录态用真实 uid 可拿全量弹幕；若被服务器掐断(TLS close) 则回退匿名
    bool m_useRealUid = false;
    bool m_realUidFailed = false;
};

#endif // BILIBILIDANMAKUCLIENT_H