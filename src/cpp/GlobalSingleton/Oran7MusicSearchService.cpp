#include "Oran7MusicSearchService.h"

#include "GlobalHelper.h"
#include "NcmSession.h"
#include "NcmSongApi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

static Oran7MusicSearchService *s_instance = nullptr;

const QString Oran7MusicSearchService::kSearchUrl =
    QStringLiteral("https://music.163.com/api/search/get");

Oran7MusicSearchService& Oran7MusicSearchService::instance()
{
    if (!s_instance)
        s_instance = new Oran7MusicSearchService();
    return *s_instance;
}

void Oran7MusicSearchService::cleanup()
{
    if (s_instance) {
        // 事件循环仍存活（aboutToQuit 中调用），QNetworkAccessManager 作为子对象随析构销毁
        delete s_instance;
        s_instance = nullptr;
    }
}

Oran7MusicSearchService::Oran7MusicSearchService(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    // 播放地址解析结果（只连一次，内部用 m_resolveGen 丢弃过期结果）
    connect(NcmSession::instance().songApi(), &NcmSongApi::urlsResolved,
            this, &Oran7MusicSearchService::onUrlsResolved, Qt::UniqueConnection);
}

Oran7MusicSearchService::~Oran7MusicSearchService() = default;

qint64 Oran7MusicSearchService::songIdForUrl(const QString& url) const
{
    return m_songIdByUrl.value(url, -1);
}

void Oran7MusicSearchService::search(const QString &keywords)
{
    // 取消上一个未完成的请求（搜索 或 详情 任一阶段）
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
    m_pendingSongs.clear();
    m_picMap.clear();
    m_songIdByUrl.clear();
    ++m_generation;   // 新一次搜索，让旧的 URL 解析结果失效

    const QString kw = keywords.trimmed();
    if (kw.isEmpty()) {
        emit searchFailed(QStringLiteral("搜索关键词为空"));
        return;
    }

    QUrl url(kSearchUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("s"), kw);
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("1"));   // 1=单曲
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("30"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Oran7MediaClient/1.0"));
    request.setRawHeader("Referer", "https://music.163.com");
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_networkManager->get(request);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onSearchReply(reply);
    });

    NETWORK_LOG << "MusicSearch: searching" << kw;
    emit searchStarted();
}

void Oran7MusicSearchService::onSearchReply(QNetworkReply *reply)
{
    if (reply != m_activeReply) {
        // 过期回复（被新搜索取消后仍回调），直接丢弃
        reply->deleteLater();
        return;
    }
    m_activeReply = nullptr;

    const QByteArray raw = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        ERROR_LOG << "MusicSearch: network error" << reply->errorString();
        emit searchFailed(QStringLiteral("网络错误: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isNull() || !doc.isObject()) {
        emit searchFailed(QStringLiteral("响应解析失败"));
        return;
    }

    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt();
    if (code != 200) {
        emit searchFailed(QStringLiteral("API错误 code=%1").arg(code));
        return;
    }

    const QJsonArray songs = root.value(QStringLiteral("result"))
                                 .toObject()
                                 .value(QStringLiteral("songs"))
                                 .toArray();

    for (const QJsonValue &val : songs) {
        const QJsonObject song = val.toObject();
        PendingSong ps;
        ps.id    = song.value(QStringLiteral("id")).toVariant().toLongLong();
        ps.name  = song.value(QStringLiteral("name")).toString();
        ps.artist = joinArtists(song.value(QStringLiteral("artists")));
        ps.album = song.value(QStringLiteral("album")).toObject()
                       .value(QStringLiteral("name")).toString();
        ps.timesize = QString::number(durationSeconds(song));
        // fee==1 为 VIP 歌曲（免费外链被网易云拦截成 HTML，无法播放），供 UI 打 VIP 标记
        ps.isVip = (song.value(QStringLiteral("fee")).toInt() == 1);

        // 搜索阶段的兜底封面：/search/get 的 album 没有 picUrl，
        // 用歌手头像 img1v1Url 顶着，详情接口失败时不至于空图
        const QJsonArray artistsArr = song.value(QStringLiteral("artists")).toArray();
        if (!artistsArr.isEmpty())
            ps.fallbackPic = artistsArr.first().toObject()
                                 .value(QStringLiteral("img1v1Url")).toString();

        m_pendingSongs.append(ps);
    }

    if (m_pendingSongs.isEmpty()) {
        NETWORK_LOG << "MusicSearch: got 0 results";
        emit searchFinished(0);
        return;
    }

    // 第二阶段：批量获取专辑封面
    fetchDetails();
}

void Oran7MusicSearchService::fetchDetails()
{
    QStringList ids;
    ids.reserve(m_pendingSongs.size());
    for (const PendingSong &s : m_pendingSongs)
        ids << QString::number(s.id);

    QUrl url(QStringLiteral("https://music.163.com/api/song/detail"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"),
                       QStringLiteral("[%1]").arg(ids.join(QLatin1Char(','))));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Oran7MediaClient/1.0"));
    request.setRawHeader("Referer", "https://music.163.com");
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_networkManager->get(request);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDetailReply(reply);
    });
}

void Oran7MusicSearchService::onDetailReply(QNetworkReply *reply)
{
    if (reply != m_activeReply) {
        reply->deleteLater();
        return;
    }
    m_activeReply = nullptr;

    const QByteArray raw = reply->readAll();
    reply->deleteLater();

    QHash<qint64, QString> picMap;
    if (reply->error() != QNetworkReply::NoError) {
        // 详情失败不阻塞整个搜索，退回兜底封面
        WARNING_LOG << "MusicSearch: song/detail failed" << reply->errorString();
        m_picMap = picMap;
        emitAllResults(m_picMap, QHash<qint64, QString>());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isObject()) {
        const QJsonArray songs = doc.object().value(QStringLiteral("songs")).toArray();
        for (const QJsonValue &val : songs) {
            const QJsonObject song = val.toObject();
            const qint64 id = song.value(QStringLiteral("id")).toVariant().toLongLong();
            const QJsonObject albumObj = song.value(QStringLiteral("album")).toObject();
            QString pic = albumObj.value(QStringLiteral("picUrl")).toString();
            if (pic.isEmpty()) {
                const QJsonArray ar = song.value(QStringLiteral("artists")).toArray();
                if (!ar.isEmpty())
                    pic = ar.first().toObject().value(QStringLiteral("picUrl")).toString();
            }
            if (!pic.isEmpty())
                picMap.insert(id, pic);
        }
    }
    m_picMap = picMap;

    // 已登录：仅对 VIP 歌曲解析真实播放地址（enhance/player/url 才能拿到 music.126.net 地址）。
    // 免费歌曲保持 outer/url 外链（这条路径久经验证稳定，避免引入新的播放问题）。
    if (NcmSession::instance().isLoggedIn()) {
        QList<qint64> ids;
        for (const PendingSong &s : std::as_const(m_pendingSongs)) {
            if (s.isVip)
                ids << s.id;
        }

        if (!ids.isEmpty()) {
            m_resolveGen = m_generation;
            NcmSession::instance().songApi()->resolveUrls(ids);
            return;
        }
    }

    // 未登录 或 没有 VIP 歌曲：用外链（免费歌可播，VIP 无登录仍无法播放）
    emitAllResults(m_picMap, QHash<qint64, QString>());
}

void Oran7MusicSearchService::onUrlsResolved(const QHash<qint64, QString> &urlMap)
{
    if (m_resolveGen != m_generation)
        return;   // 过期结果（用户又发起了新搜索），丢弃
    emitAllResults(m_picMap, urlMap);
}

void Oran7MusicSearchService::emitAllResults(const QHash<qint64, QString> &picMap,
                                             const QHash<qint64, QString> &urlMap)
{
    const int count = m_pendingSongs.size();
    for (const PendingSong &s : std::as_const(m_pendingSongs)) {
        QString pic = picMap.value(s.id);
        if (pic.isEmpty())
            pic = s.fallbackPic;

        // 优先用解析出的真实地址（VIP 可播）；没有则退回外链
        QString url = urlMap.value(s.id);
        if (url.isEmpty())
            url = QStringLiteral("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(s.id);

        // 记录 播放地址 → songId，供 Client 歌词搜索反查（真实 music.126.net 地址不含 id）
        m_songIdByUrl.insert(url, s.id);

        emit searchResultReady(pic, s.name, s.artist, s.album, s.timesize, url, s.isVip);
    }
    m_pendingSongs.clear();

    NETWORK_LOG << "MusicSearch: got" << count << "results";
    emit searchFinished(count);
}

QString Oran7MusicSearchService::joinArtists(const QJsonValue &artistsVal)
{
    QStringList names;
    if (artistsVal.isArray()) {
        const QJsonArray arr = artistsVal.toArray();
        for (const QJsonValue &entry : arr) {
            const QString nm = entry.toObject().value(QStringLiteral("name")).toString();
            if (!nm.isEmpty())
                names << nm;
        }
    }
    return names.join(QStringLiteral(" / "));
}

int Oran7MusicSearchService::durationSeconds(const QJsonObject &song)
{
    const qint64 ms = song.value(QStringLiteral("duration")).toVariant().toLongLong();
    if (ms <= 0)
        return 0;
    return static_cast<int>(ms / 1000);   // 毫秒 → 秒
}
