#pragma once
#include "NcmRequest.h"
#include <QObject>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <functional>
class CurlHttpClient;

// 播放地址走 curl/Schannel；每轮尝试的 ID 在发送前确定，以覆盖同步失败。
class NcmSongApi : public QObject
{
    Q_OBJECT
public:
    explicit NcmSongApi(CurlHttpClient* curl, QObject* parent = nullptr);
    void setCookieProvider(std::function<QString()> provider);
    void resolveUrls(const QList<qint64>& ids, quint64 requestId = 0);
signals:
    void urlsResolved(quint64 requestId, const QHash<qint64, QString>& urls);
    void apiError(quint64 requestId, const QString& message);
private:
    void finish(quint64 id, int status, const QByteArray& body, const QString& error = {});
    QPointer<CurlHttpClient> m_curl;
    std::function<QString()> m_cookieProvider;
    QSet<quint64> m_pending;
};
