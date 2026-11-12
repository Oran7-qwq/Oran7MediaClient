#include "Oran7LiveRoomInfoService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUrl>

namespace {
QString imageUrl(QString value)
{
    if (value.startsWith("//")) value.prepend("https:");
    QUrl url(value);
    const auto host = url.host().toLower();
    if ((url.scheme() != "http" && url.scheme() != "https")
        || (!host.endsWith(".hdslb.com") && !host.endsWith(".bilibili.com"))) return {};
    url.setScheme("https");
    return url.toString();
}
}
Oran7LiveRoomInfoService::Oran7LiveRoomInfoService(std::unique_ptr<QNetworkAccessManager> network)
    : m_network(network ? std::move(network) : std::make_unique<QNetworkAccessManager>())
{
}
Oran7LiveRoomInfoService::~Oran7LiveRoomInfoService() { shutdown(); m_network.reset(); }
void Oran7LiveRoomInfoService::request(const QString& room,bool force)
{
    if (m_nextRefresh.size()>100) {
        const auto now=QDateTime::currentMSecsSinceEpoch();
        for (auto it=m_nextRefresh.begin();it!=m_nextRefresh.end();) {
            if (it.value()<=now) it=m_nextRefresh.erase(it);
            else ++it;
        }
    }
    bool valid = false;
    const auto number = room.toULongLong(&valid);
    if (m_stopped || !valid || !number || m_pending.contains(room) || m_queue.size() >= 50
        || (!force && m_nextRefresh.value(room)>QDateTime::currentMSecsSinceEpoch())) return;
    m_pending.insert(room);
    m_queue.enqueue(room);
    emit started(room);
    pump();
}
void Oran7LiveRoomInfoService::pump()
{
    while (!m_stopped && m_active<2 && !m_queue.isEmpty()) {
        const auto room = m_queue.dequeue();
        ++m_active;
        get(room,"/room/v1/Room/get_info?room_id="+room,[this,room](const QJsonObject& data) {
            const auto realId = data.value("room_id").toVariant().toLongLong();
            if (realId<=0) { finish(room,{}); return; }
            const QString canonical = QString::number(realId);
            QVariantMap info{{"realRoomId",canonical},{"roomTitle",data.value("title").toString()},
                {"coverUrl",imageUrl(data.value("user_cover").toString().isEmpty()
                    ? data.value("keyframe").toString() : data.value("user_cover").toString())},
                {"liveStatus",data.value("live_status").toInt(-1)}};
            get(room,"/live_user/v1/UserInfo/get_anchor_in_room?roomid="+canonical,[this,room,info](const QJsonObject& anchor) mutable {
                const auto base = anchor.value("info").toObject();
                info["anchorName"] = base.value("uname").toString();
                info["avatarUrl"] = imageUrl(base.value("face").toString());
                info["infoUpdatedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
                finish(room,info);
            });
        });
    }
}
void Oran7LiveRoomInfoService::get(const QString& room,const QString& path,
    std::function<void(const QJsonObject&)> done)
{
    QNetworkRequest request(QUrl("https://api.live.bilibili.com"+path));
    request.setTransferTimeout(10000);
    request.setRawHeader("Referer","https://live.bilibili.com/");
    request.setHeader(QNetworkRequest::UserAgentHeader,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
    auto* reply = m_network->get(request);
    m_replies.insert(reply);
    connect(reply,&QNetworkReply::finished,this,[this,reply,room,done=std::move(done)] {
        m_replies.remove(reply);
        const auto json = QJsonDocument::fromJson(reply->readAll()).object();
        const bool valid = reply->error()==QNetworkReply::NoError && json.value("code").toInt(-1)==0;
        reply->deleteLater();
        if (m_stopped) return;
        if (!valid) { done({}); return; }
        done(json.value("data").toObject());
    });
}
void Oran7LiveRoomInfoService::finish(const QString& room,const QVariantMap& info)
{
    m_pending.remove(room);
    --m_active;
    m_nextRefresh.insert(room,QDateTime::currentMSecsSinceEpoch()+(info.isEmpty()?30000:120000));
    emit finished(room,info);
    pump();
}
void Oran7LiveRoomInfoService::shutdown()
{
    m_stopped=true;
    m_queue.clear();
    const auto replies=m_replies;
    for (auto* reply : replies) reply->abort();
}
