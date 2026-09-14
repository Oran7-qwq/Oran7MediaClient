#include "NcmSongApi.h"

#include "CurlHttpClient.h"
#include "GlobalHelper.h"
#include "NcmRequest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include <utility>

NcmSongApi::NcmSongApi(CurlHttpClient* curl, QObject* parent)
    : QObject(parent)
    , m_curl(curl)
{
    connect(m_curl, &CurlHttpClient::requestFinished, this,
            [this](int httpStatus, const QByteArray& body, const QString&, const QString& tag) {
                if (tag == QLatin1String("songurl"))
                    onResult(httpStatus, body);
            });
    connect(m_curl, &CurlHttpClient::requestError, this,
            [this](const QString& msg, const QString& tag) {
                if (tag == QLatin1String("songurl"))
                    emit apiError(msg);
            });
}

void NcmSongApi::setCookieProvider(std::function<QString()> provider)
{
    m_cookieProvider = std::move(provider);
}

void NcmSongApi::resolveUrls(const QList<qint64>& ids)
{
    if (ids.isEmpty())
        return;

    QStringList idStrs;
    idStrs.reserve(ids.size());
    for (qint64 id : ids)
        idStrs << QString::number(id);

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("ids"),
                   QStringLiteral("[%1]").arg(idStrs.join(QLatin1Char(','))));
    q.addQueryItem(QStringLiteral("br"), QStringLiteral("320000"));

    NcmRequest req;
    req.url = QUrl(QStringLiteral("https://music.163.com/api/song/enhance/player/url"));
    req.isPost = true;
    req.body = q.toString(QUrl::FullyEncoded).toUtf8();
    req.headers.insert(QByteArrayLiteral("Content-Type"),
                       QByteArrayLiteral("application/x-www-form-urlencoded"));
    req.headers.insert(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://music.163.com"));
    req.headers.insert(QByteArrayLiteral("User-Agent"),
        QByteArrayLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                          "(KHTML, like Gecko) Chrome/120 Safari/537.36"));

    // 关键：带完整登录 Cookie（MUSIC_U + __csrf + os=pc + appver），否则 VIP 歌被拒
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }

    m_curl->send(req, QStringLiteral("songurl"));
}

void NcmSongApi::onResult(int httpStatus, const QByteArray& body)
{
    NETWORK_LOG << "NcmSongApi: song url http=" << httpStatus
                << " body=" << QString::fromUtf8(body).left(160);

    QHash<qint64, QString> map;
    if (httpStatus == 200 && !body.trimmed().isEmpty()) {
        const QJsonObject root = QJsonDocument::fromJson(body).object();
        const QJsonArray data = root["data"].toArray();
        for (const QJsonValue& v : data) {
            const QJsonObject o = v.toObject();
            const qint64 id = o["id"].toVariant().toLongLong();
            const QString url = o["url"].toString();
            if (!url.isEmpty())
                map.insert(id, url);
        }
    }
    emit urlsResolved(map);
}