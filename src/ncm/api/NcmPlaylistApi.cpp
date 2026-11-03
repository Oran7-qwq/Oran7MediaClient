#include "NcmPlaylistApi.h"
#include "CurlHttpClient.h"
#include "NcmProtocol.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <utility>

NcmPlaylistApi::NcmPlaylistApi(CurlHttpClient* curl, QObject* parent) : QObject(parent), m_curl(curl)
{
    if (!curl) return;
    connect(curl, &CurlHttpClient::requestFinished, this,
        [this](quint64 id, const QString& tag, int status, const QByteArray& body, const QString&) { finish(id, tag, status, body); });
    connect(curl, &CurlHttpClient::requestError, this,
        [this](quint64 id, const QString& tag, const QString& error) { finish(id, tag, 0, {}, error); });
}
void NcmPlaylistApi::setCookieProvider(std::function<QString()> provider) { m_cookieProvider = std::move(provider); }
void NcmPlaylistApi::setCsrfProvider(std::function<QString()> provider) { m_csrfProvider = std::move(provider); }
void NcmPlaylistApi::setLikedPlaylistId(qint64 id)
{
    if (id == m_likedPlaylistId) return;
    m_likedPlaylistId = id;
    emit likedPlaylistIdChanged();
}
quint64 NcmPlaylistApi::setLike(qint64 songId, bool like)
{
    if (songId <= 0) return 0;
    const auto csrf = m_csrfProvider ? m_csrfProvider() : QString();
    const auto cookie = m_cookieProvider ? m_cookieProvider() : QString();
    QJsonObject payload{{"like",like},{"trackId",songId}};
    if (!csrf.isEmpty()) payload["csrf_token"] = csrf;
    NcmProtocol protocol;
    auto req = protocol.prepareWeapi(QStringLiteral("/weapi/song/like?csrf_token=%1").arg(csrf), payload);
    req.tag = "like";
    if (!cookie.isEmpty()) req.headers.insert("Cookie", cookie.toUtf8());
    m_pending.insert(req.id, Oran7Operation{req.tag, cookie, songId, 0, like});
    if (m_curl) m_curl->send(req);
    else finish(req.id, req.tag, 0, {}, QStringLiteral("网易云传输不可用"));
    return req.id;
}
quint64 NcmPlaylistApi::reorderLikedSongs(const QVariantList& songIds)
{
    NcmRequest context;
    if (m_likedPlaylistId <= 0) {
        emit reorderResult(context.id, false, QStringLiteral("未获取到'我喜欢的音乐'歌单ID"));
        return context.id;
    }
    QJsonArray ids;
    for (const auto& value : songIds) if (value.toLongLong() > 0) ids.append(value.toLongLong());
    if (ids.isEmpty()) {
        emit reorderResult(context.id, false, QStringLiteral("新的歌曲顺序为空"));
        return context.id;
    }
    const auto csrf = m_csrfProvider ? m_csrfProvider() : QString();
    const auto cookie = m_cookieProvider ? m_cookieProvider() : QString();
    QJsonObject payload{{"op","update"},{"pid",m_likedPlaylistId},
        {"trackIds",QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact))},{"imme",true}};
    if (!csrf.isEmpty()) payload["csrf_token"] = csrf;
    NcmProtocol protocol;
    auto req = protocol.prepareWeapi(QStringLiteral("/weapi/playlist/manipulate/tracks?csrf_token=%1").arg(csrf), payload);
    req.tag = "reorder";
    if (!cookie.isEmpty()) req.headers.insert("Cookie", cookie.toUtf8());
    m_latestReorder = req.id;
    m_pending.insert(req.id, Oran7Operation{req.tag, cookie, 0, m_likedPlaylistId, false});
    if (m_curl) m_curl->send(req);
    else finish(req.id, req.tag, 0, {}, QStringLiteral("网易云传输不可用"));
    return req.id;
}
void NcmPlaylistApi::finish(quint64 id, const QString& tag, int status, const QByteArray& body, const QString& error)
{
    const auto it = m_pending.constFind(id);
    if (it == m_pending.cend() || it->tag != tag) return;
    const auto operation = m_pending.take(id);
    if (m_cookieProvider && m_cookieProvider() != operation.cookie) return;
    const bool ok = error.isEmpty() && status == 200 && QJsonDocument::fromJson(body).object().value("code").toInt() == 200;
    const auto message = ok ? QString() : (error.isEmpty() ? QStringLiteral("同步网易云失败") : error);
    if (tag == "like") emit likeResult(id, operation.songId, operation.like, ok, message);
    else if (tag == "reorder" && id == m_latestReorder && operation.playlistId == m_likedPlaylistId)
        emit reorderResult(id, ok, message);
}
