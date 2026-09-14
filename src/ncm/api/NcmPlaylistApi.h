#ifndef NCMPLAYLISTAPI_H
#define NCMPLAYLISTAPI_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class CurlHttpClient;
class HttpClient;
class QNetworkReply;

/**
 * @brief 网易云"我喜欢的音乐"歌单 API
 *
 * 明文接口（与 /api/search/get 同款，QNAM 可过风控），带登录 Cookie：
 *   POST /api/nuser/account/get        → userId
 *   GET  /api/user/playlist?uid=       → "<昵称>喜欢的音乐" 歌单 id（specialType==5）
 *   GET  /api/v6/playlist/detail?id=   → trackIds（完整歌曲 id 列表，保持云端顺序）
 *   GET  /api/song/detail?ids=[...]    → 每首歌元数据（分批）
 *
 * 注：/weapi/song/like/get（likelist）返回的 ids 无序（官方标注"无序"），
 *     不能用于还原"我喜欢的音乐"歌单顺序，故改为经歌单详情接口取 trackIds。
 *
 * 喜欢/取消喜欢（setLike）走 WEAPI + curl（登录类写操作，curl 过 WAF）：
 *   POST /weapi/song/like?csrf_token=  body {like, trackId}
 *
 * 拖拽排序同步（reorderLikedSongs）同走 WEAPI + curl：
 *   POST /weapi/playlist/manipulate/tracks  body {op:"update", pid, trackIds:"[新顺序id数组]"}
 *   （旧 /weapi/song/order/update 已 404 下线）
 *
 * 网易云漫游（心动模式）roamSongs 同走 WEAPI + curl：
 *   POST /weapi/playmode/intelligence/list  body {songId, type:"fromPlayOne", playlistId, startMusicId, count}
 *   以"我喜欢的音乐"歌单 id 作推荐种子，返回相似风格歌曲列表
 */
class NcmPlaylistApi : public QObject
{
    Q_OBJECT
    // "我喜欢的音乐"歌单 id：fetch 已移入 Oran7AsyncManager handler，由 QML 拉取成功后回写，
    // 供 roamSongs / reorderLikedSongs 使用
    Q_PROPERTY(qint64 likedPlaylistId READ likedPlaylistId WRITE setLikedPlaylistId
               NOTIFY likedPlaylistIdChanged FINAL)

public:
    explicit NcmPlaylistApi(HttpClient* http, CurlHttpClient* curl, QObject* parent = nullptr);

    void setCookieProvider(std::function<QString()> provider);
    void setCsrfProvider(std::function<QString()> provider);

    qint64 likedPlaylistId() const;
    void setLikedPlaylistId(qint64 id);

    /// 拉取"我喜欢的音乐"歌单，结果经 likedSongReady / likedSongsFinished 逐个/汇总返回
    Q_INVOKABLE void fetchLikedSongs();

    /// 喜欢/取消喜欢（同步网易云账号），结果经 likeResult 返回
    Q_INVOKABLE void setLike(qint64 songId, bool like);

    /// 把拖拽后的新顺序同步给网易云"我喜欢的音乐"歌单，结果经 reorderResult 返回
    Q_INVOKABLE void reorderLikedSongs(const QVariantList& songIds);

    /// 网易云漫游（心动模式）：按 songId + 红心歌单请求相似歌曲推荐，结果经 roamSongsReady 返回
    Q_INVOKABLE void roamSongs(qint64 songId);

signals:
    void likedPlaylistIdChanged();
    void likedSongReady(qint64 songId, const QString& icon, const QString& name,
                        const QString& artist, const QString& album, const QString& timesize, bool isVip);
    void likedSongsFinished(int count);
    void likedSongsFailed(const QString& message);
    void likeResult(qint64 songId, bool like, bool success, const QString& message);
    void reorderResult(bool success, const QString& message);
    /// 漫游推荐歌曲列表：每项 {songId, icon, name, artist, album, timesize, isVip}
    void roamSongsReady(const QVariantList& songs);
    void roamSongsFailed(const QString& message);

private:
    void onAccountReply(int httpStatus, const QByteArray& body);
    void fetchLikedPlaylist(qint64 uid);                  // 按 uid 找"我喜欢的音乐"歌单 id
    void onUserPlaylistsReply(int httpStatus, const QByteArray& body);
    void fetchPlaylistDetail(qint64 playlistId);          // 取歌单 trackIds（保序）
    void onPlaylistDetailReply(int httpStatus, const QByteArray& body);
    void fetchNextSongDetailBatch();
    void onSongDetailReply(int httpStatus, const QByteArray& body);
    void onLikeResult(int httpStatus, const QByteArray& body, const QString& tag);
    void onReorderResult(int httpStatus, const QByteArray& body);
    void onRoamReply(int httpStatus, const QByteArray& body);

    HttpClient* m_http = nullptr;
    CurlHttpClient* m_curl = nullptr;
    std::function<QString()> m_cookieProvider;
    std::function<QString()> m_csrfProvider;

    QList<qint64> m_pendingTrackIds;   // 待分批 song/detail 的歌曲 id
    int m_totalTrackCount = 0;         // trackIds 总数（用于 finished 汇总）
    QList<qint64> m_likedIdOrder;      // 歌单 trackIds 顺序（保证列表顺序与云端一致）
    QHash<qint64, QVariantMap> m_songMetaMap;   // songId -> 元数据，全部分批完成后按顺序 emit
    qint64 m_likedPlaylistId = 0;      // "我喜欢的音乐"歌单 id（拖拽排序同步用 pid）
    qint64 m_lastLikeSongId = 0;
    bool m_lastLikeValue = false;
    bool m_fetchInProgress = false;   // 拉取进行中，避免并发拉取互相污染共享状态
};

#endif // NCMPLAYLISTAPI_H