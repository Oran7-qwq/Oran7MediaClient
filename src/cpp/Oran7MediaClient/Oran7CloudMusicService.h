#pragma once
#include "Oran7TaskHandle.h"
#include <QObject>
#include <QElapsedTimer>
#include <QVariantMap>
#include <functional>
class Oran7TrackLibrary;

// 收藏缓存及云端请求策略；界面只提交刷新/点赞意图。
class Oran7CloudMusicService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool likedFetching READ likedFetching NOTIFY likedStatusChanged)
    Q_PROPERTY(bool likedLoaded READ likedLoaded NOTIFY likedStatusChanged)
    Q_PROPERTY(bool likedDirty READ likedDirty NOTIFY likedStatusChanged)
public:
    struct Oran7Hooks {
        std::function<QVariantMap()> credentials;
        std::function<QVariant(const QString&, const QVariantMap&)> request;
        std::function<void(qint64)> setPlaylistId;
        std::function<void(qint64, bool)> setLike;
    };
    Oran7CloudMusicService(Oran7TrackLibrary& tracks, Oran7Hooks hooks);
    ~Oran7CloudMusicService() override;
    bool likedFetching() const { return m_fetching; }
    bool likedLoaded() const { return m_loaded; }
    bool likedDirty() const { return m_dirty; }
    bool loggedIn() const;
    void syncSession();
    void shutdown();
    Q_INVOKABLE bool fetchLiked(bool force = false);
    Q_INVOKABLE void markLikedDirty();
    Q_INVOKABLE bool toggleLike(const QVariantMap& track);
    quint64 requestRoam(qint64 seed);
    void cancelRoam();
signals:
    void likedStatusChanged();
    void likedChanged(qint64 songId, bool liked);
    void likedFetchFinished(int count);
    void sessionChanged();
    void roamReady(quint64 request, const QVariantList& songs);
    void roamFailed(quint64 request);
private:
    bool updateSession();
    bool sameSession(const QString& cookie) const;
    Oran7TrackLibrary& m_tracks;
    Oran7Hooks m_hooks;
    QString m_cookie;
    qint64 m_playlistId = 0;
    quint64 m_session = 0, m_roamRevision = 0;
    bool m_fetching = false, m_loaded = false, m_dirty = false, m_closed = false;
    QElapsedTimer m_loadedAt, m_likeAt;
    Oran7TaskHandle m_likedTask, m_roamTask;
};
