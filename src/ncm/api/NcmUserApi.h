#ifndef NCMUSERAPI_H
#define NCMUSERAPI_H

#include <QObject>
#include <QString>

#include <functional>

class CurlHttpClient;

/**
 * @brief 网易云用户 API（第一版只做账号信息）
 *
 * 走 CurlHttpClient（系统 curl.exe / Schannel），因为 QNetworkAccessManager 通道
 * 会被网易 WAF 概率性封杀成空响应（实测），拿不到账号昵称。
 */
class NcmUserApi : public QObject
{
    Q_OBJECT

public:
    explicit NcmUserApi(CurlHttpClient* curl, QObject* parent = nullptr);

    /// 设置鉴权 Cookie 提供器（返回 "MUSIC_U=..; __csrf=.."），account/get 需要
    void setCookieProvider(std::function<QString()> provider);

    /// 设置 csrf_token 提供器（登录后可能为空，能拿就拿）
    void setCsrfProvider(std::function<QString()> provider);

    /// POST /weapi/w/nuser/account/get 取登录用户昵称（尽力而为，失败不影响已登录态）
    void fetchAccount();

    /// 明文 POST /api/nuser/account/get 校验登录 Cookie 是否仍有效（供网络监测器周期调用）
    void verifyLogin();

signals:
    void accountFetched(const QString& nickname);
    void apiError(const QString& message);
    /// Cookie 校验结果：1=有效, 0=失效(profile:null), -1=网络/服务器异常(不判定为失效)
    void verifyResult(int valid);

private:
    void onAccountResult(int httpStatus, const QByteArray& body);
    void onVerifyLoginResult(int httpStatus, const QByteArray& body);

    CurlHttpClient* m_curl = nullptr;
    std::function<QString()> m_cookieProvider;
    std::function<QString()> m_csrfProvider;
};

#endif // NCMUSERAPI_H
