#ifndef NCMAUTHAPI_H
#define NCMAUTHAPI_H

#include <QJsonObject>
#include <QObject>
#include <QString>

class CurlHttpClient;

/**
 * @brief 网易云扫码认证 API
 *
 * 只负责发请求、解析响应并广播信号，不做状态机（状态机在 QrLoginController）。
 * 传输层用 CurlHttpClient（系统 curl.exe / Schannel），绕开网易 WAF 对 Qt 网络栈的封杀。
 */
class NcmAuthApi : public QObject
{
    Q_OBJECT

public:
    explicit NcmAuthApi(CurlHttpClient* curl, QObject* parent = nullptr);

    /// POST /weapi/login/qrcode/unikey 获取 unikey
    void createQrKey();

    /// POST /weapi/login/qrcode/client/login 轮询扫码状态
    void checkQr(const QString& unikey);

signals:
    void qrKeyReady(const QString& unikey, const QString& loginUrl);
    void qrCheckFinished(int code, const QJsonObject& data);
    void apiError(const QString& message);

private:
    void onUnikeyResult(int httpStatus, const QByteArray& body);
    void onCheckResult(int httpStatus, const QByteArray& body, const QString& setCookie);

    CurlHttpClient* m_curl = nullptr;
};

#endif // NCMAUTHAPI_H
