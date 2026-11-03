#ifndef CURLHTTPCLIENT_H
#define CURLHTTPCLIENT_H

#include <QObject>
#include <QString>
#include <QThread>

#include "NcmRequest.h"

// 沿用已验证的系统 curl.exe / Schannel 通道与 HTTP/1.1 参数。
// 请求携带唯一 ID 和操作 tag，回调原样返回；不输出 Cookie、响应正文或原始 stderr。
// 专用线程拥有 QProcess/临时文件，前端仅提交值请求并在所属线程接收结果。
// executable/temporaryRoot 可注入隔离传输程序和临时目录，默认仍选择系统 curl。
class Oran7CurlHttpWorker;
class CurlHttpClient : public QObject
{
    Q_OBJECT

public:
    explicit CurlHttpClient(QObject* parent = nullptr, const QString& executable = {}, const QString& temporaryRoot = {});
    ~CurlHttpClient() override;

    /// 异步发送 POST 请求
    void send(const NcmRequest& req);

    /// 系统 curl.exe 路径；找不到返回空串
    static QString curlPath();

signals:
    void requestFinished(quint64 requestId, const QString& tag, int httpStatus,
                         const QByteArray& body, const QString& setCookie);
    void requestError(quint64 requestId, const QString& tag, const QString& message);

private:
    QThread m_thread;
    Oran7CurlHttpWorker* m_worker = nullptr; // thread.finished -> deleteLater，前端不访问后台子对象。
};

#endif // CURLHTTPCLIENT_H
