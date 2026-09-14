#include "NcmAsyncHandlers.h"

#include "BlockingCurl.h"
#include "GlobalHelper.h"
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

QString joinArtists(const QJsonValue& artistsVal)
{
    QStringList names;
    if (artistsVal.isArray()) {
        const QJsonArray arr = artistsVal.toArray();
        for (const QJsonValue& entry : arr) {
            const QString nm = entry.toObject().value(QLatin1String("name")).toString();
            if (!nm.isEmpty())
                names << nm;
        }
    }
    return names.join(QStringLiteral(" / "));
}

/*!
 * @brief 批量拉 song/detail，返回 songId -> 元数据（中性短字段名）。
 *
 * 抽出来是因为两条链都需要它：
 *  - 「我喜欢的音乐」：只拿到有序 trackIds，元数据全靠这里补；
 *  - 漫游(心动)：接口内联的 songInfo 有时缺失或结构不同，需要兜底。
 *
 * 返回的 key 是中性短名（name/artist/album/icon/timesize/isVip），
 * 由调用方映射成各自的字段约定（liked 用 music_name 等长名，roam 用短名）。
 *
 * 单批 HTTP 非 200 只跳过该批（非致命）；curl 进程级失败由 exec 抛异常。
 */
QHash<qint64, QVariantMap> fetchSongDetailMeta(const QList<qint64>& ids, const QString& cookie)
{
    QHash<qint64, QVariantMap> metaMap;
    QList<qint64> pending = ids;

    while (!pending.isEmpty()) {
        QList<qint64> batch;
        while (!pending.isEmpty() && batch.size() < 100)
            batch.append(pending.takeFirst());

        QStringList idStrs;
        idStrs.reserve(batch.size());
        for (qint64 id : batch)
            idStrs << QString::number(id);

        QUrl sUrl(QStringLiteral("https://music.163.com/api/song/detail"));
        QUrlQuery sq;
        sq.addQueryItem(QLatin1String("ids"),
                        QStringLiteral("[%1]").arg(idStrs.join(QLatin1Char(','))));
        sUrl.setQuery(sq);
        const Reply sr = exec(makePlainGet(sUrl.toString(), cookie));
        if (sr.httpStatus != 200 || sr.body.trimmed().isEmpty()) {
            NETWORK_LOG << "NcmAsync: songDetail batch failed http=" << sr.httpStatus;
            continue;
        }

        const QJsonArray songs = QJsonDocument::fromJson(sr.body).object()
                                     .value(QLatin1String("songs")).toArray();
        for (const QJsonValue& v : songs) {
            const QJsonObject song = v.toObject();
            const qint64 id = song.value(QLatin1String("id")).toVariant().toLongLong();
            if (id <= 0)
                continue;

            const QJsonArray artists = song.value(QLatin1String("artists")).toArray();
            const QJsonObject album = song.value(QLatin1String("album")).toObject();
            QString pic = album.value(QLatin1String("picUrl")).toString();
            if (pic.isEmpty() && !artists.isEmpty())
                pic = artists.first().toObject().value(QLatin1String("picUrl")).toString();

            // 注意：song/detail 用 duration（不是 dt），毫秒 → 秒
            const qint64 durationMs = song.value(QLatin1String("duration")).toVariant().toLongLong();

            metaMap.insert(id, {
                { QStringLiteral("name"),     song.value(QLatin1String("name")).toString() },
                { QStringLiteral("artist"),   joinArtists(song.value(QLatin1String("artists"))) },
                { QStringLiteral("album"),    album.value(QLatin1String("name")).toString() },
                { QStringLiteral("icon"),     pic },
                { QStringLiteral("timesize"), QString::number(static_cast<int>(durationMs / 1000)) },
                { QStringLiteral("isVip"),    song.value(QLatin1String("fee")).toInt() == 1 }
            });
        }
    }
    return metaMap;
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

    // 阶段1：搜索（镜像 Oran7MusicSearchService::search → onSearchReply；搜索阶段不带 Cookie）
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

    struct PendingSong
    {
        qint64 id = 0;
        QString name, artist, album, timesize, fallbackPic;
        bool isVip = false;
    };
    QList<PendingSong> pending;
    const QJsonArray songs = root.value(QLatin1String("result"))
                                 .toObject()
                                 .value(QLatin1String("songs"))
                                 .toArray();
    for (const QJsonValue& val : songs) {
        const QJsonObject song = val.toObject();
        PendingSong ps;
        ps.id = song.value(QLatin1String("id")).toVariant().toLongLong();
        ps.name = song.value(QLatin1String("name")).toString();
        ps.artist = joinArtists(song.value(QLatin1String("artists")));
        ps.album = song.value(QLatin1String("album")).toObject()
                       .value(QLatin1String("name")).toString();
        const qint64 durationMs = song.value(QLatin1String("duration")).toVariant().toLongLong();
        ps.timesize = QString::number(durationMs > 0 ? static_cast<int>(durationMs / 1000) : 0);
        ps.isVip = (song.value(QLatin1String("fee")).toInt() == 1);
        // 搜索阶段的兜底封面：/search/get 的 album 没有 picUrl，用歌手头像 img1v1Url 顶着
        const QJsonArray artistsArr = song.value(QLatin1String("artists")).toArray();
        if (!artistsArr.isEmpty())
            ps.fallbackPic = artistsArr.first().toObject()
                                 .value(QLatin1String("img1v1Url")).toString();
        pending.append(ps);
    }

    if (pending.isEmpty())
        return QVariantList();   // 对应 searchFinished(0)：成功但 0 条

    // 阶段2：批量专辑封面（失败非致命，退回兜底封面）
    QStringList ids;
    ids.reserve(pending.size());
    for (const PendingSong& s : pending)
        ids << QString::number(s.id);

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
            for (const QJsonValue& val : dsongs) {
                const QJsonObject song = val.toObject();
                const qint64 id = song.value(QLatin1String("id")).toVariant().toLongLong();
                const QJsonObject albumObj = song.value(QLatin1String("album")).toObject();
                QString pic = albumObj.value(QLatin1String("picUrl")).toString();
                if (pic.isEmpty()) {
                    const QJsonArray ar = song.value(QLatin1String("artists")).toArray();
                    if (!ar.isEmpty())
                        pic = ar.first().toObject().value(QLatin1String("picUrl")).toString();
                }
                if (!pic.isEmpty())
                    picMap.insert(id, pic);
            }
        } else {
            NETWORK_LOG << "NcmAsync: search song/detail failed http=" << dr.httpStatus;
        }
    }

    // 阶段3：已登录且存在 VIP 歌 → 解析真实播放地址（失败非致命，退回外链）
    QHash<qint64, QString> urlMap;
    if (isLoggedIn) {
        QList<qint64> vipIds;
        for (const PendingSong& s : pending) {
            if (s.isVip)
                vipIds << s.id;
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
                const QJsonArray data = QJsonDocument::fromJson(vr.body).object()
                                            .value(QLatin1String("data")).toArray();
                for (const QJsonValue& v : data) {
                    const QJsonObject o = v.toObject();
                    const qint64 id = o.value(QLatin1String("id")).toVariant().toLongLong();
                    const QString u = o.value(QLatin1String("url")).toString();
                    if (!u.isEmpty())
                        urlMap.insert(id, u);
                }
            } else {
                NETWORK_LOG << "NcmAsync: search enhance/player/url failed http=" << vr.httpStatus;
            }
        }
    }

    // 输出：按搜索原顺序；songId 已由 C++ 算好，QML 无需再 songIdForUrl 反查
    QVariantList out;
    for (const PendingSong& s : pending) {
        QString pic = picMap.value(s.id);
        if (pic.isEmpty())
            pic = s.fallbackPic;
        QString u = urlMap.value(s.id);
        if (u.isEmpty())
            u = QStringLiteral("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(s.id);

        QVariantMap item;
        item[QLatin1String("icon")] = pic;
        item[QLatin1String("music_name")] = s.name;
        item[QLatin1String("music_artist")] = s.artist;
        item[QLatin1String("music_album")] = s.album;
        item[QLatin1String("timesize")] = s.timesize;
        item[QLatin1String("filepath")] = u;
        item[QLatin1String("songId")] = QVariant::fromValue<qlonglong>(s.id);
        item[QLatin1String("isVip")] = s.isVip;
        out.append(item);
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
    const QJsonArray trackIds = QJsonDocument::fromJson(dr.body).object()
                                    .value(QLatin1String("playlist")).toObject()
                                    .value(QLatin1String("trackIds")).toArray();
    for (const QJsonValue& v : trackIds) {
        const qint64 id = v.toObject().value(QLatin1String("id")).toVariant().toLongLong();
        if (id > 0)
            order.append(id);
    }
    if (order.isEmpty())
        throw std::runtime_error("歌单中无歌曲");

    // 阶段4：分批 song/detail 补元数据（解析逻辑抽在 fetchSongDetailMeta，漫游那条链共用）
    const QHash<qint64, QVariantMap> metaMap = fetchSongDetailMeta(order, cookie);

    // 按歌单 trackIds 顺序输出（缺元数据的跳过），playlistId 供 roam/reorder 使用。
    // 这里把工具返回的中性短名映射成列表模型用的长名（music_name / music_artist / ...）。
    QVariantList songsOut;
    for (const qint64 id : order) {
        const QVariantMap meta = metaMap.value(id);
        if (meta.isEmpty())
            continue;
        QVariantMap item;
        item[QLatin1String("icon")]         = meta.value(QLatin1String("icon"));
        item[QLatin1String("music_name")]   = meta.value(QLatin1String("name"));
        item[QLatin1String("music_artist")] = meta.value(QLatin1String("artist"));
        item[QLatin1String("music_album")]  = meta.value(QLatin1String("album"));
        item[QLatin1String("timesize")]     = meta.value(QLatin1String("timesize"));
        item[QLatin1String("isVip")]        = meta.value(QLatin1String("isVip"));
        item[QLatin1String("songId")]       = QVariant::fromValue<qlonglong>(id);
        songsOut.append(item);
    }

    QVariantMap result;
    result[QLatin1String("playlistId")] = QVariant::fromValue<qlonglong>(likedId);
    result[QLatin1String("songs")] = songsOut;
    return result;
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

    QVariantList songs;
    const QJsonArray data = root.value(QLatin1String("data")).toArray();
    for (const QJsonValue& v : data) {
        const QJsonObject outer = v.toObject();
        // 心动模式返回结构：data[] 每项外层 {id, alg, recommended, songInfo:{...}}，兼容平铺旧结构
        const QJsonObject song = outer[QLatin1String("songInfo")].isObject()
                                     ? outer[QLatin1String("songInfo")].toObject() : outer;
        const qint64 id = song.value(QLatin1String("id")).toVariant().toLongLong();
        if (id <= 0)
            continue;
        const QString name = song.value(QLatin1String("name")).toString();

        QStringList artistNames;
        const QJsonArray artists = song.value(QLatin1String("ar")).toArray();
        for (const QJsonValue& a : artists) {
            const QString nm = a.toObject().value(QLatin1String("name")).toString();
            if (!nm.isEmpty())
                artistNames << nm;
        }
        if (artists.isEmpty()) {
            const QJsonArray arts = song.value(QLatin1String("artists")).toArray();
            for (const QJsonValue& a : arts) {
                const QString nm = a.toObject().value(QLatin1String("name")).toString();
                if (!nm.isEmpty())
                    artistNames << nm;
            }
        }

        const QJsonObject album = song.value(QLatin1String("al")).toObject();
        QString albumName = album.value(QLatin1String("name")).toString();
        QString pic = album.value(QLatin1String("picUrl")).toString();
        if (albumName.isEmpty()) {
            const QJsonObject al = song.value(QLatin1String("album")).toObject();
            albumName = al.value(QLatin1String("name")).toString();
            if (pic.isEmpty())
                pic = al.value(QLatin1String("picUrl")).toString();
        }
        if (pic.isEmpty() && !artists.isEmpty())
            pic = artists.first().toObject().value(QLatin1String("picUrl")).toString();

        const qint64 durationMs = song.value(QLatin1String("dt")).toVariant().toLongLong();
        const QString timesize = QString::number(static_cast<int>(durationMs / 1000));
        const bool isVip = song.value(QLatin1String("fee")).toInt() == 1;

        // 短字段名：与 roamSongsReady 一致，直接进 _roamQueue 供 playRoamSong 消费
        QVariantMap item;
        item[QLatin1String("songId")] = QVariant::fromValue<qlonglong>(id);
        item[QLatin1String("icon")] = pic;
        item[QLatin1String("name")] = name;
        item[QLatin1String("artist")] = artistNames.join(QStringLiteral(" / "));
        item[QLatin1String("album")] = albumName;
        item[QLatin1String("timesize")] = timesize;
        item[QLatin1String("isVip")] = isVip;
        songs.append(item);
    }

    // 阶段2：给元数据不全的条目补 song/detail
    //
    // 心动接口 data[] 里部分条目的 songInfo 缺失或结构不同时，上面的兜底会把
    // **外层对象**当成歌曲解析 —— 外层只有 {id, alg, recommended, songInfo}，
    // 于是 name / ar / al 全空，产出一条歌名、作者、封面都为空的记录。
    // 播到它时底栏就是一片空白 —— 这正是「漫游歌的封面/歌名/作者加载不出来」的来源。
    // 注意上面只挡了 id<=0，元数据缺失是挡不住的。
    QList<qint64> needDetail;
    for (const QVariant& v : songs) {
        const QVariantMap m = v.toMap();
        if (m.value(QLatin1String("name")).toString().isEmpty()
            || m.value(QLatin1String("artist")).toString().isEmpty()
            || m.value(QLatin1String("icon")).toString().isEmpty()) {
            const qint64 detailId = m.value(QLatin1String("songId")).toLongLong();
            if (detailId > 0)
                needDetail.append(detailId);
        }
    }
    if (!needDetail.isEmpty()) {
        NETWORK_LOG << "NcmAsync roam: backfill song/detail for" << needDetail.size()
                    << "of" << songs.size() << "songs";
        const QHash<qint64, QVariantMap> detailMeta = fetchSongDetailMeta(needDetail, cookie);
        for (QVariant& v : songs) {
            QVariantMap m = v.toMap();
            const QVariantMap fill = detailMeta.value(m.value(QLatin1String("songId")).toLongLong());
            if (fill.isEmpty())
                continue;
            for (auto it = fill.constBegin(); it != fill.constEnd(); ++it) {
                // isVip 是 bool，没有「空」的概念，有记录就以 song/detail 为准；
                // 其余字符串字段只补空的，不覆盖接口已经给对的值。
                if (it.key() == QLatin1String("isVip")
                    || m.value(it.key()).toString().isEmpty())
                    m.insert(it.key(), it.value());
            }
            v = m;
        }
    }

    // 补完仍然没有歌名的直接丢掉：宁可少一首推荐，也不要播出一首底栏全空白的歌。
    // （与 fetchLikedSongsHandler「缺元数据就跳过」的处理保持一致）
    QVariantList usable;
    for (const QVariant& v : songs) {
        const QVariantMap m = v.toMap();
        if (m.value(QLatin1String("name")).toString().isEmpty()) {
            NETWORK_LOG << "NcmAsync roam: drop song with no name, songId="
                        << m.value(QLatin1String("songId")).toLongLong();
            continue;
        }
        usable.append(v);
    }
    if (usable.isEmpty())
        throw std::runtime_error("无可用漫游推荐");
    return usable;
}

} // namespace ncm
