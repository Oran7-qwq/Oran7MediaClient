#pragma once
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebSocket>
#include <QElapsedTimer>
#include <memory>

// 官方网页负责二维码及 QQ/微信认证。只观察本次独立浏览器，不接管用户现有浏览器。
class Oran7QQMusicWebLogin : public QObject
{
    Q_OBJECT
  public:
    struct Oran7Options
    {
        QString executable;
        QStringList arguments;
        int startupTimeoutMs = 20000;
        int loginTimeoutMs = 180000;
        int pollIntervalMs = 700;
    };
    explicit Oran7QQMusicWebLogin(QObject* parent = nullptr);
    Oran7QQMusicWebLogin(Oran7Options options, QObject* parent = nullptr);
    ~Oran7QQMusicWebLogin() override;
    bool start();
    void cancel();
    void shutdown();
    bool busy() const { return m_active; }
  signals:
    void statusChanged(const QString& message);
    void cookiesFound(const QJsonArray& cookies);
    void failed(const QString& message);

  private:
    void poll();
    void receive(const QString& message);
    void fail(const QString& message);
    void finishCleanup();
    void send(int id, const QString& method, const QJsonObject& parameters = {}, const QString& session = {});
    void openLoginPrompt();
    static QString browserExecutable();
    Oran7Options m_options;
    std::unique_ptr<QTemporaryDir> m_profile;
    std::unique_ptr<QProcess> m_process;
    QWebSocket m_socket;
    QTimer m_pollTimer, m_timeout, m_cleanupTimer;
    QElapsedTimer m_commandClock, m_cleanupClock;
    QElapsedTimer m_promptClock, m_promptCommandClock;
    QByteArray m_lastCandidate;
    int m_command = 0, m_cookieCommand = 0;
    int m_targetsCommand = 0, m_attachCommand = 0, m_promptCommand = 0;
    QString m_pageSession;
    bool m_promptDone = false;
    bool m_active = false, m_finishing = false, m_closed = false, m_restartPending = false;
};
