#ifndef ORAN7NETWORKMONITOR_H
#define ORAN7NETWORKMONITOR_H

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

class QNetworkReply;

/**
 * @brief 全局网络监测器（单例）
 *
 * 每秒对目标站点做一次轻量请求，测出往返延迟（ms）并上报 online / latencyMs，
 * 供 UI 右上角实时显示网络状态。
 *
 * 当网络畅通（latency < 180ms）时，周期性调用 NcmSession::verifyLogin()
 * 校验网易云登录 Cookie 是否仍有效；若服务器返回 profile:null（Cookie 失效），
 * 会 emit cookieExpired()，由上层（main.qml）自动退出登录并引导重新登录。
 *
 * 使用：
 *   QML:  import Oran7Network 1.0
 *         Oran7NetworkMonitor.latencyMs / Oran7NetworkMonitor.online
 *         Oran7NetworkMonitor.cookieExpired
 */
class Oran7NetworkMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int latencyMs READ latencyMs NOTIFY latencyMsChanged FINAL)
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged FINAL)

public:
    static Oran7NetworkMonitor& instance();
    static void cleanup();

    int latencyMs() const { return m_latencyMs; }
    bool online() const { return m_online; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void latencyMsChanged(int ms);
    void onlineChanged(bool online);
    void cookieExpired();   // 网易云 Cookie 已失效 → 上层退出登录并引导重新登录

private:
    explicit Oran7NetworkMonitor(QObject* parent = nullptr);
    ~Oran7NetworkMonitor() override;

    void onTick();
    void pingOnce();
    void onPingFinished(QNetworkReply* reply);
    void setOnline(bool online);   // 统一维护 m_online，断网恢复时重置 Cookie 校验冷却
    void maybeVerifyCookie();

    QTimer m_timer;
    QNetworkAccessManager m_nam;
    QElapsedTimer m_elapsed;
    int m_latencyMs = -1;
    bool m_online = true;
    bool m_pingInFlight = false;
    qint64 m_lastCookieCheckMs = 0;      // 上次 Cookie 校验时间戳（防抖）
    bool m_cookieCheckInFlight = false;  // 校验请求进行中，避免重复触发

    Q_DISABLE_COPY(Oran7NetworkMonitor)
};

#endif // ORAN7NETWORKMONITOR_H
