#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <functional>

class NcmSongApi;

// Client 独占的地址解析服务。只在 GUI 线程使用，不持有播放器或界面状态。
class Oran7NetworkSongResolver : public QObject
{
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;
    explicit Oran7NetworkSongResolver(NcmSongApi* api, Clock clock = {});

    void resolve(qint64 songId, const QString& fallbackUrl, bool isVip);
    void cancel();
    // 点击正在播放的歌时保留本歌的后台解析，撤销其他歌曲的待处理请求。
    void retainCurrentSong(qint64 songId);
    void playbackOpenFailed(const QString& source, qint64 songId);
    bool isCurrent(quint64 generation) const { return generation == m_generation; }
    quint64 generation() const { return m_generation; }
    static bool isNetworkUrl(const QString& path);
    static qint64 extractSongId(const QString& path);

signals:
    void identityResolved(quint64 generation, qint64 songId, bool isVip);
    void urlReady(quint64 generation, const QString& url);
    // 打开失败保留 Client 已设置的错误；地址解析失败提供明确错误文本。
    void failed(quint64 generation, qint64 songId, const QString& error);

private:
    void requestUrl(qint64 songId);
    void acceptResolved(quint64 token, const QHash<qint64, QString>& urls);
    QString freshCachedUrl(qint64 songId);

    QPointer<NcmSongApi> m_api; // API 由 NcmSession 持有，非所有权引用。
    Clock m_clock;
    QTimer m_retryTimer;
    quint64 m_generation = 0;
    quint64 m_pendingToken = 0;
    qint64 m_pendingSongId = 0;
    qint64 m_requestedSongId = 0;
    bool m_fallbackStarted = false;
    bool m_resolveRetried = false;
    bool m_openRetried = false;
    struct CachedUrl { QString url; qint64 cachedAt; };
    QHash<qint64, CachedUrl> m_cache;
};
