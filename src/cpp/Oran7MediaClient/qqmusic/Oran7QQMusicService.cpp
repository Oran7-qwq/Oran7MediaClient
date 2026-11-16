#include "Oran7QQMusicService.h"
#include "Oran7QQMusicSession.h"
#include "../Oran7TrackLibrary.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>

Oran7QQMusicService::Oran7QQMusicService(Oran7TrackLibrary& tracks, Oran7QQMusicSession& session)
    : Oran7QQMusicService(tracks,
                          [&session] { return session.loggedIn() ? session.credentialSnapshot() : Oran7QQMusicAuth{}; })
{
    connect(&session, &Oran7QQMusicSession::sessionChanged, this, &Oran7QQMusicService::syncSession);
    QTimer::singleShot(0, this, &Oran7QQMusicService::syncSession);
}
Oran7QQMusicService::Oran7QQMusicService(Oran7TrackLibrary& tracks, Credentials credentials,
                                         Oran7QQMusicApi::Oran7Options options)
    : m_tracks(tracks), m_credentials(std::move(credentials)), m_api(std::move(options))
{
    m_clock.start();
    connect(&m_api, &Oran7QQMusicApi::finished, this, &Oran7QQMusicService::completed);
}
Oran7QQMusicService::~Oran7QQMusicService()
{
    shutdown();
}
QVariantMap Oran7QQMusicService::mapSong(QJsonObject song)
{
    if (song.value("songInfo").isObject())
        song = song.value("songInfo").toObject();
    const auto mid = song.value("mid").toString(song.value("songmid").toString());
    const auto album = song.value("album").toObject();
    QStringList artists;
    for (const auto& singer : song.value("singer").toArray())
        artists << singer.toObject().value("name").toString();
    const auto albumMid = album.value("mid").toString(song.value("albummid").toString());
    const auto pay = song.value("pay").toObject();
    const QString path = "qqmusic://song/" + mid;
    QVariantMap fields{
        {"provider", "qqmusic"},
        {"providerId", mid},
        {"nativeId", song.value("id").toVariant().toString()},
        {"mediaMid", song.value("file").toObject().value("media_mid").toString()},
        {"path", path},
        {"url", path},
        {"source", "qqmusic"},
        {"songId", 0},
        {"title", song.value("title").toString(song.value("name").toString(song.value("songname").toString()))},
        {"artist", artists.join(" / ")},
        {"album", album.value("name").toString(song.value("albumname").toString())},
        {"cover",
         albumMid.isEmpty() ? QString() : "https://y.qq.com/music/photo_new/T002R300x300M000" + albumMid + ".jpg"},
        {"timesize", QString::number(song.value("interval").toInt())},
        {"isVip", pay.value("pay_play").toInt() > 0 || pay.value("payplay").toInt() > 0}};
    return !mid.isEmpty() && Oran7Track::fromVariantMap(fields) ? fields : QVariantMap{};
}
quint64 Oran7QQMusicService::request(Oran7Operation operation, const QString& module, const QString& method,
                                     const QJsonObject& parameters, bool write)
{
    const auto id = m_api.request(module, method, parameters, m_credentials(), write);
    if (id)
        m_operations.insert(id, std::move(operation));
    return id;
}
void Oran7QQMusicService::cancelRequest(quint64& id)
{
    m_operations.remove(id);
    m_api.cancel(id);
    id = 0;
}
void Oran7QQMusicService::syncSession()
{
    if (m_closed)
        return;
    const auto auth = m_credentials();
    const auto identity =
        auth.isValid() ? QCryptographicHash::hash(auth.cookieHeader(), QCryptographicHash::Sha256) : QByteArray{};
    if (identity == m_account)
        return;
    m_account = identity;
    m_operations.clear();
    m_api.cancelAll();
    ++m_resolveGeneration;
    m_searchRequest = m_likedRequest = m_resolveRequest = m_likeRequest = m_lyricRequest = 0;
    m_likedPages.clear();
    m_likedLoaded = false;
    m_likedAt = -30000;
    m_lastLike = -2000;
    m_urls.clear();
    m_resolvedPath.clear();
    m_resolvedUrl.clear();
    m_error.clear();
    cancelLyrics();
    m_tracks.qqLiked()->replaceTracks({});
    m_tracks.qqSearch()->replaceTracks({});
    emit statusChanged();
    if (auth.isValid())
        fetchLiked();
}
void Oran7QQMusicService::search(const QString& keyword)
{
    if (m_closed)
        return;
    cancelRequest(m_searchRequest);
    m_error.clear();
    m_tracks.qqSearch()->replaceTracks({});
    const auto query = keyword.trimmed().left(100);
    if (!query.isEmpty())
        m_searchRequest = request({Kind::Search, {}}, "music.search.SearchCgiService", "DoSearchForQQMusicDesktop",
                                  {{"query", query}, {"search_type", 0}, {"num_per_page", 30}, {"page_num", 1}});
    emit statusChanged();
}
bool Oran7QQMusicService::isLiked(const QString& path) const
{
    return m_tracks.qqLiked()->contains(0, {}, path);
}
void Oran7QQMusicService::fetchLiked(bool force)
{
    if (m_closed || !m_credentials().isValid() || m_likedRequest)
        return;
    if (!force && m_likedLoaded && m_clock.elapsed() - m_likedAt < 30000)
        return;
    m_error.clear();
    m_likedPages.clear();
    likedPage(0);
    emit statusChanged();
}
void Oran7QQMusicService::likedPage(int offset)
{
    m_likedRequest = request({Kind::Liked, {}, offset}, "music.srfDissInfo.DissInfo", "CgiGetDiss",
                             {{"disstid", 0},
                              {"dirid", 201},
                              {"tag", true},
                              {"song_begin", offset},
                              {"song_num", 200},
                              {"userinfo", true},
                              {"orderlist", true},
                              {"onlysonglist", false}});
}
bool Oran7QQMusicService::toggleLike(const QVariantMap& fields)
{
    if (m_closed)
        return false;
    const auto track = Oran7Track::fromVariantMap(fields);
    if (!track || track->provider != "qqmusic" || track->nativeId <= 0)
        return false;
    if (!m_credentials().isValid())
    {
        m_error = QStringLiteral("请先登录 QQ 音乐");
        emit statusChanged();
        return false;
    }
    if (!m_likedLoaded)
    {
        fetchLiked();
        m_error = QStringLiteral("请等待喜欢列表加载完成后重试");
        emit statusChanged();
        return false;
    }
    if (m_likeRequest || m_clock.elapsed() - m_lastLike < 2000)
        return false;
    const bool liked = !isLiked(track->path);
    m_lastLike = m_clock.elapsed();
    m_error.clear();
    // 写前核实 MID、数字 ID 和真实类型，不能把某一种 songType 套用到所有曲目。
    m_likeRequest = request({Kind::LikeDetail, *track, 0, liked}, "music.pf_song_detail_svr", "get_song_detail_yqq",
                            {{"song_mid", track->providerId}});
    if (!m_likeRequest)
        m_error = QStringLiteral("QQ 音乐收藏请求创建失败，请稍后重试");
    emit statusChanged();
    return m_likeRequest != 0;
}
bool Oran7QQMusicService::matchesSource(const QString& path, const QString& url) const
{
    return path == m_resolvedPath && url == m_resolvedUrl && !url.isEmpty();
}
void Oran7QQMusicService::cancelResolve()
{
    ++m_resolveGeneration;
    cancelRequest(m_resolveRequest);
}
void Oran7QQMusicService::resolve(const Oran7Track& track)
{
    m_openRetry = true;
    resolveUrl(track, true);
}
void Oran7QQMusicService::resolveUrl(const Oran7Track& track, bool cache)
{
    cancelResolve();
    if (m_closed || track.provider != "qqmusic")
        return;
    if (cache)
    {
        if (const auto* saved = m_urls.object(track.path); saved && saved->expiry > m_clock.elapsed())
        {
            const auto generation = m_resolveGeneration;
            const auto url = saved->url;
            QTimer::singleShot(0, this,
                               [this, generation, path = track.path, url]
                               {
                                   if (generation == m_resolveGeneration && !m_closed)
                                   {
                                       m_resolvedPath = path;
                                       m_resolvedUrl = url;
                                       emit urlReady(path, url);
                                   }
                               });
            return;
        }
    }
    if (track.mediaMid.isEmpty())
    {
        m_resolveRequest = request({Kind::Detail, track}, "music.pf_song_detail_svr", "get_song_detail_yqq",
                                   {{"song_mid", track.providerId}});
        return;
    }
    QJsonArray filenames, mids, types;
    QStringList media{track.mediaMid};
    if (track.providerId != track.mediaMid)
        media << track.providerId;
    for (const auto& id : media)
        for (const auto& pair :
             {qMakePair(QString("F000"), QString(".flac")), qMakePair(QString("M800"), QString(".mp3")),
              qMakePair(QString("M500"), QString(".mp3")), qMakePair(QString("C400"), QString(".m4a"))})
        {
            filenames.append(pair.first + id + pair.second);
            mids.append(track.providerId);
            types.append(0);
        }
    m_resolveRequest = request({Kind::Resolve, track}, "vkey.GetVkeyServer", "CgiGetVkey",
                               {{"guid", QString::number(QDateTime::currentMSecsSinceEpoch() % 10000000000LL)},
                                {"uin", m_credentials().userId.isEmpty() ? "0" : m_credentials().userId},
                                {"songmid", mids},
                                {"songtype", types},
                                {"filename", filenames},
                                {"loginflag", 1},
                                {"platform", "20"}});
}
void Oran7QQMusicService::playbackOpenFailed(const Oran7Track& track, const QString& source)
{
    if (!matchesSource(track.path, source))
        return;
    if (m_openRetry)
    {
        m_openRetry = false;
        m_urls.remove(track.path);
        resolveUrl(track, false);
    }
    else
        failResolve(track.path, QStringLiteral("QQ 音乐播放地址打开失败，请重试"));
}
void Oran7QQMusicService::failResolve(const QString& path, const QString& error)
{
    m_error = error;
    emit statusChanged();
    emit resolveFailed(path, error);
}
void Oran7QQMusicService::loadLyrics(const Oran7Track& track)
{
    cancelLyrics();
    if (m_closed)
        return;
    m_lyricRequest = m_api.lyrics(track.providerId, m_credentials());
    if (m_lyricRequest)
        m_operations.insert(m_lyricRequest, {Kind::Lyrics, track});
}
void Oran7QQMusicService::cancelLyrics()
{
    cancelRequest(m_lyricRequest);
    m_lyricsDirectory.reset();
    emit lyricsUnavailable();
}
void Oran7QQMusicService::completed(quint64 id, const QJsonObject& data, const QString& error)
{
    if (m_closed || !m_operations.contains(id))
        return;
    const auto operation = m_operations.take(id);
    switch (operation.kind)
    {
    case Kind::Search:
    {
        if (id != m_searchRequest)
            return;
        m_searchRequest = 0;
        if (error.isEmpty())
        {
            const auto songs = data.value("body").toObject().value("song").toObject().value("list");
            if (!songs.isArray())
                m_error = QStringLiteral("QQ 音乐搜索结果格式异常");
            else
            {
                QVariantList tracks;
                for (const auto& item : songs.toArray())
                {
                    auto track = mapSong(item.toObject());
                    if (!track.isEmpty())
                        tracks.append(track);
                }
                m_tracks.qqSearch()->replaceTracks(tracks);
            }
        }
        else
            m_error = error;
        emit statusChanged();
        break;
    }
    case Kind::Liked:
    {
        if (id != m_likedRequest)
            return;
        m_likedRequest = 0;
        if (!error.isEmpty() || !data.value("songlist").isArray())
        {
            m_error = error.isEmpty() ? QStringLiteral("QQ 音乐喜欢列表格式异常") : error;
            m_likedPages.clear();
            emit statusChanged();
            break;
        }
        const auto raw = data.value("songlist").toArray();
        for (const auto& song : raw)
        {
            auto track = mapSong(song.toObject());
            if (!track.isEmpty())
                m_likedPages.append(track);
        }
        const int offset = operation.offset + raw.size();
        const int total = data.value("total").toInt(data.value("dirinfo").toObject().value("songnum").toInt(offset));
        const bool more = data.contains("hasmore") ? data.value("hasmore").toVariant().toBool() : offset < total;
        if (more)
        {
            if (raw.isEmpty() || offset >= 100000)
            {
                m_error = QStringLiteral("QQ 音乐喜欢列表加载不完整，请刷新重试");
                m_likedPages.clear();
            }
            else
            {
                likedPage(offset);
                return;
            }
        }
        else
        {
            m_likedLoaded = true;
            m_likedAt = m_clock.elapsed();
            m_tracks.qqLiked()->replaceTracks(m_likedPages);
            m_likedPages.clear();
        }
        emit statusChanged();
        break;
    }
    case Kind::Detail:
    {
        if (id != m_resolveRequest)
            return;
        m_resolveRequest = 0;
        auto track = mapSong(data.value("track_info").toObject());
        if (!error.isEmpty() || track.isEmpty())
        {
            failResolve(operation.track.path, error.isEmpty() ? QStringLiteral("QQ 音乐歌曲详情加载失败") : error);
            break;
        }
        auto detailed = operation.track;
        detailed.mediaMid = track.value("mediaMid").toString();
        if (detailed.mediaMid.isEmpty())
            detailed.mediaMid = detailed.providerId;
        resolveUrl(detailed, false);
        break;
    }
    case Kind::Resolve:
    {
        if (id != m_resolveRequest)
            return;
        m_resolveRequest = 0;
        QString purl;
        for (const auto& item : data.value("midurlinfo").toArray())
        {
            const auto value = item.toObject().value("purl").toString();
            if (!value.isEmpty())
            {
                purl = value;
                break;
            }
        }
        if (!error.isEmpty() || purl.isEmpty())
        {
            failResolve(operation.track.path,
                        error.isEmpty() ? QStringLiteral("该 QQ 音乐歌曲暂无可播放音源，请检查账号权限") : error);
            break;
        }
        const auto bases = data.value("sip").toArray();
        QUrl base(bases.isEmpty() ? "https://ws.stream.qqmusic.qq.com/" : bases.first().toString());
        if (base.scheme() == "http")
            base.setScheme("https");
        const auto url = base.resolved(QUrl(purl));
        if ((url.scheme() != "https" && url.scheme() != "http") || url.host().isEmpty() || !url.userInfo().isEmpty())
        {
            failResolve(operation.track.path, QStringLiteral("QQ 音乐返回了无效播放地址"));
            break;
        }
        m_resolvedPath = operation.track.path;
        m_resolvedUrl = url.toString();
        m_urls.insert(m_resolvedPath, new Oran7CachedUrl{m_resolvedUrl, m_clock.elapsed() + 20 * 60 * 1000});
        emit urlReady(m_resolvedPath, m_resolvedUrl);
        break;
    }
    case Kind::LikeDetail:
    {
        if (id != m_likeRequest)
            return;
        m_likeRequest = 0;
        const auto detail = data.value("track_info").toObject();
        const auto type = detail.value("type");
        if (!error.isEmpty() || detail.value("mid").toString() != operation.track.providerId ||
            detail.value("id").toVariant().toLongLong() != operation.track.nativeId || !type.isDouble() ||
            type.toInt(-1) < 0 || type.toDouble() != type.toInt())
        {
            m_error = error.isEmpty() ? QStringLiteral("QQ 音乐歌曲详情加载失败") : error;
            emit statusChanged();
            break;
        }
        // 收藏采用官网的最小参数；不自动重试写入，等待 retCode 后再读回完整列表。
        m_likeRequest =
            request({Kind::Like, operation.track, 0, operation.like}, "music.musicasset.PlaylistDetailWrite",
                    operation.like ? "AddSonglist" : "DelSonglist",
                    {{"dirId", 201},
                     {"v_songInfo", QJsonArray{QJsonObject{{"songId", operation.track.nativeId}, {"songType", type}}}}},
                    true);
        if (!m_likeRequest)
            m_error = QStringLiteral("QQ 音乐收藏请求创建失败，请稍后重试");
        emit statusChanged();
        break;
    }
    case Kind::Like:
    {
        if (id != m_likeRequest)
            return;
        m_likeRequest = 0;
        if (error.isEmpty())
        {
            if (operation.like)
                m_tracks.qqLiked()->prependTracks({operation.track.toVariantMap()});
            else
                m_tracks.qqLiked()->removeMatching(0, {}, operation.track.path);
            m_likedLoaded = false;
            cancelRequest(m_likedRequest);
            fetchLiked(true);
        }
        else
        {
            m_error = error;
            m_likedLoaded = false;
            cancelRequest(m_likedRequest);
            fetchLiked(true);
            m_error = error;
        }
        emit statusChanged();
        break;
    }
    case Kind::Lyrics:
    {
        if (id != m_lyricRequest)
            return;
        m_lyricRequest = 0;
        auto text = data.value("lyric").toString();
        if (!text.contains('['))
            text = QString::fromUtf8(QByteArray::fromBase64(text.toUtf8()));
        text.replace("&#58;", ":");
        text.replace("&#46;", ".");
        text.replace("&#39;", "'");
        text.replace("&amp;", "&");
        if (!error.isEmpty() || !text.contains(QRegularExpression("\\[\\d+:\\d+")))
        {
            emit lyricsUnavailable();
            break;
        }
        m_lyricsDirectory = std::make_unique<QTemporaryDir>();
        const auto path = m_lyricsDirectory->filePath("qqmusic.lrc");
        QFile file(path);
        const auto bytes = text.toUtf8();
        if (!m_lyricsDirectory->isValid() || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
        {
            emit lyricsUnavailable();
            break;
        }
        file.close();
        emit lyricsReady(path);
        break;
    }
    }
}
void Oran7QQMusicService::shutdown()
{
    if (m_closed)
        return;
    m_closed = true;
    m_operations.clear();
    m_api.cancelAll();
    ++m_resolveGeneration;
    m_searchRequest = m_likedRequest = m_resolveRequest = m_likeRequest = 0;
    cancelLyrics();
    m_urls.clear();
}
