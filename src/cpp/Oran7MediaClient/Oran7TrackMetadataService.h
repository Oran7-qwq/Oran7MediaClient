#pragma once
#include <QObject>
#include <QPointer>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

// 只获取歌曲详情值，不读写播放状态或配置。结果携带提交时的快照版本。
class Oran7TrackMetadataService : public QObject
{
    Q_OBJECT
public:
    Oran7TrackMetadataService();
    explicit Oran7TrackMetadataService(std::unique_ptr<QNetworkAccessManager> manager);
    ~Oran7TrackMetadataService() override;
    void fetch(qint64 songId, quint64 trackRevision);
    void cancel();
signals:
    void detailsReady(qint64 songId, quint64 trackRevision, const QString& album, const QString& timesize);
private:
    std::unique_ptr<QNetworkAccessManager> m_manager;
    QPointer<QNetworkReply> m_reply; // reply 由 manager 的 QObject 树持有。
    quint64 m_generation = 0;
};
