#pragma once
#include "Oran7QQMusicAuth.h"
#include "Oran7QQMusicWebLogin.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkReply;
class Oran7AppConfigManager;

// Client 独占的 QQ 音乐账号服务；网络/登录任务均在 GUI 事件循环异步执行。
class Oran7QQMusicSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY sessionChanged)
    Q_PROPERTY(bool loginBusy READ loginBusy NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
  public:
    struct Oran7Options
    {
        Oran7QQMusicWebLogin::Oran7Options browser;
        QUrl verificationEndpoint{QStringLiteral("https://u.y.qq.com/cgi-bin/musicu.fcg")};
        int verificationTimeoutMs = 10000;
    };
    explicit Oran7QQMusicSession(Oran7AppConfigManager& config, QObject* parent = nullptr);
    Oran7QQMusicSession(Oran7AppConfigManager& config, Oran7Options options, QObject* parent = nullptr);
    ~Oran7QQMusicSession() override;
    bool loggedIn() const { return m_loggedIn; }
    QString userName() const { return m_auth.nickname; }
    bool loginBusy() const { return m_loginActive || m_verifying; }
    QString statusText() const { return m_status; }
    // C++ 网络服务按值借用，不把 Cookie 暴露到 QML 或日志。
    Oran7QQMusicAuth credentialSnapshot() const { return m_auth; }
    void restore();
    void shutdown();
    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void logout();
  signals:
    void sessionChanged();
    void statusChanged();
    void loginSucceeded();

  private:
    void verify(Oran7QQMusicAuth auth, bool restoring);
    void abortVerification();
    void fail(const QString& message);
    bool persist(const Oran7QQMusicAuth& auth);
    Oran7AppConfigManager& m_config;
    Oran7Options m_options;
    QNetworkAccessManager m_network;
    Oran7QQMusicWebLogin m_browser;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    Oran7QQMusicAuth m_auth;
    QString m_status;
    quint64 m_generation = 0;
    bool m_loggedIn = false, m_loginActive = false, m_verifying = false, m_restored = false, m_closed = false;
};
