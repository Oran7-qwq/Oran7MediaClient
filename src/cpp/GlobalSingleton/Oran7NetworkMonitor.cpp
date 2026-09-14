#include "Oran7NetworkMonitor.h"

#include "NcmSession.h"

#include <QDateTime>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {
// 每 3 秒测一次（兼顾实时性与探测开销）
constexpr int kCheckIntervalMs = 3 * 1000;
// 低于该延迟视为网络畅通，才允许做 Cookie 校验（避免断网时误判 Cookie 失效）
constexpr int kLatencyGoodMs = 180;
// Cookie 校验冷却时间：避免频繁打网易云 account/get 触发 WAF/风控限流。
// 30s 连续轮询偏激进；Cookie 失效非分秒级事件，10 分钟一次 + 启动/重连各一次已足够及时，
// 请求量约为原方案 1/20（每小时 6 次），与正常客户端行为相当。
constexpr int kCookieCheckCooldownMs = 10 * 60 * 1000;
// 单次探测超时，断网时尽快判负
constexpr int kPingTimeoutMs = 5000;
// 探测目标：国内稳定可达的站点，用于测通用网络往返延迟
const QUrl kPingUrl(QStringLiteral("https://www.baidu.com/"));
} // namespace

static Oran7NetworkMonitor* s_instance = nullptr;

Oran7NetworkMonitor& Oran7NetworkMonitor::instance()
{
    if (!s_instance)
        s_instance = new Oran7NetworkMonitor();
    return *s_instance;
}

void Oran7NetworkMonitor::cleanup()
{
    delete s_instance;
    s_instance = nullptr;
}

Oran7NetworkMonitor::Oran7NetworkMonitor(QObject* parent)
    : QObject(parent)
{
    // 网易云 Cookie 校验结果（由 NcmSession 转发）：valid==0 表示失效 → 转成全局 cookieExpired
    connect(&NcmSession::instance(), &NcmSession::verifyResult, this,
            [this](int valid) {
                m_cookieCheckInFlight = false;
                if (valid == 0)
                    emit cookieExpired();
            });

    m_timer.setInterval(kCheckIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &Oran7NetworkMonitor::onTick);
}

Oran7NetworkMonitor::~Oran7NetworkMonitor() = default;

void Oran7NetworkMonitor::start()
{
    m_timer.start();
    onTick(); // 立即测一次，让"无网络"尽早上报，登录页可立刻显示无网络提示
}

void Oran7NetworkMonitor::stop()
{
    m_timer.stop();
}

void Oran7NetworkMonitor::onTick()
{
    pingOnce();
    maybeVerifyCookie();
}

void Oran7NetworkMonitor::pingOnce()
{
    if (m_pingInFlight)
        return;
    m_pingInFlight = true;

    QNetworkRequest request(kPingUrl);
    request.setTransferTimeout(kPingTimeoutMs);
    request.setRawHeader("User-Agent", "Oran7MediaClient/1.0");
    // HEAD：只等响应头返回，不下载完整 body。之前用 GET 会把百度首页 ~100KB
    // 全下完才算完成，导致延迟虚高（≈TCP+TLS握手+下载）。HEAD 后只剩握手+首字节。
    request.setRawHeader("Connection", "keep-alive"); // 尽力复用连接，避免每次重建握手

    m_elapsed.start();
    QNetworkReply* reply = m_nam.head(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onPingFinished(reply); });
}

void Oran7NetworkMonitor::onPingFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    m_pingInFlight = false;

    const bool ok = (reply->error() == QNetworkReply::NoError);
    const int newLatency = ok ? static_cast<int>(m_elapsed.isValid() ? m_elapsed.elapsed() : -1)
                              : -1;
    const bool newOnline = ok;

    if (newLatency != m_latencyMs) {
        m_latencyMs = newLatency;
        emit latencyMsChanged(m_latencyMs);
    }
    setOnline(newOnline);
}

void Oran7NetworkMonitor::setOnline(bool online)
{
    if (m_online == online)
        return;
    if (online && !m_online) {
        // 断网恢复：重置校验冷却，让下一个 tick 立即校验一次
        // （覆盖"断网启动→联网"场景，避免要等满 10 分钟才感知 Cookie 状态）
        m_lastCookieCheckMs = 0;
    }
    m_online = online;
    emit onlineChanged(m_online);
}

void Oran7NetworkMonitor::maybeVerifyCookie()
{
    if (m_cookieCheckInFlight)
        return;
    // 仅网络畅通且已登录时才校验；断网/高延迟不做，避免误判
    if (!m_online || m_latencyMs < 0 || m_latencyMs >= kLatencyGoodMs)
        return;
    if (!NcmSession::instance().isLoggedIn())
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastCookieCheckMs < kCookieCheckCooldownMs)
        return;

    m_lastCookieCheckMs = now;
    m_cookieCheckInFlight = true;
    NcmSession::instance().verifyLogin();
}
