#ifndef ORAN7ONLINELYRICSFETCHER_H
#define ORAN7ONLINELYRICSFETCHER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QPointer>
#include <memory>
#include <utility>
#include <QTextStream>
#include <QRegularExpression>

#include "Oran7GlobalHelper.h"
#include "NcmSession.h"

/**
 * @brief 在线歌词获取器
 *
 * 搜索策略：LRCLIB 精确匹配 → LRCLIB 模糊搜索 → 网易云音乐搜索
 * 找到带时间轴的歌词后保存到本地 .lrc 文件，复用 Oran7MediaClient::lyricsAvailable 信号。
 */
class Oran7OnlineLyricsFetcher : public QObject
{
    Q_OBJECT

public:
    explicit Oran7OnlineLyricsFetcher(QObject *parent = nullptr)
        : Oran7OnlineLyricsFetcher(std::make_unique<QNetworkAccessManager>(), parent)
    {}

    explicit Oran7OnlineLyricsFetcher(std::unique_ptr<QNetworkAccessManager> networkManager,
                                     QObject* parent = nullptr)
        : QObject(parent), m_networkManager(std::move(networkManager))
    {
        Q_ASSERT(m_networkManager && !m_networkManager->parent());
    }

    ~Oran7OnlineLyricsFetcher() override {
        cancel();
        m_networkManager.reset(); // 所有回调守卫仍存活时，先销毁 reply 所有者。
    }

    /**
     * @brief 启动歌词搜索级联
     *
     * 会自动取消正在进行的搜索。整个搜索链是异步的。
     *
     * @param artist  歌手名
     * @param title   歌曲名
     * @param mediaFilePath 媒体文件完整路径（用于生成 .lrc 文件名和防过期检查）
     * @param lrcDirPath    本地歌词目录（保存 .lrc 文件的位置）
     */
    void search(const QString &artist, const QString &title,
                const QString &mediaFilePath, const QString &lrcDirPath)
    {
        cancel();

        m_currentSongId = 0;
        m_currentArtist    = artist;
        m_currentTitle     = title;
        m_currentFilePath  = mediaFilePath;
        m_currentLrcDirPath = lrcDirPath;

        INFO_LOG << "Oran7OnlineLyricsFetcher: searching for" << artist << "-" << title;

        // 第一步：LRCLIB 精确匹配
        searchLrclibExact(artist, title);
    }

    /**
     * @brief 直接按网易云 songId 拉歌词（用于网络资源，无本地文件可提元数据）
     *        跳过 LRCLIB 级联，直接命中网易云 api/song/lyric?id=<id>
     */
    void searchById(qint64 songId, const QString &mediaFilePath, const QString &lrcDirPath)
    {
        cancel();
        m_currentSongId = songId;
        m_currentArtist.clear();
        m_currentTitle.clear();
        m_currentFilePath  = mediaFilePath;
        m_currentLrcDirPath = lrcDirPath;

        INFO_LOG << "Oran7OnlineLyricsFetcher: fetching lyrics by NetEase songId" << songId;
        fetchNetEaseLyrics(songId);
    }

    /** 取消正在进行的网络请求 */
    void cancel()
    {
        // 先取出 reply 并置空 m_activeReply：abort() 可能同步触发 finished 回调，
        // 回调里会读 m_activeReply；若先 abort 再 deleteLater(m_activeReply)，
        // 回调可能已把 m_activeReply 置空，导致后续对 NULL 调用 deleteLater 崩溃。
        const auto reply = m_activeReply;
        m_activeReply = nullptr;
        m_currentFilePath.clear();
        if (reply) {
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            if (reply) reply->deleteLater();
        }
    }

signals:
    /** 找到歌词并已保存到本地 */
    void lyricsFound(const QString &lrcFilePath);
    /** 所有来源均未找到 */
    void lyricsNotFound();

private:
    // ──────────── LRCLIB 精确匹配 ────────────

    void searchLrclibExact(const QString &artist, const QString &title)
    {
        QUrl url("https://lrclib.net/api/get");
        QUrlQuery query;
        query.addQueryItem("artist_name", artist);
        query.addQueryItem("track_name", title);
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "Oran7MediaClient/1.0");

        m_activeReply = m_networkManager->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this,
                [this, reply = m_activeReply]() { onLrclibExactReply(reply); });
    }

    void onLrclibExactReply(QNetworkReply *reply)
    {
        // 快速切歌时 m_activeReply 可能已被 cancel()/新一轮 search 覆盖；
        // 只有该 reply 仍是当前请求时才处理，否则只清理这个旧 reply 并忽略，
        // 避免对 NULL 调用 deleteLater（崩溃）或误删新的 m_activeReply
        if (!reply || m_activeReply != reply)
            return;
        m_activeReply = nullptr;
        reply->deleteLater();

        if (!isStillCurrent()) return;
        if (reply->error() != QNetworkReply::NoError) {
            // 网络错误或 404 → 降级到模糊搜索
            searchLrclibSearch(m_currentArtist + " " + m_currentTitle);
            return;
        }

        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString synced = obj["syncedLyrics"].toString();

        if (!synced.isEmpty()) {
            INFO_LOG << "Oran7OnlineLyricsFetcher: found via LRCLIB exact match";
            saveAndEmit(synced);
        } else {
            // 有记录但无同步歌词 → 降级到模糊搜索
            searchLrclibSearch(m_currentArtist + " " + m_currentTitle);
        }
    }

    // ──────────── LRCLIB 模糊搜索 ────────────

    void searchLrclibSearch(const QString &query)
    {
        QUrl url("https://lrclib.net/api/search");
        QUrlQuery q;
        q.addQueryItem("q", query);
        url.setQuery(q);

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "Oran7MediaClient/1.0");

        m_activeReply = m_networkManager->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this,
                [this, reply = m_activeReply]() { onLrclibSearchReply(reply); });
    }

    void onLrclibSearchReply(QNetworkReply *reply)
    {
        // 快速切歌时 m_activeReply 可能已被 cancel()/新一轮 search 覆盖；
        // 只有该 reply 仍是当前请求时才处理，否则只清理这个旧 reply 并忽略
        if (!reply || m_activeReply != reply)
            return;
        m_activeReply = nullptr;
        reply->deleteLater();

        if (!isStillCurrent()) return;
        if (reply->error() != QNetworkReply::NoError) {
            // LRCLIB 完全失败 → 降级到网易云
            searchNetEase(m_currentArtist + " " + m_currentTitle);
            return;
        }

        QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
        for (const QJsonValue &val : std::as_const(arr)) {
            QString synced = val["syncedLyrics"].toString();
            if (!synced.isEmpty()) {
                INFO_LOG << "Oran7OnlineLyricsFetcher: found via LRCLIB search";
                saveAndEmit(synced);
                return;
            }
        }

        // LRCLIB 没有带时间轴的歌词 → 降级到网易云
        searchNetEase(m_currentArtist + " " + m_currentTitle);
    }

    // ──────────── 网易云：搜索歌曲 ────────────

    void searchNetEase(const QString &keyword)
    {
        QUrl url("https://music.163.com/api/search/get");
        QUrlQuery q;
        q.addQueryItem("s", keyword);
        q.addQueryItem("limit", "10");
        q.addQueryItem("type", "1");
        url.setQuery(q);

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "Oran7MediaClient/1.0");
        request.setRawHeader("Referer", "https://music.163.com");

        m_activeReply = m_networkManager->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this,
                [this, reply = m_activeReply]() { onNetEaseSearchReply(reply); });
    }

    void onNetEaseSearchReply(QNetworkReply *reply)
    {
        // 快速切歌时 m_activeReply 可能已被 cancel()/新一轮 search 覆盖；
        // 只有该 reply 仍是当前请求时才处理，否则只清理这个旧 reply 并忽略
        if (!reply || m_activeReply != reply)
            return;
        m_activeReply = nullptr;
        reply->deleteLater();

        if (!isStillCurrent()) return;
        if (reply->error() != QNetworkReply::NoError) {
            emit lyricsNotFound();
            return;
        }

        QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray songs = root["result"].toObject()["songs"].toArray();

        if (songs.isEmpty()) {
            INFO_LOG << "Oran7OnlineLyricsFetcher: NetEase search returned no results";
            emit lyricsNotFound();
            return;
        }

        // 取第一个匹配的 songId 去拿歌词
        qint64 songId = songs[0].toObject().value("id").toVariant().toLongLong();
        INFO_LOG << "Oran7OnlineLyricsFetcher: NetEase songId =" << songId;
        fetchNetEaseLyrics(songId);
    }

    // ──────────── 网易云：获取歌词 ────────────

    void fetchNetEaseLyrics(qint64 songId)
    {
        QUrl url("https://music.163.com/api/song/lyric");
        QUrlQuery q;
        q.addQueryItem("id", QString::number(songId));
        q.addQueryItem("lv", "1");
        url.setQuery(q);

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "Oran7MediaClient/1.0");
        request.setRawHeader("Referer", "https://music.163.com");

        // 会员歌曲的歌词需要登录态 Cookie 才能拉取（免费歌曲无登录也可拉）
        const QString cookie = NcmSession::instance().cookieString();
        if (!cookie.isEmpty())
            request.setRawHeader("Cookie", cookie.toUtf8());

        m_activeReply = m_networkManager->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this,
                [this, reply = m_activeReply]() { onNetEaseLyricsReply(reply); });
    }

    void onNetEaseLyricsReply(QNetworkReply *reply)
    {
        // 快速切歌时 m_activeReply 可能已被 cancel()/新一轮 search 覆盖；
        // 只有该 reply 仍是当前请求时才处理，否则只清理这个旧 reply 并忽略
        if (!reply || m_activeReply != reply)
            return;
        m_activeReply = nullptr;
        reply->deleteLater();

        if (!isStillCurrent()) return;
        if (reply->error() != QNetworkReply::NoError) {
            emit lyricsNotFound();
            return;
        }

        QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        QString lrc = root["lrc"].toObject()["lyric"].toString();

        if (!lrc.isEmpty() && lrc.contains("[")) {
            INFO_LOG << "Oran7OnlineLyricsFetcher: found via NetEase";
            saveAndEmit(lrc);
        } else {
            INFO_LOG << "Oran7OnlineLyricsFetcher: NetEase lyrics empty or not synced";
            emit lyricsNotFound();
        }
    }

    // ──────────── 工具方法 ────────────

    /** 保存 LRC 内容到本地文件，emit lyricsFound */
    void saveAndEmit(const QString &lrcContent)
    {
        QString baseName = lyricsBaseName(m_currentFilePath);
        QString lrcPath  = m_currentLrcDirPath + "/" + baseName + ".lrc";

        QSaveFile file(lrcPath);
        const QByteArray content = lrcContent.toUtf8();
        if (file.open(QIODevice::WriteOnly) && file.write(content) == content.size() && file.commit()) {
            emit lyricsFound(lrcPath);
        } else {
            emit lyricsNotFound();
        }
    }

    /** 检查当前搜索是否仍然有效（用户没切歌） */
    bool isStillCurrent() const { return !m_currentFilePath.isEmpty(); }

    /** 生成 .lrc 文件名主干：本地文件用原文件名；网络 URL 用 song_<id>（URL 里的 ? 在 Windows 文件名非法） */
    QString lyricsBaseName(const QString &mediaFilePath) const
    {
        if (mediaFilePath.startsWith(QStringLiteral("http://"))
                || mediaFilePath.startsWith(QStringLiteral("https://"))) {
            if (m_currentSongId > 0) return QStringLiteral("song_%1").arg(m_currentSongId);
            QRegularExpression re(QStringLiteral("id=(\\d+)"));
            auto m = re.match(mediaFilePath);
            return m.hasMatch() ? QStringLiteral("song_%1").arg(m.captured(1))
                                : QStringLiteral("network_song");
        }
        return QFileInfo(mediaFilePath).completeBaseName();
    }

    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    qint64 m_currentSongId = 0;

    QString m_currentFilePath;
    QString m_currentArtist;
    QString m_currentTitle;
    QString m_currentLrcDirPath;
    QPointer<QNetworkReply> m_activeReply;
};

#endif // ORAN7ONLINELYRICSFETCHER_H
