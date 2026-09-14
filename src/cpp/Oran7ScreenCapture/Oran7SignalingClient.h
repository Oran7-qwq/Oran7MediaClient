#ifndef ORAN7SIGNALINGCLIENT_H
#define ORAN7SIGNALINGCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <string>

/**
 * @brief 最小信令客户端 — 登录 → 创建房间 → 通知推流
 *
 * 流程: WebSocket 连上 → login(user3,123456) → create_room("demo")
 *       → 服务器返回 room_created(含真实 room_id)
 *       → 外部调用 notifyStreamPublished → stream_published 广播给房间内所有人
 *
 * 注意:
 *   - create_room 成功后服务器端 user3 自动加入该房间
 *   - web 端用户通过大厅列表找到同名("demo")房间加入即可
 */
class Oran7SignalingClient : public QObject {
    Q_OBJECT
public:
    explicit Oran7SignalingClient(QObject* parent = nullptr);
    ~Oran7SignalingClient();

    /** 启动: 连接 → login → create_room 一气呵成 */
    void start(const std::string& wsUrl,
               const std::string& username,
               const std::string& roomName);

    /** WHIP 成功拿到 stream_id 后调用，发送 stream_published */
    void notifyStreamPublished(const std::string& streamId);

    /** 通知推流停止 */
    void notifyStreamStopped();

    /** 关闭 */
    void stop();

    /** 服务器分配的真实 room_id（create_room 返回的），供外部参考 */
    std::string roomId() const { return _roomId; }

private slots:
    void onConnected();
    void onMessage(const QString& text);

private:
    void sendJson(const QJsonObject& obj);
    void doLogin();
    void doGetRooms();
    void doCreateRoom();
    void doJoinRoom(const std::string& roomId);
    void doPublishStream();

    QWebSocket   _ws;
    QTimer       _connectTimer;
    std::string  _username;
    std::string  _roomName;   // 要创建的房间名
    std::string  _roomId;     // 服务器分配的真实 room_id
    std::string  _streamId;
    bool         _loggedIn = false;
    bool         _inRoom   = false;
    bool         _pendingPublish = false;
    bool         _joinRetry = false;
};

#endif // ORAN7SIGNALINGCLIENT_H
