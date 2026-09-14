#ifndef WEBLOGINCONTROLLER_H
#define WEBLOGINCONTROLLER_H

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTimer;
class QWebSocket;
class NcmSession;
class NcmUserApi;

/**
 * @brief 网易云网页登录控制器（QML 单例 NcmWebLogin）
 *
 * 8821 说明纯 API 二维码 check 通道被网易服务端安全环境校验拦截。
 * 本方案改为：启动本机真实 Chromium（Edge/Chrome），用户在官方网页里完成登录
 * （真实浏览器环境可通过风控），再用 Chrome DevTools Protocol(CDP) 的
 * Network.getAllCookies 把 HttpOnly 的 MUSIC_U/__csrf 读回 CookieStore。
 *
 * 流程：start → 启动浏览器 → CDP 连接 → 轮询 getAllCookies → 捕获 MUSIC_U
 *       → 注入 CookieStore → account/get 验证取昵称 → 关闭浏览器。
 */
class WebLoginController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WebLoginState loginState READ loginState NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged FINAL)

public:
    enum class WebLoginState {
        Idle,          // 未开始
        StartingBrowser, // 正在启动浏览器
        WaitingLogin,  // 等待用户在浏览器中登录
        Captured,      // 已捕获凭据，正在验证
        Success,       // 登录成功
        Error          // 出错
    };
    Q_ENUM(WebLoginState)

    explicit WebLoginController(NcmSession* session, NcmUserApi* userApi, QObject* parent = nullptr);

    Q_INVOKABLE void start();    // 幂等：活跃状态忽略
    Q_INVOKABLE void cancel();   // 关浏览器、断连接，回 Idle

    WebLoginState loginState() const;
    QString statusText() const;

signals:
    void stateChanged();
    void statusTextChanged();
    void loginSucceeded(const QString& nickname);
    void loginFailed(const QString& message);

private slots:
    void onBrowserStarted();
    void onBrowserFinished(int exitCode, QProcess::ExitStatus status);
    void onBrowserError();
    void probeTargets();
    void onWsConnected();
    void onWsTextMessage(const QString& message);
    void onWsError();
    void onWsDisconnected();
    void pollCookies();
    void onProbeReply(QNetworkReply* reply);
    void onAccountFetched(const QString& nickname);

private:
    static QString findBrowserExecutable();
    static int findFreePort();
    QString makeTempProfileDir();

    void setState(WebLoginState s);
    void setStatus(const QString& t);
    void sendCdp(int id, const QString& method, const QJsonObject& params);
    void handleCookies(const QJsonArray& cookies);
    void verifyAndFinish();
    void finish(bool success, const QString& nickname, const QString& msg);
    void cleanupBrowser();

    NcmSession* m_session = nullptr;
    NcmUserApi* m_userApi = nullptr;
    QProcess* m_browser = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
    QWebSocket* m_ws = nullptr;
    QTimer* m_probeTimer = nullptr;
    QTimer* m_cookieTimer = nullptr;
    QTimer* m_verifyTimer = nullptr;
    QTimer* m_probeTimeoutTimer = nullptr;

    int m_port = 0;
    int m_cmdId = 0;
    int m_cookieCmdId = 0;
    QString m_wsUrl;
    QString m_tempProfileDir;
    bool m_captured = false;
    bool m_verified = false;
    bool m_cleaningUp = false;

    WebLoginState m_state = WebLoginState::Idle;
    QString m_statusText;
};

#endif // WEBLOGINCONTROLLER_H
