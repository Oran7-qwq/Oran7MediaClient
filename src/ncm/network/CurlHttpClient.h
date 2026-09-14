#ifndef CURLHTTPCLIENT_H
#define CURLHTTPCLIENT_H

#include <QObject>
#include <QString>

#include "NcmRequest.h"

/**
 * @brief 基于系统 curl.exe（Schannel TLS）的 HTTP 传输层
 *
 * 背景：网易云 WAF 对 Qt 的 QNetworkAccessManager 传输层做概率性空响应封杀（实测），
 * 而 Windows 自带 curl.exe（C:\Windows\System32\curl.exe，Schannel）100% 通过。
 * 扫码登录的 weapi 请求（unikey / check）走此通道。
 *
 * 用法：send() 异步发送，结果经 requestFinished / requestError 信号返回，tag 用于区分请求。
 */
class CurlHttpClient : public QObject
{
    Q_OBJECT

public:
    explicit CurlHttpClient(QObject* parent = nullptr);

    /// 异步发送 POST 请求
    void send(const NcmRequest& req, const QString& tag = QString());

    /// 系统 curl.exe 路径；找不到返回空串
    static QString curlPath();

signals:
    void requestFinished(int httpStatus, const QByteArray& body,
                         const QString& setCookie, const QString& tag);
    void requestError(const QString& message, const QString& tag);

private:
    QString m_curl;
};

#endif // CURLHTTPCLIENT_H
