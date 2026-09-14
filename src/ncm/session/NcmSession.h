#ifndef NCMSESSION_H
#define NCMSESSION_H

#include <QObject>
#include <QString>

class CookieStore;
class CurlHttpClient;
class HttpClient;
class NcmAuthApi;
class NcmPlaylistApi;
class NcmSongApi;
class NcmUserApi;
class QrLoginController;
class WebLoginController;

/**
 * @brief 网易云会话门面（单例）
 *
 * 统一持有 HttpClient / CookieStore / NcmAuthApi / NcmUserApi / QrLoginController。
 * 对 QML 暴露 isLoggedIn / userName / logout，对 C++ 暴露 cookieString / ffmpegCookieString。
 *
 * 使用方式：
 *   QML:  NcmSession.isLoggedIn / .userName / .logout()
 *   C++:  NcmSession::instance().ffmpegCookieString()（Oran7FFplayerCore 用）
 */
class NcmSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY loginStatusChanged FINAL)
    Q_PROPERTY(QString userName READ userName NOTIFY loginStatusChanged FINAL)
    Q_PROPERTY(QString csrfToken READ csrfToken NOTIFY loginStatusChanged FINAL)
    // 供 QML 在 GUI 线程快照登录 Cookie，作为参数传给 Oran7AsyncManager 的线程池 handler
    Q_PROPERTY(QString cookieString READ cookieString NOTIFY loginStatusChanged FINAL)

public:
    static NcmSession& instance();
    static void cleanup();

    bool isLoggedIn() const;
    QString userName() const;
    QString csrfToken() const;

    QString cookieString() const;
    QString ffmpegCookieString() const;

    CookieStore* cookieStore() const;
    QrLoginController* qrController() const;
    WebLoginController* webLoginController() const;
    NcmUserApi* userApi() const;
    NcmSongApi* songApi() const;
    NcmPlaylistApi* playlistApi() const;

    Q_INVOKABLE void load();   // 启动时加载持久化登录态
    Q_INVOKABLE void save();
    Q_INVOKABLE void logout();
    /// 校验登录 Cookie 是否仍有效（网络监测器周期调用；结果见 verifyResult 信号）
    Q_INVOKABLE void verifyLogin();

    // 供 QrLoginController 登录链路调用
    void setLoggedIn(bool v);
    void setUserName(const QString& n);
    void setCsrf(const QString& c);

signals:
    void loginStatusChanged();
    /// Cookie 校验结果转发（NcmUserApi）：1=有效, 0=失效, -1=网络异常
    void verifyResult(int valid);

private:
    explicit NcmSession(QObject* parent = nullptr);
    ~NcmSession() override;

    NcmSession(const NcmSession&) = delete;
    NcmSession& operator=(const NcmSession&) = delete;

    HttpClient* m_httpClient = nullptr;
    CurlHttpClient* m_curlClient = nullptr;
    CookieStore* m_cookieStore = nullptr;
    NcmAuthApi* m_authApi = nullptr;
    NcmUserApi* m_userApi = nullptr;
    NcmSongApi* m_songApi = nullptr;
    NcmPlaylistApi* m_playlistApi = nullptr;
    QrLoginController* m_qrController = nullptr;
    WebLoginController* m_webLoginController = nullptr;
};

#endif // NCMSESSION_H
