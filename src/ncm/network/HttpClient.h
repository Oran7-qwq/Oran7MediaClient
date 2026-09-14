#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <QString>

#include <functional>

#include "NcmRequest.h"

class QNetworkAccessManager;
class QNetworkReply;

/**
 * @brief 纯网络层：只懂 QNetworkRequest/QNetworkAccessManager
 *
 * 不认识 WEAPI，也不持有任何业务 Cookie 概念。
 * 鉴权 Cookie 通过 cookieProvider 显式注入（不做 QNAM 自动 cookie jar，
 * 避免带出 os 类 cookie 触发风控）。
 */
class HttpClient : public QObject
{
    Q_OBJECT

public:
    explicit HttpClient(QObject* parent = nullptr);

    /// 设置鉴权 Cookie 提供器（返回 "MUSIC_U=..; __csrf=.."），仅对需要登录态的请求注入
    void setCookieProvider(std::function<QString()> provider);

    QNetworkReply* send(const NcmRequest& req);
    QNetworkAccessManager* nam() const;

private:
    QNetworkAccessManager* m_nam = nullptr;
    std::function<QString()> m_cookieProvider;
};

#endif // HTTPCLIENT_H
