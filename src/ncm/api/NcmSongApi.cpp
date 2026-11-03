#include "NcmSongApi.h"
#include "CurlHttpClient.h"
#include "Oran7NcmJsonMapper.h"
#include <QJsonDocument>
#include <QUrlQuery>
#include <utility>

NcmSongApi::NcmSongApi(CurlHttpClient* curl, QObject* parent) : QObject(parent), m_curl(curl)
{
    if (!curl) return;
    connect(curl, &CurlHttpClient::requestFinished, this,
        [this](quint64 id, const QString& tag, int status, const QByteArray& body, const QString&) {
            if (tag == "songurl") finish(id, status, body);
        });
    connect(curl, &CurlHttpClient::requestError, this,
        [this](quint64 id, const QString& tag, const QString& error) {
            if (tag == "songurl") finish(id, 0, {}, error);
        });
}
void NcmSongApi::setCookieProvider(std::function<QString()> provider) { m_cookieProvider = std::move(provider); }
void NcmSongApi::resolveUrls(const QList<qint64>& ids, quint64 requestId)
{
    if (ids.isEmpty()) return;
    NcmRequest req;
    if (requestId) req.id = requestId;
    req.tag = "songurl";
    if (m_pending.contains(req.id)) return;
    QStringList values;
    for (auto id : ids) if (id > 0) values.append(QString::number(id));
    if (values.isEmpty()) return;
    QUrlQuery query;
    query.addQueryItem("ids", QStringLiteral("[%1]").arg(values.join(',')));
    query.addQueryItem("br", "320000");
    req.url = QUrl("https://music.163.com/api/song/enhance/player/url");
    req.body = query.toString(QUrl::FullyEncoded).toUtf8();
    req.headers = {{"Content-Type","application/x-www-form-urlencoded"},{"Referer","https://music.163.com"},
        {"User-Agent","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120 Safari/537.36"}};
    if (m_cookieProvider) req.headers.insert("Cookie", m_cookieProvider().toUtf8());
    m_pending.insert(req.id);
    if (m_curl) m_curl->send(req);
    else finish(req.id, 0, {}, QStringLiteral("播放地址传输不可用"));
}
void NcmSongApi::finish(quint64 id, int status, const QByteArray& body, const QString& error)
{
    if (!m_pending.remove(id)) return;
    const auto urls = status == 200 ? Oran7NcmJsonMapper::playbackUrls(QJsonDocument::fromJson(body).object()) : QHash<qint64, QString>();
    // 空结果同样携带 ID，由 Resolver 的原重试预算处理；不再广播无关联结果。
    QPointer<NcmSongApi> guard(this);
    if (!error.isEmpty()) emit apiError(id, error);
    if (guard) emit urlsResolved(id, urls);
}
