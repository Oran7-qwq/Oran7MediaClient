#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QSet>
#include <QHash>
#include <QVariantMap>
#include <memory>
#include <functional>
#include <QJsonObject>

// 房间资料查询，GUI 线程异步网络，最多两个房间并发。
class Oran7LiveRoomInfoService : public QObject
{
    Q_OBJECT
public:
    explicit Oran7LiveRoomInfoService(std::unique_ptr<QNetworkAccessManager> network = {});
    ~Oran7LiveRoomInfoService() override;
    void request(const QString& room, bool force = false);
    void shutdown();
signals:
    void started(const QString& room);
    void finished(const QString& room, const QVariantMap& info);
private:
    void pump();
    void get(const QString& room, const QString& path, std::function<void(const QJsonObject&)> done);
    void finish(const QString& room, const QVariantMap& info);
    std::unique_ptr<QNetworkAccessManager> m_network;
    QQueue<QString> m_queue;
    QSet<QString> m_pending;
    QSet<QNetworkReply*> m_replies;
    QHash<QString,qint64> m_nextRefresh;
    int m_active = 0;
    bool m_stopped = false;
};
