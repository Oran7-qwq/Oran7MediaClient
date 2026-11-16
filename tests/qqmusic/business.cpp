#include "Oran7QQMusicService.h"
#include "Oran7QQMusicWebProtocol.h"
#include "Oran7TrackLibrary.h"
#include "Oran7PlaybackState.h"
#include "Oran7PlaybackViewModel.h"
#include "Oran7CloudMusicService.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QFile>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstdlib>
static int checks = 0;
static void check(bool ok, const char* label)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
    ++checks;
}
template <class F> static void until(F condition)
{
    QElapsedTimer t;
    t.start();
    while (!condition() && t.elapsed() < 6000)
        QTest::qWait(5);
    check(condition(), "asynchronous completion");
}
static QJsonObject song(int id, const QString& mid)
{
    return {{"id", id},
            {"type", 0},
            {"mid", mid},
            {"title", "fixture " + mid},
            {"interval", 120},
            {"singer", QJsonArray{QJsonObject{{"name", "artist"}}}},
            {"album", QJsonObject{{"mid", "albumMid"}, {"name", "album"}}},
            {"file", QJsonObject{{"media_mid", "media" + mid}}},
            {"pay", QJsonObject{{"pay_play", 0}}}};
}
static QByteArray decrypt(const QByteArray& encoded)
{
    auto raw = QByteArray::fromBase64(encoded);
    check(raw.size() >= 28, "ag-1 envelope has nonce and authentication tag");
    auto iv = raw.left(12), tag = raw.right(16), encrypted = raw.mid(12, raw.size() - 28);
    QByteArray plain(encrypted.size(), '\0');
    unsigned char secret[]{189, 48, 95, 16, 208, 255, 116, 182, 239, 84, 218, 184, 53, 181, 225, 207};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) >= 0, "fixture AES provider");
    check(BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
                            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                            sizeof(BCRYPT_CHAIN_MODE_GCM), 0) >= 0,
          "fixture GCM mode");
    check(BCryptGenerateSymmetricKey(algorithm, &key, nullptr, 0, secret, sizeof(secret), 0) >= 0, "fixture GCM key");
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = reinterpret_cast<PUCHAR>(iv.data());
    info.cbNonce = iv.size();
    info.pbTag = reinterpret_cast<PUCHAR>(tag.data());
    info.cbTag = tag.size();
    ULONG n = 0;
    const auto result = BCryptDecrypt(key, reinterpret_cast<PUCHAR>(encrypted.data()), encrypted.size(), &info, nullptr,
                                      0, reinterpret_cast<PUCHAR>(plain.data()), plain.size(), &n, 0);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    check(result >= 0, "ag-1 authentication and decryption");
    plain.resize(n);
    return plain;
}
class Oran7MusicServer : public QTcpServer
{
  public:
    int searches = 0, pages = 0, resolves = 0, writes = 0, details = 0;
    bool liked = false, failPage = false, failWrite = false, noUrl = false;
    int delay = 0;
    Oran7MusicServer()
    {
        check(listen(QHostAddress::LocalHost), "local QQ music fixture");
        connect(
            this, &QTcpServer::newConnection, this,
            [this]
            {
                auto* socket = nextPendingConnection();
                socket->setParent(this);
                auto bytes = std::make_shared<QByteArray>();
                auto done = std::make_shared<bool>(false);
                connect(
                    socket, &QTcpSocket::readyRead, socket,
                    [this, socket, bytes, done]
                    {
                        bytes->append(socket->readAll());
                        if (*done)
                            return;
                        const int end = bytes->indexOf("\r\n\r\n");
                        if (end < 0)
                            return;
                        int length = 0;
                        for (auto line : bytes->left(end).split('\n'))
                            if (line.toLower().startsWith("content-length:"))
                                length = line.mid(15).trimmed().toInt();
                        if (bytes->size() < end + 4 + length)
                            return;
                        *done = true;
                        const auto first = bytes->left(bytes->indexOf('\r')).split(' ');
                        const QUrl url("http://127.0.0.1" + QString::fromUtf8(first.value(1)));
                        const bool encrypted = url.path() == "/write";
                        const auto body =
                            encrypted ? decrypt(bytes->mid(end + 4, length)) : bytes->mid(end + 4, length);
                        if (encrypted)
                            check(QUrlQuery(url).queryItemValue("sign") == Oran7QQMusicWebProtocol::signature(body),
                                  "wire signature matches the actual serialized JSON");
                        const auto envelope = QJsonDocument::fromJson(body).object();
                        const bool search = envelope.contains("music.search.SearchCgiService");
                        if (search)
                            check(!envelope.contains("comm"), "search uses its required module envelope without comm");
                        const auto responseKey =
                            search ? QStringLiteral("music.search.SearchCgiService") : QStringLiteral("req");
                        auto request = envelope.value(responseKey).toObject();
                        auto param = request.value("param").toObject();
                        const auto method = request.value("method").toString();
                        QJsonObject data;
                        int code = 0;
                        if (method == "DoSearchForQQMusicDesktop")
                        {
                            ++searches;
                            data = {
                                {"body",
                                 QJsonObject{
                                     {"song",
                                      QJsonObject{{"list", QJsonArray{song(11, param.value("query").toString()),
                                                                      song(11, param.value("query").toString())}}}}}}};
                        }
                        else if (method == "CgiGetDiss")
                        {
                            ++pages;
                            const int offset = param.value("song_begin").toInt();
                            if (failPage && offset)
                                code = 1000;
                            data = {{"songlist", offset == 0 ? QJsonArray{song(11, "first")}
                                                 : liked     ? QJsonArray{song(12, "second"), song(13, "third")}
                                                             : QJsonArray{song(12, "second")}},
                                    {"hasmore", offset == 0},
                                    {"total", liked ? 3 : 2}};
                        }
                        else if (method == "get_song_detail_yqq")
                        {
                            ++details;
                            const auto mid = param.value("song_mid").toString();
                            data = {{"track_info", song(mid == "third" ? 13 : mid == "detail" ? 14 : 11, mid)}};
                        }
                        else if (method == "CgiGetVkey")
                        {
                            ++resolves;
                            const auto filename = param.value("filename").toArray().first().toString();
                            data = {{"sip", QJsonArray{"https://ws.stream.qqmusic.qq.com/"}},
                                    {"midurlinfo",
                                     QJsonArray{QJsonObject{{"filename", filename},
                                                            {"purl", noUrl ? "" : "fixture.flac?vkey=FICTIONAL"}}}}};
                        }
                        else if (method == "AddSonglist" || method == "DelSonglist")
                        {
                            ++writes;
                            if (!failWrite)
                                liked = method == "AddSonglist";
                            data = {{"retCode", failWrite ? 1 : 0}};
                            check(encrypted && param.value("dirId") == QJsonValue(201),
                                  "favorite command uses signed QQ playlist protocol");
                            const auto info = param.value("v_songInfo").toArray().first().toObject();
                            check(info.value("songId") == QJsonValue(13) && info.value("songType") == QJsonValue(0) &&
                                      !param.contains("tid") && !param.contains("bFmtUtf8"),
                                  "favorite uses verified identity and actual song type with minimal web parameters");
                        }
                        else if (url.path() == "/lyrics")
                            data = {{"code", 0}, {"lyric", "[00:00.00]QQ lyric\n[00:03.00]second line"}};
                        else
                            code = -1;
                        QByteArray response =
                            url.path() == "/lyrics"
                                ? QJsonDocument(data).toJson(QJsonDocument::Compact)
                                : QJsonDocument(QJsonObject{{"code", 0},
                                                            {responseKey, QJsonObject{{"code", code}, {"data", data}}}})
                                      .toJson(QJsonDocument::Compact);
                        if (encrypted)
                            response = Oran7QQMusicWebProtocol::decodeResponse(response);
                        QTimer::singleShot(delay, socket,
                                           [socket, response]
                                           {
                                               if (socket->state() != QAbstractSocket::ConnectedState)
                                                   return;
                                               socket->write("HTTP/1.1 200 OK\r\nContent-Length: " +
                                                             QByteArray::number(response.size()) +
                                                             "\r\nConnection: close\r\n\r\n" + response);
                                               socket->disconnectFromHost();
                                           });
                    });
            });
    }
    Oran7QQMusicApi::Oran7Options options() const
    {
        Oran7QQMusicApi::Oran7Options o;
        const auto base = QString("http://127.0.0.1:%1").arg(serverPort());
        o.endpoint = QUrl(base + "/read");
        o.writeEndpoint = QUrl(base + "/write");
        o.lyricsEndpoint = QUrl(base + "/lyrics");
        return o;
    }
};
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    check(Oran7QQMusicWebProtocol::signature("{}") == "zzcf8e26805gyafigxmxjehoe02mvsjjgtwzw6f1a05f9",
          "Web signature matches independently generated Node.js SHA-1 fixture");
    Oran7MusicServer server;
    Oran7TrackLibrary tracks;
    tracks.replaceLikedTracks({QVariantMap{{"songId", 11}, {"title", "NCM 11"}}});
    Oran7QQMusicAuth auth = Oran7QQMusicAuth::fromBrowserCookies(
        QJsonArray{QJsonObject{{"name", "uin"}, {"value", "123456"}, {"domain", "qq.com"}},
                   QJsonObject{{"name", "qm_keyst"}, {"value", "FICTIONAL_QQ_KEY"}, {"domain", "y.qq.com"}}});
    Oran7QQMusicService service(tracks, [&] { return auth; }, server.options());
    service.syncSession();
    until([&] { return service.likedLoaded(); });
    check(tracks.qqLiked()->count() == 2 && server.pages == 2 && tracks.liked()->count() == 1,
          "QQ liked pages publish atomically without changing NetEase");
    service.fetchLiked();
    check(server.pages == 2, "liked preload and page entry share TTL");
    service.search("first");
    until([&] { return !service.searchBusy(); });
    check(tracks.qqSearch()->count() == 1 && tracks.qqSearch()->get(0).value("songId").toLongLong() == 0 &&
              tracks.qqSearch()->get(0).value("isLiked").toBool(),
          "QQ mapping deduplicates and uses separate hashed likes");
    const auto track = *Oran7Track::fromVariantMap(tracks.qqSearch()->get(0));
    check(track.providerId == "first" && track.nativeId == 11 && track.provider == "qqmusic" &&
              track.path == "qqmusic://song/first",
          "stable MID and numeric ID are distinct from NetEase ID");
    tracks.addIntegratedTrack(tracks.qqSearch()->get(0));
    check(tracks.integrated()->count() == 1 && tracks.qqSearch()->get(0).value("isIntegrated").toBool(),
          "integration membership preserves provider identity");
    Oran7PlaybackState playback;
    Oran7CloudMusicService cloud(tracks, {[] { return QVariantMap{{"isLoggedIn", false}}; }, {}, {}, {}});
    Oran7PlaybackViewModel queue(tracks, playback, cloud);
    QSignalSpy selections(&queue, &Oran7PlaybackViewModel::trackRequested);
    QObject::connect(&queue, &Oran7PlaybackViewModel::trackRequested,
                     [&](const QVariantMap& f, bool) { playback.selectTrack(*Oran7Track::fromVariantMap(f)); });
    check(queue.activate(tracks.qqLiked(), 0) && queue.owner() == "qqmusic",
          "QQ source joins existing queue with its own owner");
    queue.advance(1);
    check(playback.currentTrack().providerId == "second", "QQ next does not require NetEase login");
    queue.advance(-1);
    check(playback.currentTrack().providerId == "first", "QQ previous follows stable source order");
    QSignalSpy urls(&service, &Oran7QQMusicService::urlReady), failures(&service, &Oran7QQMusicService::resolveFailed);
    service.resolve(track);
    until([&] { return urls.count() == 1; });
    const auto url = urls.last()[1].toString();
    check(url.startsWith("https://") && server.resolves == 1, "vkey resolution produces only runtime CDN URL");
    service.resolve(track);
    until([&] { return urls.count() == 2; });
    check(server.resolves == 1, "bounded URL cache reuses current account result");
    service.playbackOpenFailed(track, url);
    until([&] { return urls.count() == 3; });
    service.playbackOpenFailed(track, url);
    check(failures.count() == 1 && server.resolves == 2, "expired URL has exactly one backend-open retry");
    auto detailTrack = *Oran7Track::fromVariantMap(Oran7QQMusicService::mapSong(song(14, "detail")));
    detailTrack.mediaMid.clear();
    service.resolve(detailTrack);
    until([&] { return urls.count() == 4; });
    check(server.details == 1, "missing media MID is filled by the QQ detail endpoint before vkey resolution");
    server.noUrl = true;
    service.resolve(*Oran7Track::fromVariantMap(Oran7QQMusicService::mapSong(song(15, "restricted"))));
    until([&] { return failures.count() == 2; });
    check(urls.count() == 4, "missing playback permission never fabricates a CDN address");
    server.noUrl = false;
    QSignalSpy lyrics(&service, &Oran7QQMusicService::lyricsReady);
    service.loadLyrics(track);
    until([&] { return lyrics.count() == 1; });
    const auto lyricPath = lyrics.last()[0].toString();
    QFile lrc(lyricPath);
    check(lrc.open(QIODevice::ReadOnly) && lrc.readAll().contains("QQ lyric"), "QQ lyrics have their own LRC route");
    lrc.close();
    service.cancelLyrics();
    check(!QFile::exists(lyricPath), "QQ lyric cache is released on song cancellation");
    const auto third = Oran7QQMusicService::mapSong(song(13, "third"));
    check(service.toggleLike(third), "favorite command accepted");
    until([&] { return server.writes == 1 && service.likedLoaded() && service.isLiked("qqmusic://song/third"); });
    check(service.isLiked("qqmusic://song/third") && server.liked, "server-confirmed favorite updates QQ models only");
    check(!service.toggleLike(third), "two-second shared favorite cooldown");
    QTest::qWait(2050);
    server.failWrite = true;
    check(service.toggleLike(third), "unlike command submitted after cooldown");
    until([&] { return server.writes == 2 && service.likedLoaded() && !service.likedBusy(); });
    check(service.isLiked("qqmusic://song/third") && server.liked,
          "rejected favorite write preserves server-confirmed membership");
    QTest::qWait(2050);
    server.failWrite = false;
    check(service.toggleLike(third), "unlike can be retried explicitly after cooldown");
    until([&] { return server.writes == 3 && service.likedLoaded() && !service.isLiked("qqmusic://song/third"); });
    QTest::qWait(2050);
    auto mismatched = third;
    mismatched["nativeId"] = "999";
    check(service.toggleLike(mismatched), "favorite validates identity before attempting a write");
    until([&] { return !service.errorMessage().isEmpty(); });
    check(server.writes == 3 && !service.isLiked("qqmusic://song/third"),
          "mismatched MID and native ID cannot modify favorites");
    server.failPage = true;
    service.fetchLiked(true);
    until([&] { return !service.likedBusy(); });
    check(tracks.qqLiked()->count() == 2 && !service.errorMessage().isEmpty(),
          "partial refresh failure preserves complete previous liked model");
    server.failPage = false;
    server.delay = 150;
    service.search("old");
    service.search("latest");
    until([&] { return !service.searchBusy(); });
    check(tracks.qqSearch()->get(0).value("providerId") == "latest", "late old search cannot overwrite latest intent");
    service.search("logout");
    auth = {};
    service.syncSession();
    QTest::qWait(200);
    check(tracks.qqLiked()->count() == 0 && tracks.qqSearch()->count() == 0 && !service.likedLoaded() &&
              tracks.liked()->count() == 1,
          "logout cancels all QQ requests and clears its own account cache only");
    const int before = urls.count();
    service.resolve(track);
    service.cancelResolve();
    QTest::qWait(200);
    check(urls.count() == before, "cancelled resolver cannot start playback");
    service.shutdown();
    queue.shutdown();
    cloud.shutdown();
    Oran7Async::waitForDone();
    std::printf("PASS: %d QQ music business checks\n", checks);
}
