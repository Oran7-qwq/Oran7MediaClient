#ifndef ONLINELYRICSFETCHER_H
#define ONLINELYRICSFETCHER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QTextStream>

#include "GlobalHelper.h"

/**
 * @brief 在线歌词获取器
 *
 * 搜索策略：LRCLIB 精确匹配 → LRCLIB 模糊搜索 → 网易云音乐搜索
 * 找到带时间轴的歌词后保存到本地 .lrc 文件，复用 Client::lyricsAvailable 信号。
 */
class OnlineLyricsFetcher : public QObject
{
    Q_OBJECT

public:
    explicit OnlineLyricsFetcher(QObject *parent = nullptr)
        : QObject(parent)
        , m_networkManager(new QNetworkAccessManager(this))
    {}

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

        m_currentArtist    = artist;
        m_currentTitle     = title;
        m_currentFilePath  = mediaFilePath;
        m_currentLrcDirPath = lrcDirPath;

        INFO_LOG << "OnlineLyricsFetcher: searching for" << artist << "-" << title;

        // 第一步：LRCLIB 精确匹配
        searchLrclibExact(artist, title);
    }

    /** 取消正在进行的网络请求 */
    void cancel()
    {
        if (m_activeReply) {
            m_activeReply->abort();
            m_activeReply->deleteLater();
            m_activeReply = nullptr;
        }
        m_currentFilePath.clear();
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
                [this]() { onLrclibExactReply(); });
    }

    void onLrclibExactReply()
    {
        QNetworkReply *reply = m_activeReply;
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
            INFO_LOG << "OnlineLyricsFetcher: found via LRCLIB exact match";
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
                [this]() { onLrclibSearchReply(); });
    }

    void onLrclibSearchReply()
    {
        QNetworkReply *reply = m_activeReply;
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
                INFO_LOG << "OnlineLyricsFetcher: found via LRCLIB search";
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
                [this]() { onNetEaseSearchReply(); });
    }

    void onNetEaseSearchReply()
    {
        QNetworkReply *reply = m_activeReply;
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
            INFO_LOG << "OnlineLyricsFetcher: NetEase search returned no results";
            emit lyricsNotFound();
            return;
        }

        // 取第一个匹配的 songId 去拿歌词
        qint64 songId = songs[0].toObject().value("id").toVariant().toLongLong();
        INFO_LOG << "OnlineLyricsFetcher: NetEase songId =" << songId;
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

        m_activeReply = m_networkManager->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this,
                [this]() { onNetEaseLyricsReply(); });
    }

    void onNetEaseLyricsReply()
    {
        QNetworkReply *reply = m_activeReply;
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
            INFO_LOG << "OnlineLyricsFetcher: found via NetEase";
            saveAndEmit(lrc);
        } else {
            INFO_LOG << "OnlineLyricsFetcher: NetEase lyrics empty or not synced";
            emit lyricsNotFound();
        }
    }

    // ──────────── 工具方法 ────────────

    /** 保存 LRC 内容到本地文件，emit lyricsFound */
    void saveAndEmit(const QString &lrcContent)
    {
        QFileInfo fi(m_currentFilePath);
        QString baseName = fi.completeBaseName();
        QString lrcPath  = m_currentLrcDirPath + "/" + baseName + ".lrc";

        QFile file(lrcPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << lrcContent;
            file.close();

            INFO_LOG << "OnlineLyricsFetcher: saved to" << lrcPath;
            emit lyricsFound(lrcPath);
        } else {
            INFO_LOG << "OnlineLyricsFetcher: failed to save" << lrcPath;
            emit lyricsNotFound();
        }
    }

    /** 检查当前搜索是否仍然有效（用户没切歌） */
    bool isStillCurrent() const { return !m_currentFilePath.isEmpty(); }

    QNetworkAccessManager *m_networkManager;

    QString m_currentFilePath;
    QString m_currentArtist;
    QString m_currentTitle;
    QString m_currentLrcDirPath;
    QNetworkReply *m_activeReply = nullptr;
};

#endif // ONLINELYRICSFETCHER_H
