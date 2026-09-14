#ifndef NCMSONGAPI_H
#define NCMSONGAPI_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class CurlHttpClient;

/**
 * @brief 网易云歌曲播放地址解析 API
 *
 * 明文 POST /api/song/enhance/player/url（ids + br），带登录 Cookie。
 * 注意：必须走 CurlHttpClient(curl.exe/Schannel)，QNAM 通道会被网易对 VIP 歌判定为
 * 异常客户端返回 code:-110（实测 curl 同参数返回 200）。
 */
class NcmSongApi : public QObject
{
    Q_OBJECT

public:
    explicit NcmSongApi(CurlHttpClient* curl, QObject* parent = nullptr);

    void setCookieProvider(std::function<QString()> provider);

    /// 批量解析歌曲播放地址（一次请求多个 id）
    void resolveUrls(const QList<qint64>& ids);

signals:
    /// id -> 播放地址；无权限/未登录的歌曲不在 map 里
    void urlsResolved(const QHash<qint64, QString>& urlMap);
    void apiError(const QString& message);

private:
    void onResult(int httpStatus, const QByteArray& body);

    CurlHttpClient* m_curl = nullptr;
    std::function<QString()> m_cookieProvider;
};

#endif // NCMSONGAPI_H