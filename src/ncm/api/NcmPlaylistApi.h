#pragma once
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QVariantList>
#include <functional>
class CurlHttpClient;

// 仅保留现行写操作；收藏/漫游读取由共享异步 handler 执行。
class NcmPlaylistApi : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 likedPlaylistId READ likedPlaylistId WRITE setLikedPlaylistId NOTIFY likedPlaylistIdChanged FINAL)
public:
    explicit NcmPlaylistApi(CurlHttpClient* curl, QObject* parent = nullptr);
    void setCookieProvider(std::function<QString()> provider);
    void setCsrfProvider(std::function<QString()> provider);
    qint64 likedPlaylistId() const { return m_likedPlaylistId; }
    void setLikedPlaylistId(qint64 id);
    Q_INVOKABLE quint64 setLike(qint64 songId, bool like);
    Q_INVOKABLE quint64 reorderLikedSongs(const QVariantList& songIds);
signals:
    void likedPlaylistIdChanged();
    void likeResult(quint64 requestId, qint64 songId, bool like, bool success, const QString& message);
    void reorderResult(quint64 requestId, bool success, const QString& message);
private:
    struct Oran7Operation {
        QString tag, cookie;
        qint64 songId = 0, playlistId = 0;
        bool like = false;
    };
    void finish(quint64 id, const QString& tag, int status, const QByteArray& body, const QString& error = {});
    QPointer<CurlHttpClient> m_curl;
    std::function<QString()> m_cookieProvider, m_csrfProvider;
    QHash<quint64, Oran7Operation> m_pending;
    qint64 m_likedPlaylistId = 0;
    quint64 m_latestReorder = 0;
};
