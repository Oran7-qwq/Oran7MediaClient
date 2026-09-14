#include "HttpClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <utility>

HttpClient::HttpClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void HttpClient::setCookieProvider(std::function<QString()> provider)
{
    m_cookieProvider = std::move(provider);
}

QNetworkReply* HttpClient::send(const NcmRequest& req)
{
    QNetworkRequest request(req.url);
    for (auto it = req.headers.constBegin(); it != req.headers.constEnd(); ++it)
        request.setRawHeader(it.key(), it.value());
    // 网易云部分端点对 HTTP/2 返回空响应，协议层统一置 disableHttp2
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, req.disableHttp2 ? false : true);

    // 鉴权 Cookie 显式注入
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            request.setRawHeader("Cookie", cookie.toUtf8());
    }

    if (req.isPost)
        return m_nam->post(request, req.body);
    return m_nam->get(request);
}

QNetworkAccessManager* HttpClient::nam() const
{
    return m_nam;
}
