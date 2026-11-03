#include "NcmAsyncHandlers.h"
#include "Oran7NcmJsonMapper.h"

#include "BlockingCurl.h"
#include "Oran7GlobalHelper.h"
#include "NcmProtocol.h"
#include "NcmRequest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <stdexcept>

namespace {

const QByteArray kChromeUA =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120 Safari/537.36";

// 一次请求的返回值（body + http 状态码）
struct Reply
{
    QByteArray body;
    int httpStatus = 0;
};

// 发一次请求；curl 进程级失败时抛异常（致命，交由 Oran7AsyncManager 转 failed 回调）
Reply exec(const NcmRequest& req)
{
    Reply r;
    QString error;
    if (!ncm::blockingCurl(req, &r.body, &r.httpStatus, &error))
        throw std::runtime_error(error.toStdString());
    return r;
}

// 发一次请求；curl 进程级失败时返回 httpStatus=0 的空 body（非致命，由调用方降级处理）
Reply tryExec(const NcmRequest& req)
{
    Reply r;
    QString error;
    ncm::blockingCurl(req, &r.body, &r.httpStatus, &error);
    return r;
}

NcmRequest makePlainGet(const QString& url, const QString& cookie)
{
    NcmRequest req;
    req.url = QUrl(url);
    req.tag = req.url.path();
    req.isPost = false;
    req.headers.insert(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://music.163.com"));
    req.headers.insert(QByteArrayLiteral("User-Agent"), kChromeUA);
    if (!cookie.isEmpty())
        req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    return req;
}

NcmRequest makePlainPost(const QString& url, const QByteArray& body, const QString& cookie)
{
    NcmRequest req;
    req.url = QUrl(url);
    req.tag = req.url.path();
    req.isPost = true;
    req.body = body;
    req.headers.insert(QByteArrayLiteral("Content-Type"),
                       QByteArrayLiteral("application/x-www-form-urlencoded"));
    req.headers.insert(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://music.163.com"));
    req.headers.insert(QByteArrayLiteral("User-Agent"), kChromeUA);
    if (!cookie.isEmpty())
        req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    return req;
}

QHash<qint64, Oran7NcmTrackMeta> fetchSongDetailMeta(const QList<qint64>& ids, const QString& cookie)
{
    QHash<qint64, Oran7NcmTrackMeta> result;
    for (int start = 0; start < ids.size(); start += 100) {
        QStringList batch;
        for (int i = start; i < qMin(start + 100, int(ids.size())); ++i) batch.append(QString::number(ids[i]));
        QUrl url(QStringLiteral("https://music.163.com/api/song/detail"));
        QUrlQuery query;
        query.addQueryItem("ids", QStringLiteral("[%1]").arg(batch.join(',')));
        url.setQuery(query);
        const auto reply = exec(makePlainGet(url.toString(), cookie));
        if (reply.httpStatus != 200 || reply.body.trimmed().isEmpty()) {
            NETWORK_LOG << "NcmAsync: songDetail batch failed http=" << reply.httpStatus;
            continue;
        }
        for (const auto& value : QJsonDocument::fromJson(reply.body).object().value("songs").toArray()) {
            const auto track = Oran7NcmJsonMapper::track(value.toObject());
            if (track) result.insert(track->songId, *track);
        }
    }
    return result;
}

} // namespace

namespace ncm {

QVariant musicSearchHandler(const QVariantMap& args)
{
    const QString keywords = args.value(QLatin1String("keywords")).toString().trimmed();
    if (keywords.isEmpty())
        throw std::runtime_error("搜索关键词为空");

    const QString cookie = args.value(QLatin1String("cookie")).toString();
    const bool isLoggedIn = args.value(QLatin1String("isLoggedIn")).toBool();

    // 阶段1：搜索（搜索阶段不带 Cookie）
    QUrl url(QStringLiteral("https://music.163.com/api/search/get"));
    QUrlQuery query;
    query.addQueryItem(QLatin1String("s"), keywords);
    query.addQueryItem(QLatin1String("type"), QLatin1String("1"));   // 1=单曲
    query.addQueryItem(QLatin1String("limit"), QLatin1String("30"));
    url.setQuery(query);

    const Reply r = exec(makePlainGet(url.toString(), QString()));
    if (r.httpStatus != 200 || r.body.trimmed().isEmpty())
        throw std::runtime_error("网络错误: 搜索请求失败");

    const QJsonObject root = QJsonDocument::fromJson(r.body).object();
    if (root.isEmpty() || root.value(QLatin1String("code")).toInt() != 200)
        throw std::runtime_error(
            QStringLiteral("API错误 code=%1").arg(root.value(QLatin1String("code")).toInt()).toStdString());

    QList<Oran7NcmTrackMeta> pending;
    for (const auto& value : root.value("result").toObject().value("songs").toArray()) {
        const auto track = Oran7NcmJsonMapper::track(value.toObject());
        if (track) pending.append(*track);
    }

    if (pending.isEmpty())
        return QVariantList();   // 对应 searchFinished(0)：成功但 0 条

    // 阶段2：批量专辑封面（失败非致命，退回兜底封面）
    QStringList ids;
    ids.reserve(pending.size());
    for (const Oran7NcmTrackMeta& s : pending)
        ids << QString::number(s.songId);

    QHash<qint64, QString> picMap;
    {
        QUrl durl(QStringLiteral("https://music.163.com/api/song/detail"));
        QUrlQuery dq;
        dq.addQueryItem(QLatin1String("ids"),
                        QStringLiteral("[%1]").arg(ids.join(QLatin1Char(','))));
        durl.setQuery(dq);
        const Reply dr = tryExec(makePlainGet(durl.toString(), QString()));
        if (dr.httpStatus == 200 && !dr.body.trimmed().isEmpty()) {
            const QJsonArray dsongs = QJsonDocument::fromJson(dr.body).object()
                                          .value(QLatin1String("songs")).toArray();
            for (const auto& value : dsongs) {
                const auto track = Oran7NcmJsonMapper::track(value.toObject());
                if (track && !track->cover.isEmpty()) picMap.insert(track->songId, track->cover);
            }
        } else {
            NETWORK_LOG << "NcmAsync: search song/detail failed http=" << dr.httpStatus;
        }
    }

    // 阶段3：已登录且存在 VIP 歌 → 解析真实播放地址（失败非致命，退回外链）
    QHash<qint64, QString> urlMap;
    if (isLoggedIn) {
        QList<qint64> vipIds;
        for (const Oran7NcmTrackMeta& s : pending) {
            if (s.isVip)
                vipIds << s.songId;
        }
        if (!vipIds.isEmpty()) {
            QStringList vidStrs;
            vidStrs.reserve(vipIds.size());
            for (qint64 id : vipIds)
                vidStrs << QString::number(id);

            QUrlQuery vq;
            vq.addQueryItem(QLatin1String("ids"),
                            QStringLiteral("[%1]").arg(vidStrs.join(QLatin1Char(','))));
            vq.addQueryItem(QLatin1String("br"), QLatin1String("320000"));
            // 关键：enhance/player/url 必须带 os=pc + appver（与 NcmSession/NcmSongApi 一致）
            const QString vipCookie =
                cookie.isEmpty() ? QString() : cookie + QStringLiteral("; os=pc; appver=8.10.35");
            const NcmRequest vreq = makePlainPost(
                QStringLiteral("https://music.163.com/api/song/enhance/player/url"),
                vq.toString(QUrl::FullyEncoded).toUtf8(), vipCookie);
            const Reply vr = tryExec(vreq);
            if (vr.httpStatus == 200 && !vr.body.trimmed().isEmpty()) {
                urlMap = Oran7NcmJsonMapper::playbackUrls(QJsonDocument::fromJson(vr.body).object());
            } else {
                NETWORK_LOG << "NcmAsync: search enhance/player/url failed http=" << vr.httpStatus;
            }
        }
    }

    QVariantList out;
    for (auto track : pending) {
        if (picMap.contains(track.songId)) track.cover = picMap.value(track.songId);
        track.url = urlMap.value(track.songId);
        if (track.url.isEmpty())
            track.url = QStringLiteral("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(track.songId);
        out.append(track.toVariantMap());
    }
    return out;
}

QVariant fetchLikedSongsHandler(const QVariantMap& args)
{
    const QString cookie = args.value(QLatin1String("cookie")).toString();
    const QString csrf = args.value(QLatin1String("csrf")).toString();

    // 阶段1：明文 POST /api/nuser/account/get 拿 userId（走 curl 通道，避开 QNAM 被 WAF 拦）
    const Reply r = exec(makePlainPost(QStringLiteral("https://music.163.com/api/nuser/account/get"),
                                       QStringLiteral("csrf_token=%1").arg(csrf).toUtf8(), cookie));
    if (r.httpStatus != 200 || r.body.trimmed().isEmpty())
        throw std::runtime_error("获取账号信息失败");
    const qint64 uid = QJsonDocument::fromJson(r.body).object()
                           .value(QLatin1String("profile")).toObject()
                           .value(QLatin1String("userId")).toVariant().toLongLong();
    if (uid <= 0)
        throw std::runtime_error("获取账号信息失败");

    // 阶段2：按 uid 找"我喜欢的音乐"歌单（specialType==5），名字兜底
    QUrl pUrl(QStringLiteral("https://music.163.com/api/user/playlist"));
    QUrlQuery pq;
    pq.addQueryItem(QLatin1String("uid"), QString::number(uid));
    pq.addQueryItem(QLatin1String("limit"), QLatin1String("100"));
    pUrl.setQuery(pq);
    const Reply pr = exec(makePlainGet(pUrl.toString(), cookie));
    if (pr.httpStatus != 200 || pr.body.trimmed().isEmpty())
        throw std::runtime_error(
            QStringLiteral("获取歌单失败(http=%1)").arg(pr.httpStatus).toStdString());

    qint64 likedId = 0;
    const QJsonArray playlists = QJsonDocument::fromJson(pr.body).object()
                                     .value(QLatin1String("playlist")).toArray();
    for (const QJsonValue& v : playlists) {
        const QJsonObject p = v.toObject();
        const qint64 id = p.value(QLatin1String("id")).toVariant().toLongLong();
        if (id <= 0)
            continue;
        if (p.value(QLatin1String("specialType")).toInt() == 5) {
            likedId = id;
            break;
        }
        if (likedId == 0 && p.value(QLatin1String("name")).toString().endsWith(QLatin1String("喜欢的音乐")))
            likedId = id;
    }
    if (likedId <= 0)
        throw std::runtime_error("未找到'我喜欢的音乐'歌单");

    // 阶段3：歌单详情拿有序 trackIds（保证列表顺序与云端一致）
    QUrl dUrl(QStringLiteral("https://music.163.com/api/v6/playlist/detail"));
    QUrlQuery dq;
    dq.addQueryItem(QLatin1String("id"), QString::number(likedId));
    dUrl.setQuery(dq);
    const Reply dr = exec(makePlainGet(dUrl.toString(), cookie));
    if (dr.httpStatus != 200 || dr.body.trimmed().isEmpty())
        throw std::runtime_error(
            QStringLiteral("获取歌单详情失败(http=%1)").arg(dr.httpStatus).toStdString());

    QList<qint64> order;
    const auto playlist = QJsonDocument::fromJson(dr.body).object().value("playlist").toObject();
    if (!playlist.value("trackIds").isArray())
        throw std::runtime_error("获取歌单详情失败：缺少曲目列表");
    const auto trackIds = playlist.value("trackIds").toArray();
    for (const QJsonValue& v : trackIds) {
        const qint64 id = v.toObject().value(QLatin1String("id")).toVariant().toLongLong();
        if (id > 0)
            order.append(id);
    }


    const auto metadata = fetchSongDetailMeta(order, cookie);
    if (!order.isEmpty() && metadata.isEmpty())
        throw std::runtime_error("歌曲详情加载失败");
    QVariantList songs;
    for (const auto id : order) {
        const auto it = metadata.constFind(id);
        if (it != metadata.cend()) songs.append(it->toVariantMap());
    }
    return QVariantMap{{"playlistId",likedId},{"songs",songs}};
}

QVariant roamSongsHandler(const QVariantMap& args)
{
    const qint64 songId = args.value(QLatin1String("songId")).toLongLong();
    const qint64 playlistId = args.value(QLatin1String("playlistId")).toLongLong();
    const QString cookie = args.value(QLatin1String("cookie")).toString();
    const QString csrf = args.value(QLatin1String("csrf")).toString();

    if (songId <= 0)
        throw std::runtime_error("当前歌曲无 songId，无法漫游");
    if (playlistId <= 0)
        throw std::runtime_error("未获取到'我喜欢的音乐'歌单ID");

    // 与 binaryify playmode_intelligence_list 一致：songId/type/playlistId/startMusicId/count
    QJsonObject payload;
    payload[QLatin1String("songId")] = songId;
    payload[QLatin1String("type")] = QStringLiteral("fromPlayOne");
    payload[QLatin1String("playlistId")] = playlistId;
    payload[QLatin1String("startMusicId")] = songId;
    payload[QLatin1String("count")] = 20;
    if (!csrf.isEmpty())
        payload[QLatin1String("csrf_token")] = csrf;

    NcmProtocol protocol;
    NcmRequest req = protocol.prepareWeapi(
        QStringLiteral("/weapi/playmode/intelligence/list?csrf_token=%1").arg(csrf), payload);
    // 注意：漫游走基础 cookie（不带 os=pc，NcmProtocol 已说明带 os=pc 会触发风控）
    if (!cookie.isEmpty())
        req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());

    const Reply r = exec(req);
    if (r.httpStatus != 200 || r.body.trimmed().isEmpty())
        throw std::runtime_error("漫游请求失败");
    const QJsonObject root = QJsonDocument::fromJson(r.body).object();
    if (root.value(QLatin1String("code")).toInt() != 200)
        throw std::runtime_error("漫游请求失败");

    QList<Oran7NcmTrackMeta> tracks;
    QList<qint64> missing;
    for (const auto& value : root.value("data").toArray()) {
        const auto track = Oran7NcmJsonMapper::track(value.toObject());
        if (!track) continue;
        tracks.append(*track);
        if (track->title.isEmpty() || track->artist.isEmpty() || track->cover.isEmpty()) missing.append(track->songId);
    }
    const auto details = fetchSongDetailMeta(missing, cookie);
    QVariantList usable;
    for (auto track : tracks) {
        const auto it = details.constFind(track.songId);
        if (it != details.cend()) track.completeMissing(*it);
        if (!track.title.isEmpty()) usable.append(track.toVariantMap());
    }
    if (usable.isEmpty()) throw std::runtime_error("无可用漫游推荐");
    return usable;
}

} // namespace ncm
