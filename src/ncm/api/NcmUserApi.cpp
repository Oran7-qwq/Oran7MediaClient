#include "NcmUserApi.h"
#include "CurlHttpClient.h"
#include "NcmProtocol.h"
#include <QJsonDocument>
#include <QUrlQuery>
#include <utility>

NcmUserApi::NcmUserApi(CurlHttpClient* curl, QObject* parent) : QObject(parent), m_curl(curl)
{
    if (!curl) return;
    connect(curl, &CurlHttpClient::requestFinished, this,
        [this](quint64 id, const QString& tag, int status, const QByteArray& body, const QString&) { finish(id, tag, status, body); });
    connect(curl, &CurlHttpClient::requestError, this,
        [this](quint64 id, const QString& tag, const QString& error) { finish(id, tag, 0, {}, error); });
}
void NcmUserApi::setCookieProvider(std::function<QString()> provider) { m_cookieProvider = std::move(provider); }
void NcmUserApi::setCsrfProvider(std::function<QString()> provider) { m_csrfProvider = std::move(provider); }
void NcmUserApi::send(NcmRequest request)
{
    const auto cookie = m_cookieProvider ? m_cookieProvider() : QString();
    if (!cookie.isEmpty()) request.headers.insert("Cookie", cookie.toUtf8());
    m_pending.insert(request.id, Oran7Query{request.tag, cookie});
    if (m_curl) m_curl->send(request);
    else finish(request.id, request.tag, 0, {}, QStringLiteral("账号传输不可用"));
}
void NcmUserApi::fetchAccount(quint64 requestId)
{
    QJsonObject payload;
    const auto csrf = m_csrfProvider ? m_csrfProvider() : QString();
    if (!csrf.isEmpty()) payload["csrf_token"] = csrf;
    NcmProtocol protocol;
    auto request = protocol.prepareWeapi("/weapi/w/nuser/account/get", payload);
    request.id = requestId;
    request.tag = "account";
    send(request);
}
void NcmUserApi::verifyLogin(quint64 requestId)
{
    NcmRequest request;
    request.id = requestId;
    request.tag = "verifyLogin";
    request.url = QUrl("https://music.163.com/api/nuser/account/get");
    QUrlQuery query;
    query.addQueryItem("csrf_token", m_csrfProvider ? m_csrfProvider() : QString());
    request.body = query.toString(QUrl::FullyEncoded).toUtf8();
    request.headers = {{"Content-Type","application/x-www-form-urlencoded"},{"Referer","https://music.163.com"},
        {"User-Agent","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120 Safari/537.36"}};
    send(request);
}
void NcmUserApi::finish(quint64 id, const QString& tag, int status, const QByteArray& body, const QString& error)
{
    const auto it = m_pending.constFind(id);
    if (it == m_pending.cend() || it->tag != tag) return;
    const auto query = m_pending.take(id);
    if (m_cookieProvider && query.cookie != m_cookieProvider()) return;
    const auto root = QJsonDocument::fromJson(body).object();
    const bool ok = error.isEmpty() && status == 200 && root.value("code").toInt() == 200;
    if (tag == "verifyLogin") {
        emit verifyResult(id, ok ? (root.value("profile").toObject().value("userId").toVariant().toLongLong() > 0 ? 1 : 0) : -1);
    } else if (!ok) {
        emit apiError(id, error.isEmpty() ? QStringLiteral("获取用户信息失败") : error);
    } else {
        auto nickname = root.value("profile").toObject().value("nickname").toString();
        if (nickname.isEmpty()) nickname = root.value("data").toObject().value("profile").toObject().value("nickname").toString();
        emit accountFetched(id, nickname.isEmpty() ? QStringLiteral("网易云用户") : nickname);
    }
}
