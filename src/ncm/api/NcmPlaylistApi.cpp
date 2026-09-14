#include "NcmPlaylistApi.h"

#include "CurlHttpClient.h"
#include "GlobalHelper.h"
#include "HttpClient.h"
#include "NcmProtocol.h"
#include "NcmRequest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

#include <utility>

namespace {
const QByteArray kPlaylistUA =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120 Safari/537.36";
}

NcmPlaylistApi::NcmPlaylistApi(HttpClient* http, CurlHttpClient* curl, QObject* parent)
    : QObject(parent)
    , m_http(http)
    , m_curl(curl)
{
    connect(m_curl, &CurlHttpClient::requestFinished, this,
            [this](int httpStatus, const QByteArray& body, const QString&, const QString& tag) {
                if (tag == QLatin1String("account"))
                    onAccountReply(httpStatus, body);
                else if (tag == QLatin1String("like"))
                    onLikeResult(httpStatus, body, tag);
                else if (tag == QLatin1String("reorder"))
                    onReorderResult(httpStatus, body);
                else if (tag == QLatin1String("userPlaylists"))
                    onUserPlaylistsReply(httpStatus, body);
                else if (tag == QLatin1String("playlistDetail"))
                    onPlaylistDetailReply(httpStatus, body);
                else if (tag == QLatin1String("songDetail"))
                    onSongDetailReply(httpStatus, body);
                else if (tag == QLatin1String("roam"))
                    onRoamReply(httpStatus, body);
            });
    connect(m_curl, &CurlHttpClient::requestError, this,
            [this](const QString& msg, const QString& tag) {
                if (tag == QLatin1String("account")
                    || tag == QLatin1String("userPlaylists")
                    || tag == QLatin1String("playlistDetail")
                    || tag == QLatin1String("songDetail")) {
                    m_fetchInProgress = false;
                    emit likedSongsFailed(msg);
                } else if (tag == QLatin1String("like"))
                    emit likeResult(m_lastLikeSongId, m_lastLikeValue, false, msg);
                else if (tag == QLatin1String("reorder"))
                    emit reorderResult(false, msg);
                else if (tag == QLatin1String("roam"))
                    emit roamSongsFailed(msg);
            });
}

void NcmPlaylistApi::setCookieProvider(std::function<QString()> provider)
{
    m_cookieProvider = std::move(provider);
}

void NcmPlaylistApi::setCsrfProvider(std::function<QString()> provider)
{
    m_csrfProvider = std::move(provider);
}

qint64 NcmPlaylistApi::likedPlaylistId() const
{
    return m_likedPlaylistId;
}

void NcmPlaylistApi::setLikedPlaylistId(qint64 id)
{
    if (m_likedPlaylistId == id)
        return;
    m_likedPlaylistId = id;
    emit likedPlaylistIdChanged();
}

void NcmPlaylistApi::fetchLikedSongs()
{
    // 拉取进行中则忽略新请求，避免并发链共享 m_pendingTrackIds/m_likedIdOrder 互相覆盖导致乱序
    if (m_fetchInProgress)
        return;
    m_fetchInProgress = true;
    m_likedPlaylistId = 0;   // 拉取开始先清空，避免上次的旧歌单 id 残留

    // 阶段1：明文 POST /api/nuser/account/get 拿 userId（走 curl 通道，避免 QNAM 被 WAF 拦）
    QString csrf;
    if (m_csrfProvider)
        csrf = m_csrfProvider();

    NcmRequest req;
    req.url = QUrl(QStringLiteral("https://music.163.com/api/nuser/account/get"));
    req.isPost = true;
    req.body = QStringLiteral("csrf_token=%1").arg(csrf).toUtf8();
    req.headers.insert(QByteArrayLiteral("Content-Type"),
                       QByteArrayLiteral("application/x-www-form-urlencoded"));
    req.headers.insert(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://music.163.com"));
    req.headers.insert(QByteArrayLiteral("User-Agent"), kPlaylistUA);
    // 关键：明文 account/get 必须带登录 Cookie，否则返回 profile:null 拿不到 userId
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }

    m_curl->send(req, QStringLiteral("account"));
}

void NcmPlaylistApi::onAccountReply(int httpStatus, const QByteArray& body)
{
    if (httpStatus != 200 || body.trimmed().isEmpty()) {
        m_fetchInProgress = false;
        emit likedSongsFailed(QStringLiteral("获取账号信息失败"));
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const qint64 uid = root["profile"].toObject()["userId"].toVariant().toLongLong();
    if (uid <= 0) {
        m_fetchInProgress = false;
        emit likedSongsFailed(QStringLiteral("获取账号信息失败"));
        return;
    }

    // 阶段2：按 uid 找"我喜欢的音乐"歌单（specialType==5），再取其 trackIds 拿到保序 id 列表。
    // 不用 /weapi/song/like/get（likelist）：其 ids 官方标注"无序"，会导致列表乱序。
    fetchLikedPlaylist(uid);
}

void NcmPlaylistApi::fetchLikedPlaylist(qint64 uid)
{
    QUrl url(QStringLiteral("https://music.163.com/api/user/playlist"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("uid"), QString::number(uid));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    url.setQuery(q);

    NcmRequest req;
    req.url = url;
    req.isPost = false;
    req.headers.insert(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://music.163.com"));
    req.headers.insert(QByteArrayLiteral("User-Agent"), kPlaylistUA);
    // 走 curl 通道必须手动注入 Cookie（curl 不会自动带），否则歌单列表不含"我喜欢的音乐"
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }

    // 走 curl 通道（与 account/get 一致），规避 QNAM 在部分网络下被网易云 WAF 拦截返回空响应
    m_curl->send(req, QStringLiteral("userPlaylists"));
}

void NcmPlaylistApi::onUserPlaylistsReply(int httpStatus, const QByteArray& body)
{
    if (httpStatus != 200 || body.trimmed().isEmpty()) {
        m_fetchInProgress = false;
        emit likedSongsFailed(QStringLiteral("获取歌单失败(http=%1)").arg(httpStatus));
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonArray playlists = root["playlist"].toArray();

    qint64 likedId = 0;
    for (const QJsonValue& v : playlists) {
        const QJsonObject p = v.toObject();
        const qint64 id = p["id"].toVariant().toLongLong();
        if (id <= 0)
            continue;
        if (p["specialType"].toInt() == 5) {
            // "我喜欢的音乐"歌单的 specialType 固定为 5，命中即用
            likedId = id;
            break;
        }
        if (likedId == 0 && p["name"].toString().endsWith(QStringLiteral("喜欢的音乐")))
            likedId = id;   // 名字兜底，继续找 specialType==5
    }

    if (likedId <= 0) {
        m_fetchInProgress = false;
        NETWORK_LOG << "NcmPlaylistApi: 歌单列表无'我喜欢的音乐'(playlist count=" << playlists.size()
                    << ") body=" << QString::fromUtf8(body).left(200);
        emit likedSongsFailed(QStringLiteral("未找到'我喜欢的音乐'歌单"));
        return;
    }
    setLikedPlaylistId(likedId);   // 保存歌单 id，拖拽排序同步（reorderLikedSongs）用 pid
    fetchPlaylistDetail(likedId);
}

void NcmPlaylistApi::fetchPlaylistDetail(qint64 playlistId)
{
    QUrl url(QStringLiteral("https://music.163.com/api/v6/playlist/detail"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("id"), QString::number(playlistId));
    url.setQuery(q);

    NcmRequest req;
    req.url = url;
    req.isPost = false;
    req.headers.insert(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://music.163.com"));
    req.headers.insert(QByteArrayLiteral("User-Agent"), kPlaylistUA);
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }

    m_curl->send(req, QStringLiteral("playlistDetail"));
}

void NcmPlaylistApi::onPlaylistDetailReply(int httpStatus, const QByteArray& body)
{
    if (httpStatus != 200 || body.trimmed().isEmpty()) {
        m_fetchInProgress = false;
        emit likedSongsFailed(QStringLiteral("获取歌单详情失败(http=%1)").arg(httpStatus));
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonObject playlist = root["playlist"].toObject();
    const QJsonArray trackIds = playlist["trackIds"].toArray();

    m_pendingTrackIds.clear();
    m_likedIdOrder.clear();
    m_songMetaMap.clear();
    for (const QJsonValue& v : trackIds) {
        const qint64 id = v.toObject()["id"].toVariant().toLongLong();
        if (id > 0) {
            m_pendingTrackIds.append(id);
            m_likedIdOrder.append(id);   // 记录歌单 trackIds 顺序 = 云端"我喜欢的音乐"顺序
        }
    }
    if (m_pendingTrackIds.isEmpty()) {
        m_fetchInProgress = false;
        emit likedSongsFailed(QStringLiteral("歌单中无歌曲"));
        return;
    }
    m_totalTrackCount = m_pendingTrackIds.size();

    // 阶段3：分批 song/detail
    fetchNextSongDetailBatch();
}

void NcmPlaylistApi::fetchNextSongDetailBatch()
{
    if (m_pendingTrackIds.isEmpty()) {
        // 全部分批完成：按歌单 trackIds 顺序逐条 emit，保证列表顺序与云端一致
        for (qint64 id : std::as_const(m_likedIdOrder)) {
            const QVariantMap meta = m_songMetaMap.value(id);
            if (meta.isEmpty())
                continue;
            emit likedSongReady(id, meta.value("icon").toString(),
                                meta.value("name").toString(), meta.value("artist").toString(),
                                meta.value("album").toString(), meta.value("timesize").toString(),
                                meta.value("isVip").toBool());
        }
        m_songMetaMap.clear();
        m_fetchInProgress = false;
        emit likedSongsFinished(m_totalTrackCount);
        return;
    }

    QList<qint64> batch;
    while (!m_pendingTrackIds.isEmpty() && batch.size() < 100)
        batch.append(m_pendingTrackIds.takeFirst());

    QStringList idStrs;
    idStrs.reserve(batch.size());
    for (qint64 id : batch)
        idStrs << QString::number(id);

    QUrl url(QStringLiteral("https://music.163.com/api/song/detail"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("ids"),
                   QStringLiteral("[%1]").arg(idStrs.join(QLatin1Char(','))));
    url.setQuery(q);

    NcmRequest req;
    req.url = url;
    req.isPost = false;
    req.headers.insert(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://music.163.com"));
    req.headers.insert(QByteArrayLiteral("User-Agent"), kPlaylistUA);
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }

    m_curl->send(req, QStringLiteral("songDetail"));
}

void NcmPlaylistApi::onSongDetailReply(int httpStatus, const QByteArray& body)
{
    if (httpStatus != 200 || body.trimmed().isEmpty()) {
        // 单批失败不终止整个拉取：记录日志，继续下一批（部分歌可能缺失）
        NETWORK_LOG << "NcmPlaylistApi: songDetail batch failed http=" << httpStatus;
        fetchNextSongDetailBatch();
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonArray songs = root["songs"].toArray();

    for (const QJsonValue& v : songs) {
        const QJsonObject song = v.toObject();
        const qint64 id = song["id"].toVariant().toLongLong();
        if (id <= 0)
            continue;
        const QString name = song["name"].toString();

        QStringList artistNames;
        const QJsonArray artists = song["artists"].toArray();
        for (const QJsonValue& a : artists) {
            const QString nm = a.toObject()["name"].toString();
            if (!nm.isEmpty())
                artistNames << nm;
        }

        const QJsonObject album = song["album"].toObject();
        const QString albumName = album["name"].toString();
        QString pic = album["picUrl"].toString();
        if (pic.isEmpty() && !artists.isEmpty())
            pic = artists.first().toObject()["picUrl"].toString();

        const qint64 durationMs = song["duration"].toVariant().toLongLong();
        const QString timesize = QString::number(static_cast<int>(durationMs / 1000));
        const bool isVip = song["fee"].toInt() == 1;

        // 先缓存，待所有批次完成后再按歌单 trackIds 顺序统一 emit
        m_songMetaMap.insert(id, {
            { QStringLiteral("icon"), pic },
            { QStringLiteral("name"), name },
            { QStringLiteral("artist"), artistNames.join(QStringLiteral(" / ")) },
            { QStringLiteral("album"), albumName },
            { QStringLiteral("timesize"), timesize },
            { QStringLiteral("isVip"), isVip }
        });
    }

    fetchNextSongDetailBatch();
}

void NcmPlaylistApi::setLike(qint64 songId, bool like)
{
    if (songId <= 0)
        return;
    m_lastLikeSongId = songId;
    m_lastLikeValue = like;

    QString csrf;
    if (m_csrfProvider)
        csrf = m_csrfProvider();

    QJsonObject payload;
    payload["like"] = like;
    payload["trackId"] = songId;
    if (!csrf.isEmpty())
        payload["csrf_token"] = csrf;

    NcmProtocol protocol;
    // 2026 年网易云 like 接口是 /weapi/song/like（返回 {playlistId, code:200}）；
    // 旧 /weapi/playlist/like 已 404。
    NcmRequest req = protocol.prepareWeapi(
        QStringLiteral("/weapi/song/like?csrf_token=%1").arg(csrf), payload);
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }
    m_curl->send(req, QStringLiteral("like"));
}

void NcmPlaylistApi::onLikeResult(int httpStatus, const QByteArray& body, const QString& tag)
{
    Q_UNUSED(tag);
    bool ok = (httpStatus == 200);
    if (ok) {
        const QJsonObject root = QJsonDocument::fromJson(body).object();
        ok = (root["code"].toInt() == 200);
    }
    emit likeResult(m_lastLikeSongId, m_lastLikeValue, ok,
                    ok ? QString() : QStringLiteral("同步网易云失败"));
}

void NcmPlaylistApi::reorderLikedSongs(const QVariantList& songIds)
{
    // 拖拽排序同步：把"我喜欢的音乐"歌单的新顺序提交给网易云官方服务器
    if (m_likedPlaylistId <= 0) {
        emit reorderResult(false, QStringLiteral("未获取到'我喜欢的音乐'歌单ID"));
        return;
    }

    QJsonArray idArr;
    for (const QVariant& v : songIds) {
        const qint64 id = v.toLongLong();
        if (id > 0)
            idArr.append(id);
    }
    if (idArr.isEmpty()) {
        emit reorderResult(false, QStringLiteral("新的歌曲顺序为空"));
        return;
    }

    QString csrf;
    if (m_csrfProvider)
        csrf = m_csrfProvider();

    QJsonObject payload;
    // op='update' 走 /weapi/playlist/manipulate/tracks 重排歌单歌曲（旧 /weapi/song/order/update 已 404）
    payload["op"] = QStringLiteral("update");
    payload["pid"] = m_likedPlaylistId;
    // trackIds 为完整歌曲 id 列表的 JSON 字符串（"[1,2,3]"），按新顺序排列
    payload["trackIds"] = QString::fromUtf8(QJsonDocument(idArr).toJson(QJsonDocument::Compact));
    payload["imme"] = true;
    if (!csrf.isEmpty())
        payload["csrf_token"] = csrf;

    NcmProtocol protocol;
    NcmRequest req = protocol.prepareWeapi(
        QStringLiteral("/weapi/playlist/manipulate/tracks?csrf_token=%1").arg(csrf), payload);
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }
    m_curl->send(req, QStringLiteral("reorder"));
}

void NcmPlaylistApi::onReorderResult(int httpStatus, const QByteArray& body)
{
    bool ok = (httpStatus == 200);
    if (ok) {
        const QJsonObject root = QJsonDocument::fromJson(body).object();
        ok = (root["code"].toInt() == 200);
        if (!ok)
            NETWORK_LOG << "reorder failed body=" << QString::fromUtf8(body);
    }
    emit reorderResult(ok,
                       ok ? QString() : QStringLiteral("同步网易云排序失败"));
}

void NcmPlaylistApi::roamSongs(qint64 songId)
{
    if (songId <= 0) {
        emit roamSongsFailed(QStringLiteral("当前歌曲无 songId，无法漫游"));
        return;
    }
    if (m_likedPlaylistId <= 0) {
        emit roamSongsFailed(QStringLiteral("未获取到'我喜欢的音乐'歌单ID"));
        return;
    }

    QString csrf;
    if (m_csrfProvider)
        csrf = m_csrfProvider();

    QJsonObject payload;
    // 与 binaryify playmode_intelligence_list 一致：songId/type/playlistId/startMusicId/count
    payload["songId"] = songId;
    payload["type"] = QStringLiteral("fromPlayOne");
    payload["playlistId"] = m_likedPlaylistId;
    payload["startMusicId"] = songId;
    payload["count"] = 20;
    if (!csrf.isEmpty())
        payload["csrf_token"] = csrf;

    NcmProtocol protocol;
    NcmRequest req = protocol.prepareWeapi(
        QStringLiteral("/weapi/playmode/intelligence/list?csrf_token=%1").arg(csrf), payload);
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }
    m_curl->send(req, QStringLiteral("roam"));
}

void NcmPlaylistApi::onRoamReply(int httpStatus, const QByteArray& body)
{
    if (httpStatus != 200 || body.trimmed().isEmpty()) {
        NETWORK_LOG << "roam failed http=" << httpStatus << " body=" << QString::fromUtf8(body).left(160);
        emit roamSongsFailed(QStringLiteral("漫游请求失败"));
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    if (root["code"].toInt() != 200) {
        NETWORK_LOG << "roam failed code=" << root["code"].toInt()
                    << " body=" << QString::fromUtf8(body).left(160);
        emit roamSongsFailed(QStringLiteral("漫游请求失败"));
        return;
    }

    QVariantList songs;
    const QJsonArray data = root["data"].toArray();
    for (const QJsonValue& v : data) {
        const QJsonObject outer = v.toObject();
        // 心动模式返回结构：data[] 每项外层是 {id, alg, recommended, songInfo:{...歌曲元数据...}}
        // 元数据在 songInfo 内；兼容直接平铺的旧结构
        const QJsonObject song = outer["songInfo"].isObject() ? outer["songInfo"].toObject() : outer;
        const qint64 id = song["id"].toVariant().toLongLong();
        if (id <= 0)
            continue;
        const QString name = song["name"].toString();

        QStringList artistNames;
        const QJsonArray artists = song["ar"].toArray();
        for (const QJsonValue& a : artists) {
            const QString nm = a.toObject()["name"].toString();
            if (!nm.isEmpty())
                artistNames << nm;
        }
        if (artists.isEmpty()) {
            const QJsonArray arts = song["artists"].toArray();
            for (const QJsonValue& a : arts) {
                const QString nm = a.toObject()["name"].toString();
                if (!nm.isEmpty())
                    artistNames << nm;
            }
        }

        const QJsonObject album = song["al"].toObject();
        QString albumName = album["name"].toString();
        QString pic = album["picUrl"].toString();
        if (albumName.isEmpty()) {
            const QJsonObject al = song["album"].toObject();
            albumName = al["name"].toString();
            if (pic.isEmpty())
                pic = al["picUrl"].toString();
        }
        if (pic.isEmpty() && !artists.isEmpty())
            pic = artists.first().toObject()["picUrl"].toString();

        const qint64 durationMs = song["dt"].toVariant().toLongLong();
        const QString timesize = QString::number(static_cast<int>(durationMs / 1000));
        const bool isVip = song["fee"].toInt() == 1;

        QVariantMap item;
        item["songId"] = id;
        item["icon"] = pic;
        item["name"] = name;
        item["artist"] = artistNames.join(QStringLiteral(" / "));
        item["album"] = albumName;
        item["timesize"] = timesize;
        item["isVip"] = isVip;
        songs.append(item);
    }

    if (songs.isEmpty()) {
        emit roamSongsFailed(QStringLiteral("无可用漫游推荐"));
        return;
    }
    emit roamSongsReady(songs);
}