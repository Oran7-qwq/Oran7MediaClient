#include "Oran7SignalingClient.h"
#include "Oran7GlobalHelper.h"
#include <QJsonArray>

Oran7SignalingClient::Oran7SignalingClient(QObject* parent)
    : QObject(parent)
{
    _connectTimer.setSingleShot(true);

    connect(&_ws, &QWebSocket::connected,
            this, &Oran7SignalingClient::onConnected);
    connect(&_ws, &QWebSocket::textMessageReceived,
            this, &Oran7SignalingClient::onMessage);
    connect(&_ws, &QWebSocket::disconnected, this, []() {
        INFO_LOG << "[signal] disconnected";
    });
    connect(&_ws, &QWebSocket::errorOccurred, this, [](QAbstractSocket::SocketError) {
        WARNING_LOG << "[signal] socket error";
    });
    connect(&_connectTimer, &QTimer::timeout, this, [this]() {
        WARNING_LOG << "[signal] connect timeout";
        _ws.close();
    });
}

Oran7SignalingClient::~Oran7SignalingClient() { stop(); }

void Oran7SignalingClient::start(const std::string& wsUrl,
                                  const std::string& username,
                                  const std::string& roomName, const std::string& password)
{
    _password = password;
    _stopping = false;
    _username = username;
    _roomName = roomName;
    _ws.open(QUrl(QString::fromStdString(wsUrl)));
    _connectTimer.start(5000);
    INFO_LOG << "[signal] connecting";
}

void Oran7SignalingClient::notifyStreamPublished(const std::string& streamId)
{
    _streamId = streamId;
    if (_inRoom) {
        doPublishStream();
    } else {
        _pendingPublish = true;
    }
}

void Oran7SignalingClient::notifyStreamStopped()
{
    if (_inRoom && !_streamId.empty()) {
        QJsonObject msg;
        msg["type"]      = "stream_stopped";
        msg["stream_id"] = QString::fromStdString(_streamId);
        sendJson(msg);
    }
}

void Oran7SignalingClient::stop()
{
    if (_stopping) return;
    _stopping = true;
    _connectTimer.stop();
    notifyStreamStopped();
    // 离开房间，必须确保消息在 close 之前发出去
    if (_inRoom) {
        QJsonObject msg;
        msg["type"] = "leave_room";
        sendJson(msg);
        _ws.flush(); // 交由 Qt 异步发送，不在 GUI 里嵌套事件循环等待断开。
        _inRoom = false;
    }
    _ws.close();
}

// ---- slots ----

void Oran7SignalingClient::onConnected()
{
    if (_stopping) return;
    INFO_LOG << "[signal] connected, submitting login";
    _connectTimer.stop();
    doLogin();
}

void Oran7SignalingClient::onMessage(const QString& text)
{
    if (_stopping) return;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "login_result") {
        if (obj["success"].toBool()) {
            _loggedIn = true;
            INFO_LOG << "[signal] login ok, checking rooms for:" << _roomName.c_str();
            doGetRooms();
        } else {
            WARNING_LOG << "[signal] login failed";
        }
    } else if (type == "rooms_list") {
        // 查房间列表，看 "demo" 是否已存在
        QJsonArray rooms = obj["rooms"].toArray();
        QString target = QString::fromStdString(_roomName);
        QString foundId;
        for (const auto& r : rooms) {
            QJsonObject room = r.toObject();
            if (room["room_name"].toString() == target) {
                foundId = room["room_id"].toString();
                break;
            }
        }
        if (!foundId.isEmpty()) {
            INFO_LOG << "[signal] room exists, joining:" << foundId;
            doJoinRoom(foundId.toStdString());
        } else {
            INFO_LOG << "[signal] room not found, creating:" << _roomName.c_str();
            doCreateRoom();
        }
    } else if (type == "room_created") {
        _roomId = obj["room_id"].toString().toStdString();
        _inRoom = true;
        INFO_LOG << "[signal] room created, id=" << _roomId.c_str()
                 << " name=" << obj["room_name"].toString();
        if (_pendingPublish) {
            _pendingPublish = false;
            doPublishStream();
        }
    } else if (type == "room_joined") {
        _roomId = obj["room_id"].toString().toStdString();
        _inRoom = true;
        INFO_LOG << "[signal] joined room:" << _roomId.c_str()
                 << " name=" << obj["room_name"].toString();
        if (_pendingPublish) {
            _pendingPublish = false;
            doPublishStream();
        }
    } else if (type == "error") {
        // 如果 join_room 报"房间不存在"，回退到创建
        QString msg = obj["message"].toString();
        if (msg.contains("不存在") && _joinRetry) {
            _joinRetry = false;
            INFO_LOG << "[signal] join failed (room gone), creating:" << _roomName.c_str();
            doCreateRoom();
        } else {
            WARNING_LOG << "[signal] server reported an error";
        }
    }
}

// ---- private ----

void Oran7SignalingClient::sendJson(const QJsonObject& obj)
{
    _ws.sendTextMessage(
        QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void Oran7SignalingClient::doLogin()
{
    QJsonObject msg;
    msg["type"]     = "login";
    msg["username"] = QString::fromStdString(_username);
    msg["password"] = QString::fromStdString(_password);
    sendJson(msg);
}

void Oran7SignalingClient::doGetRooms()
{
    QJsonObject msg;
    msg["type"] = "get_rooms";
    sendJson(msg);
}

void Oran7SignalingClient::doCreateRoom()
{
    QJsonObject msg;
    msg["type"]      = "create_room";
    msg["room_name"] = QString::fromStdString(_roomName);
    sendJson(msg);
}

void Oran7SignalingClient::doJoinRoom(const std::string& roomId)
{
    _joinRetry = true;  // 允许 join 失败后 fallback 到 create
    QJsonObject msg;
    msg["type"]    = "join_room";
    msg["room_id"] = QString::fromStdString(roomId);
    sendJson(msg);
}

void Oran7SignalingClient::doPublishStream()
{
    if (_streamId.empty()) return;
    QJsonObject msg;
    msg["type"]      = "stream_published";
    msg["stream_id"] = QString::fromStdString(_streamId);
    sendJson(msg);
    INFO_LOG << "[signal] stream_published:" << _streamId.c_str();
}
