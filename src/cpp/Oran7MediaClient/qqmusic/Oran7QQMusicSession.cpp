#include "Oran7QQMusicSession.h"
#include "Oran7AppConfigManager.h"
#include "Oran7GlobalHelper.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <utility>

Oran7QQMusicSession::Oran7QQMusicSession(Oran7AppConfigManager& config, QObject* parent)
    : Oran7QQMusicSession(config, Oran7Options{}, parent)
{
}
Oran7QQMusicSession::Oran7QQMusicSession(Oran7AppConfigManager& config, Oran7Options options, QObject* parent)
    : QObject(parent), m_config(config), m_options(std::move(options)), m_browser(m_options.browser)
{
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this,
            [this]
            {
                if (!m_closed && m_verifying)
                    fail(QStringLiteral("QQ 音乐登录验证超时，请检查网络后重试"));
            });
    connect(&m_browser, &Oran7QQMusicWebLogin::statusChanged, this,
            [this](const QString& message)
            {
                if (m_closed || !m_loginActive || m_verifying)
                    return;
                m_status = message;
                emit statusChanged();
            });
    connect(&m_browser, &Oran7QQMusicWebLogin::cookiesFound, this,
            [this](const QJsonArray& cookies)
            {
                if (m_closed || !m_loginActive)
                    return;
                const auto auth = Oran7QQMusicAuth::fromBrowserCookies(cookies);
                if (auth.isValid())
                    verify(auth, false);
            });
    connect(&m_browser, &Oran7QQMusicWebLogin::failed, this,
            [this](const QString& message)
            {
                if (!m_closed && m_loginActive)
                    fail(message);
            });
}
Oran7QQMusicSession::~Oran7QQMusicSession()
{
    shutdown();
}
void Oran7QQMusicSession::abortVerification()
{
    ++m_generation; // abort 可同步触发 finished，必须先作废。
    m_deadline.stop();
    m_verifying = false;
    const auto reply = m_reply;
    m_reply.clear();
    if (reply)
        reply->abort();
}
void Oran7QQMusicSession::restore()
{
    if (m_closed || m_restored)
        return;
    m_restored = true;
    if (loginBusy() || m_loggedIn)
        return;
    if (!m_config.credentialError("qqmusic").isEmpty())
    {
        m_status = QStringLiteral("QQ 音乐凭据无法恢复，请重新扫码登录");
        emit statusChanged();
        return;
    }
    const auto saved = m_config.snapshot().auth.qqMusic;
    if (saved.cookies.isEmpty())
        return;
    auto auth = Oran7QQMusicAuth::fromBrowserCookies(QJsonArray::fromVariantList(saved.cookies));
    if (!auth.isValid() || auth.userId != saved.userId)
    {
        m_status = QStringLiteral("QQ 音乐登录已过期，请重新扫码登录");
        emit statusChanged();
        return;
    }
    auth.nickname = saved.nickname;
    verify(std::move(auth), true);
}
void Oran7QQMusicSession::startLogin()
{
    if (m_closed || loginBusy() || m_loggedIn)
        return;
    m_loginActive = true;
    m_status.clear();
    emit statusChanged();
    m_browser.start(); // 启动失败也经 failed 收口。
}
void Oran7QQMusicSession::cancelLogin()
{
    if (m_closed)
        return;
    abortVerification();
    m_loginActive = false;
    m_browser.cancel();
    m_status.clear();
    emit statusChanged();
}
void Oran7QQMusicSession::fail(const QString& message)
{
    abortVerification();
    m_loginActive = false;
    m_browser.cancel();
    m_status = message;
    emit statusChanged();
}
bool Oran7QQMusicSession::persist(const Oran7QQMusicAuth& auth)
{
    const auto before = m_config.snapshot().auth.qqMusic;
    auto next = m_config.snapshot();
    next.auth.qqMusic = {auth.cookies.toVariantList(), auth.userId, auth.nickname};
    m_config.updateConfig(next);
    if (m_config.saveConfig())
        return true;
    auto rollback = m_config.snapshot();
    rollback.auth.qqMusic = before;
    m_config.updateConfig(rollback);
    return false;
}
void Oran7QQMusicSession::verify(Oran7QQMusicAuth auth, bool restoring)
{
    abortVerification();
    const auto generation = m_generation;
    m_verifying = true;
    m_status = restoring ? QStringLiteral("正在恢复 QQ 音乐登录…") : QStringLiteral("已扫码，正在验证 QQ 音乐账号…");
    emit statusChanged();
    if (generation != m_generation || m_closed)
        return;
    QNetworkRequest request(m_options.verificationEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Referer", "https://y.qq.com/");
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/130.0.0.0 Safari/537.36");
    request.setRawHeader("Cookie", auth.cookieHeader());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(m_options.verificationTimeoutMs);
    auto* reply = m_network.post(request, QJsonDocument(auth.verificationRequest()).toJson(QJsonDocument::Compact));
    NETWORK_LOG << "QQMusic verify request=" << generation << " restoring=" << restoring;
    m_reply = reply;
    reply->setReadBufferSize(512 * 1024);
    auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this,
            [reply, bytes]
            {
                bytes->append(reply->readAll());
                if (bytes->size() > 512 * 1024)
                    reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, bytes, auth = std::move(auth), generation, restoring]() mutable
            {
                reply->deleteLater();
                if (m_closed || generation != m_generation || m_reply != reply)
                    return;
                m_reply.clear();
                m_deadline.stop();
                m_verifying = false;
                bytes->append(reply->readAll());
                if (reply->error() != QNetworkReply::NoError ||
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
                    bytes->size() > 512 * 1024)
                {
                    fail(QStringLiteral("QQ 音乐登录验证失败，请检查网络后重试"));
                    return;
                }
                QJsonParseError parse;
                const auto document = QJsonDocument::fromJson(*bytes, &parse);
                bytes->fill('\0');
                NETWORK_LOG << "QQMusic verify request=" << generation
                            << " http=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                            << " code=" << document.object().value("req").toObject().value("code").toInt(-1);
                if (parse.error != QJsonParseError::NoError || !document.isObject())
                {
                    fail(QStringLiteral("QQ 音乐登录响应异常，请稍后重试"));
                    return;
                }
                if (!Oran7QQMusicAuth::accepted(document.object()))
                {
                    if (restoring)
                        fail(QStringLiteral("QQ 音乐登录已过期，请重新扫码登录"));
                    else
                    {
                        // 网页可能先写入中间票据；继续等音乐票据变化，不把扫码前状态当成功。
                        m_status = QStringLiteral("尚未通过登录验证，请完成网页中的扫码确认");
                        emit statusChanged();
                    }
                    return;
                }
                const auto nickname = Oran7QQMusicAuth::responseNickname(document.object());
                if (!nickname.isEmpty())
                    auth.nickname = nickname;
                if (auth.nickname.isEmpty())
                    auth.nickname = QStringLiteral("QQ 音乐用户");
                if (!restoring && !persist(auth))
                {
                    fail(QStringLiteral("QQ 音乐登录保存失败，请检查配置文件权限后重试"));
                    return;
                }
                if (m_closed || generation != m_generation)
                    return;
                m_auth = std::move(auth);
                m_loggedIn = true;
                m_loginActive = false;
                m_browser.cancel();
                m_status = QStringLiteral("QQ 音乐登录成功");
                emit sessionChanged();
                emit statusChanged();
                if (!m_closed && generation == m_generation && m_loggedIn)
                    emit loginSucceeded();
            });
    m_deadline.start(m_options.verificationTimeoutMs);
}
void Oran7QQMusicSession::logout()
{
    if (m_closed)
        return;
    cancelLogin();
    const auto previous = m_config.snapshot().auth.qqMusic;
    if (!m_config.clearCredentialGroup("qqmusic"))
    {
        auto rollback = m_config.snapshot();
        rollback.auth.qqMusic = previous;
        m_config.updateConfig(rollback);
        m_status = QStringLiteral("QQ 音乐退出登录保存失败，请检查配置文件权限");
        emit statusChanged();
        return;
    }
    m_auth = {};
    m_loggedIn = false;
    emit sessionChanged();
}
void Oran7QQMusicSession::shutdown()
{
    if (m_closed)
        return;
    m_closed = true;
    abortVerification();
    m_loginActive = false;
    m_browser.shutdown();
    m_auth = {};
    m_loggedIn = false;
}
