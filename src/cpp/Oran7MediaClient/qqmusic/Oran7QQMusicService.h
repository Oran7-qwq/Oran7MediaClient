#pragma once
#include "Oran7QQMusicApi.h"
#include "../Oran7Track.h"
#include <QCache>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <functional>
#include <memory>
class Oran7TrackLibrary;
class Oran7QQMusicSession;

// QQ 音乐业务服务；列表归集合、播放事实归 PlaybackState，此处不复制它们。
class Oran7QQMusicService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool searchBusy READ searchBusy NOTIFY statusChanged)
    Q_PROPERTY(bool likedBusy READ likedBusy NOTIFY statusChanged)
    Q_PROPERTY(bool likedLoaded READ likedLoaded NOTIFY statusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)
  public:
    using Credentials = std::function<Oran7QQMusicAuth()>;
    Oran7QQMusicService(Oran7TrackLibrary& tracks, Oran7QQMusicSession& session);
    Oran7QQMusicService(Oran7TrackLibrary& tracks, Credentials credentials, Oran7QQMusicApi::Oran7Options options = {});
    ~Oran7QQMusicService() override;
    bool searchBusy() const { return m_searchRequest != 0; }
    bool likedBusy() const { return m_likedRequest != 0; }
    bool likedLoaded() const { return m_likedLoaded; }
    QString errorMessage() const { return m_error; }
    Q_INVOKABLE void search(const QString& keyword);
    Q_INVOKABLE void fetchLiked(bool force = false);
    Q_INVOKABLE bool toggleLike(const QVariantMap& fields);
    Q_INVOKABLE bool isLiked(const QString& path) const;
    void syncSession();
    void resolve(const Oran7Track& track);
    void cancelResolve();
    bool matchesSource(const QString& path, const QString& url) const;
    void playbackOpenFailed(const Oran7Track& track, const QString& source);
    void loadLyrics(const Oran7Track& track);
    void cancelLyrics();
    void shutdown();
    static QVariantMap mapSong(QJsonObject song);
  signals:
    void statusChanged();
    void urlReady(const QString& path, const QString& url);
    void resolveFailed(const QString& path, const QString& error);
    void lyricsReady(const QString& path);
    void lyricsUnavailable();

  private:
    enum class Kind
    {
        Search,
        Liked,
        Detail,
        Resolve,
        LikeDetail,
        Like,
        Lyrics
    };
    struct Oran7Operation
    {
        Kind kind;
        Oran7Track track;
        int offset = 0;
        bool like = false;
    };
    struct Oran7CachedUrl
    {
        QString url;
        qint64 expiry = 0;
    };
    quint64 request(Oran7Operation operation, const QString& module, const QString& method,
                    const QJsonObject& parameters, bool write = false);
    void cancelRequest(quint64& id);
    void completed(quint64 id, const QJsonObject& data, const QString& error);
    void likedPage(int offset);
    void resolveUrl(const Oran7Track& track, bool cache);
    void failResolve(const QString& path, const QString& error);
    Oran7TrackLibrary& m_tracks;
    Credentials m_credentials;
    Oran7QQMusicApi m_api;
    QHash<quint64, Oran7Operation> m_operations;
    QElapsedTimer m_clock;
    QCache<QString, Oran7CachedUrl> m_urls{64};
    QByteArray m_account;
    QVariantList m_likedPages;
    QString m_error, m_resolvedPath, m_resolvedUrl;
    std::unique_ptr<QTemporaryDir> m_lyricsDirectory;
    quint64 m_searchRequest = 0, m_likedRequest = 0, m_resolveRequest = 0, m_likeRequest = 0, m_lyricRequest = 0,
            m_resolveGeneration = 0;
    qint64 m_likedAt = -30000, m_lastLike = -2000;
    bool m_likedLoaded = false, m_openRetry = true, m_closed = false;
};
