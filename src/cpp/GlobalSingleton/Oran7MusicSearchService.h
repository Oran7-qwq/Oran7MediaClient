#ifndef ORAN7MUSICSEARCHSERVICE_H
#define ORAN7MUSICSEARCHSERVICE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>

#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * @brief 网易云音乐搜索服务（单例）
 *
 * 直连网易云官方明文接口（无需加密/登录，与 OnlineLyricsFetcher 同款）：
 *   1) 搜索：GET /api/search/get?s=<关键词>&type=1&limit=30
 *   2) 批量补封面：GET /api/song/detail?ids=[id1,id2,..]（/search/get 的 album 里没有 picUrl）
 * 可播放 URL 用外链接口：https://music.163.com/song/media/outer/url?id=<id>.mp3
 *
 * QML 使用：
 *   import Oran7MusicSearch 1.0
 *   Oran7MusicSearchService.search(keywords)
 *   Connections { target: Oran7MusicSearchService;
 *                 function onSearchResultReady(icon,name,artist,album,timesize,url) {} }
 */
class Oran7MusicSearchService : public QObject
{
    Q_OBJECT

public:
    static Oran7MusicSearchService& instance();
    static void cleanup();

    Q_INVOKABLE void search(const QString &keywords);

    /// 取某个搜索结果播放地址对应的歌曲 id（真实 music.126.net 地址不含 id，靠此映射反查）
    Q_INVOKABLE qint64 songIdForUrl(const QString& url) const;

signals:
    void searchStarted();
    void searchResultReady(const QString &icon,
                           const QString &name,
                           const QString &artist,
                           const QString &album,
                           const QString &timesize,
                           const QString &url,
                           bool isVip);
    void searchFinished(int count);
    void searchFailed(const QString &message);

private:
    explicit Oran7MusicSearchService(QObject *parent = nullptr);
    ~Oran7MusicSearchService() override;

    Oran7MusicSearchService(const Oran7MusicSearchService&) = delete;
    Oran7MusicSearchService& operator=(const Oran7MusicSearchService&) = delete;

    struct PendingSong {
        qint64 id = 0;
        QString name;
        QString artist;
        QString album;
        QString timesize;      // 秒
        QString fallbackPic;   // 搜索阶段拿到的歌手头像，详情接口失败时兜底
        bool isVip = false;    // fee==1 视为 VIP，需会员才能播放
    };

    void onSearchReply(QNetworkReply *reply);
    void fetchDetails();
    void onDetailReply(QNetworkReply *reply);
    void onUrlsResolved(const QHash<qint64, QString> &urlMap);
    void emitAllResults(const QHash<qint64, QString> &picMap,
                        const QHash<qint64, QString> &urlMap);

    static QString joinArtists(const QJsonValue &artistsVal);
    static int durationSeconds(const QJsonObject &song);

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_activeReply = nullptr;
    QVector<PendingSong> m_pendingSongs;
    QHash<qint64, QString> m_picMap;   // 详情阶段解析出的专辑封面
    QHash<QString, qint64> m_songIdByUrl;  // 播放地址 → songId（真实地址不含 id，供歌词反查）
    int m_generation = 0;              // 每次搜索递增，防止过期 URL 解析结果串台
    int m_resolveGen = -1;             // 发起 URL 解析时的 generation，用于丢弃过期结果

    static const QString kSearchUrl;
};

#endif // ORAN7MUSICSEARCHSERVICE_H
