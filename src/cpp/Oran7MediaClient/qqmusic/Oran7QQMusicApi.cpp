#include "Oran7QQMusicApi.h"
#include "Oran7QQMusicWebProtocol.h"
#include "Oran7GlobalHelper.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QTimer>
#include <QDateTime>
#include <memory>
Oran7QQMusicApi::Oran7QQMusicApi(QObject* parent) : Oran7QQMusicApi(Oran7Options{}, parent) {}
Oran7QQMusicApi::Oran7QQMusicApi(Oran7Options options, QObject* parent) : QObject(parent), m_options(std::move(options))
{
}
Oran7QQMusicApi::~Oran7QQMusicApi()
{
    cancelAll();
}
quint64 Oran7QQMusicApi::request(const QString& module, const QString& method, const QJsonObject& parameters,
                                 const Oran7QQMusicAuth& auth, bool write)
{
    auto comm = auth.verificationRequest().value("comm").toObject();
    comm["inCharset"] = "utf-8";
    comm["outCharset"] = "utf-8";
    if (!auth.isValid())
    {
        comm.remove("authst");
        comm["uin"] = "0";
        comm["ct"] = 24;
    }
    if (write)
    {
        const auto cookies = auth.values();
        const auto hash = [](const QString& key)
        {
            quint32 h = 5381;
            for (auto c : key)
                h = h * 33 + c.unicode();
            return int(h & 0x7fffffff);
        };
        comm["ct"] = 24;
        comm["cv"] = 4747474;
        comm["platform"] = "yqq.json";
        comm["g_tk"] = hash(cookies.value("skey", cookies.value("qqmusic_key", auth.musicKey())));
        comm["g_tk_new_20200303"] = hash(cookies.value("qqmusic_key", cookies.value("p_skey", auth.musicKey())));
        comm["needNewCode"] = 1;
        comm["inCharset"] = "utf-8";
        comm["outCharset"] = "utf-8";
    }
    // 搜索接口要求以 module 为信封键，且不能附带 comm；否则可能成功返回空结果。
    const bool search = module == QStringLiteral("music.search.SearchCgiService");
    const auto responseKey = search ? module : QStringLiteral("req");
    QJsonObject envelope{{responseKey, QJsonObject{{"module", module}, {"method", method}, {"param", parameters}}}};
    if (!search)
        envelope["comm"] = comm;
    const auto json = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    auto url = write ? m_options.writeEndpoint : m_options.endpoint;
    QByteArray body = json;
    if (write)
    {
        body = Oran7QQMusicWebProtocol::encrypt(json);
        if (body.isEmpty())
            return 0;
        QUrlQuery q;
        q.addQueryItem("encoding", "ag-1");
        q.addQueryItem("sign", Oran7QQMusicWebProtocol::signature(json));
        q.addQueryItem("_", QString::number(QDateTime::currentMSecsSinceEpoch()));
        url.setQuery(q);
    }
    return submit(url, body, auth, write, false, responseKey);
}
quint64 Oran7QQMusicApi::lyrics(const QString& mid, const Oran7QQMusicAuth& auth)
{
    auto url = m_options.lyricsEndpoint;
    QUrlQuery q;
    q.addQueryItem("songmid", mid);
    q.addQueryItem("format", "json");
    q.addQueryItem("nobase64", "1");
    q.addQueryItem("g_tk", "5381");
    q.addQueryItem("platform", "yqq.json");
    q.addQueryItem("loginUin", auth.userId.isEmpty() ? "0" : auth.userId);
    url.setQuery(q);
    return submit(url, {}, auth, false, true);
}
quint64 Oran7QQMusicApi::submit(const QUrl& url, const QByteArray& body, const Oran7QQMusicAuth& auth, bool write,
                                bool lyrics, const QString& responseKey)
{
    const auto id = ++m_nextRequest;
    QNetworkRequest request(url);
    request.setRawHeader("Referer", "https://y.qq.com/portal/player.html");
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/130.0.0.0 Safari/537.36");
    if (auth.isValid())
        request.setRawHeader("Cookie", auth.cookieHeader());
    request.setRawHeader("Accept", write ? "application/octet-stream" : "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      write ? QStringLiteral("text/plain") : QStringLiteral("application/json;charset=utf-8"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(m_options.timeoutMs);
    auto* reply = lyrics ? m_network.get(request) : m_network.post(request, body);
    m_pending.insert(id, reply);
    reply->setReadBufferSize(4 * 1024 * 1024);
    auto bytes = std::make_shared<QByteArray>();
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->start(m_options.timeoutMs);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::readyRead, this,
            [reply, bytes]
            {
                bytes->append(reply->readAll());
                if (bytes->size() > 4 * 1024 * 1024)
                    reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, id, reply, bytes, write, lyrics, responseKey]
            {
                reply->deleteLater();
                if (!m_pending.remove(id))
                    return;
                bytes->append(reply->readAll());
                if (reply->error() != QNetworkReply::NoError ||
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
                    bytes->size() > 4 * 1024 * 1024)
                {
                    emit finished(id, {}, QStringLiteral("QQ 音乐网络请求失败，请检查网络后重试"));
                    return;
                }
                if (write)
                    *bytes = Oran7QQMusicWebProtocol::decodeResponse(*bytes);
                QJsonParseError parse;
                const auto doc = QJsonDocument::fromJson(*bytes, &parse);
                bytes->fill('\0');
                const auto root = doc.object(), block = root.value(responseKey).toObject();
                const auto data = lyrics ? root : block.value("data").toObject();
                const auto code = lyrics ? root.value("code") : block.value("code");
                NETWORK_LOG << "QQMusic request=" << id << " code=" << code.toInt(-1);
                const bool failed = parse.error != QJsonParseError::NoError || !doc.isObject() ||
                                    root.value("code") != QJsonValue(0) ||
                                    (!lyrics && (code != QJsonValue(0) || !block.value("data").isObject())) ||
                                    (data.contains("code") && data.value("code") != QJsonValue(0)) ||
                                    (data.contains("subcode") && data.value("subcode") != QJsonValue(0)) ||
                                    (write && data.value("retCode") != QJsonValue(0));
                emit finished(id, failed ? QJsonObject{} : data,
                              failed ? QStringLiteral("QQ 音乐接口请求失败，请稍后重试") : QString());
            });
    return id;
}
void Oran7QQMusicApi::cancel(quint64 id)
{
    const auto reply = m_pending.take(id);
    if (reply)
        reply->abort();
}
void Oran7QQMusicApi::cancelAll()
{
    const auto replies = m_pending;
    m_pending.clear();
    for (const auto& reply : replies)
        if (reply)
            reply->abort();
}
