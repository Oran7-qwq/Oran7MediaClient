#include "Oran7NetworkSongResolver.h"
#include "NcmSongApi.h"
#include <QDateTime>
#include <QRegularExpression>

#include <utility>

Oran7NetworkSongResolver::Oran7NetworkSongResolver(NcmSongApi* api, Clock clock)
    : m_api(api), m_clock(clock ? std::move(clock) : Clock(QDateTime::currentMSecsSinceEpoch))
{
    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(3000);
    connect(&m_retryTimer, &QTimer::timeout, this, [this] { requestUrl(m_pendingSongId); });
    if (api) connect(api, &NcmSongApi::urlsResolved,
                     this, &Oran7NetworkSongResolver::acceptResolved);
}

bool Oran7NetworkSongResolver::isNetworkUrl(const QString& path)
{
    return path.startsWith(QStringLiteral("http://")) || path.startsWith(QStringLiteral("https://"));
}

qint64 Oran7NetworkSongResolver::extractSongId(const QString& path)
{
    static const QRegularExpression expression(QStringLiteral("(?:id=|song_)(\\d+)"));
    const auto match = expression.match(path);
    return match.hasMatch() ? match.captured(1).toLongLong() : -1;
}

void Oran7NetworkSongResolver::cancel()
{
    ++m_generation;
    m_retryTimer.stop();
    m_pendingToken = 0;
    m_pendingSongId = 0;
    m_requestedSongId = 0;
    m_fallbackStarted = false;
    m_resolveRetried = false;
    m_openRetried = false;
}

void Oran7NetworkSongResolver::retainCurrentSong(qint64 songId)
{
    if (songId == m_requestedSongId) return;
    cancel();
    m_requestedSongId = songId;
}

void Oran7NetworkSongResolver::resolve(qint64 songId, const QString& fallbackUrl, bool isVip)
{
    cancel();
    m_requestedSongId = songId;
    const quint64 generation = m_generation;
    QPointer<Oran7NetworkSongResolver> guard(this);
    if (songId > 0 && (isNetworkUrl(fallbackUrl) || fallbackUrl.isEmpty())) {
        emit identityResolved(generation, songId, isVip);
        if (!guard || !isCurrent(generation)) return;
        const QString cached = freshCachedUrl(songId);
        if (!cached.isEmpty()) {
            emit urlReady(generation, cached);
            return;
        }
        if (!isVip) {
            QString directUrl = fallbackUrl;
            // 持久化 CDN 地址会过期；先用稳定外链，后台结果只补充缓存。
            if (!isNetworkUrl(directUrl) || directUrl.contains(QStringLiteral("music.126.net")))
                directUrl = QStringLiteral("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(songId);
            m_fallbackStarted = true;
            emit urlReady(generation, directUrl);
            if (!guard || !isCurrent(generation)) return;
        }
        requestUrl(songId);
        return;
    }
    if (isNetworkUrl(fallbackUrl)) {
        const qint64 extracted = extractSongId(fallbackUrl);
        if (extracted > 0) {
            emit identityResolved(generation, extracted, isVip);
            if (!guard || !isCurrent(generation)) return;
            requestUrl(extracted);
            return;
        }
    }
    // 无歌曲 ID 的旧网络记录和本地路径继续沿用直接播放。
    emit urlReady(generation, fallbackUrl);
}

void Oran7NetworkSongResolver::requestUrl(qint64 songId)
{
    if (songId <= 0) return;
    m_pendingSongId = songId;
    m_pendingToken = NcmRequest::nextId();
    if (m_api) m_api->resolveUrls({songId}, m_pendingToken);
    else acceptResolved(m_pendingToken, {});
}

void Oran7NetworkSongResolver::acceptResolved(quint64 token, const QHash<qint64, QString>& urls)
{
    // 普通查询（0）、已取消/重复/旧尝试的结果均不得消费本轮请求。
    if (token == 0 || token != m_pendingToken || m_pendingSongId <= 0) return;
    m_pendingToken = 0;
    const qint64 songId = m_pendingSongId;
    const QString url = urls.value(songId);
    if (url.isEmpty()) {
        if (m_fallbackStarted) {
            m_fallbackStarted = false;
            return;
        }
        if (!m_resolveRetried) {
            m_resolveRetried = true;
            m_retryTimer.start();
            return;
        }
        m_pendingSongId = 0;
        m_resolveRetried = false;
        emit failed(m_generation, songId, QStringLiteral("歌曲播放地址解析失败"));
        return;
    }
    m_pendingSongId = 0;
    m_resolveRetried = false;
    m_cache.insert(songId, {url, m_clock()});
    if (m_fallbackStarted) {
        m_fallbackStarted = false;
        return;
    }
    emit urlReady(m_generation, url);
}

QString Oran7NetworkSongResolver::freshCachedUrl(qint64 songId)
{
    auto entry = m_cache.find(songId);
    if (songId <= 0 || entry == m_cache.end()) return {};
    if (!entry->url.isEmpty() && m_clock() - entry->cachedAt < 20 * 60 * 1000)
        return entry->url;
    m_cache.erase(entry);
    return {};
}

void Oran7NetworkSongResolver::playbackOpenFailed(const QString& source, qint64 songId)
{
    if (!isNetworkUrl(source)) return;
    const QString cached = freshCachedUrl(songId);
    if (!cached.isEmpty() && !m_openRetried) {
        m_openRetried = true;
        emit urlReady(m_generation, cached);
        return;
    }
    emit failed(m_generation, songId, {});
}
