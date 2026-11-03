#include "WebLoginController.h"

#include "CookieStore.h"
#include "Oran7GlobalHelper.h"
#include "NcmSession.h"
#include "NcmUserApi.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTimer>
#include <QTcpServer>
#include <QUuid>
#include <QWebSocket>

WebLoginController::WebLoginController(NcmSession* session, NcmUserApi* userApi, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_userApi(userApi)
    , m_browser(new QProcess(this))
    , m_nam(new QNetworkAccessManager(this))
    , m_ws(new QWebSocket)
    , m_probeTimer(new QTimer(this))
    , m_cookieTimer(new QTimer(this))
    , m_verifyTimer(new QTimer(this))
    , m_probeTimeoutTimer(new QTimer(this))
{
    // 关键：本地 CDP 的 ws://127.0.0.1 必须直连。
    // QWebSocket 默认走 QNetworkProxy::applicationProxy，若系统挂了代理(Clash/V2Ray 等)
    // 会把 localhost 请求也转代理，导致连接一直挂起/失败。强制 NoProxy。
    m_ws->setProxy(QNetworkProxy::NoProxy);
    m_ws->setParent(this);
    // HTTP 探测(/json)同样强制直连，避免本地调试端口被代理劫持
    m_nam->setProxy(QNetworkProxy::NoProxy);
    m_probeTimer->setInterval(700);
    m_cookieTimer->setInterval(1500);
    m_verifyTimer->setInterval(5000);
    m_verifyTimer->setSingleShot(true);
    m_probeTimeoutTimer->setInterval(20000);
    m_probeTimeoutTimer->setSingleShot(true);
    connect(m_probeTimeoutTimer, &QTimer::timeout, this, [this]() {
        // 20 秒仍未连上 CDP：判定为无法连接浏览器调试通道
        if (!(m_ws && m_ws->state() == QAbstractSocket::ConnectedState)
            && (m_state == WebLoginState::StartingBrowser || m_state == WebLoginState::WaitingLogin)) {
            m_probeTimer->stop();
            setState(WebLoginState::Error);
            setStatus(QStringLiteral("无法连接浏览器调试通道，请确认浏览器已打开"));
            emit loginFailed(QStringLiteral("无法连接浏览器调试通道，请确认浏览器已打开"));
            cleanupBrowser();
        }
    });

    connect(m_browser, &QProcess::started, this, &WebLoginController::onBrowserStarted);
    connect(m_browser, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WebLoginController::onBrowserFinished);
    connect(m_browser, &QProcess::errorOccurred, this, &WebLoginController::onBrowserError);
    connect(m_probeTimer, &QTimer::timeout, this, &WebLoginController::probeTargets);
    connect(m_cookieTimer, &QTimer::timeout, this, &WebLoginController::pollCookies);
    connect(m_verifyTimer, &QTimer::timeout, this, [this]() {
        // 网络层拿到 MUSIC_U 即视为登录成功，account/get 拿不到昵称也不阻塞
        if (m_captured && !m_verified)
            finish(true, m_session->userName(), QString());
    });
    connect(m_ws, &QWebSocket::connected, this, &WebLoginController::onWsConnected);
    connect(m_ws, &QWebSocket::textMessageReceived, this, &WebLoginController::onWsTextMessage);
    connect(m_ws, &QWebSocket::disconnected, this, &WebLoginController::onWsDisconnected);
    connect(m_ws, &QWebSocket::errorOccurred, this, &WebLoginController::onWsError);
}

WebLoginController::WebLoginState WebLoginController::loginState() const { return m_state; }

QString WebLoginController::statusText() const { return m_statusText; }

void WebLoginController::setState(WebLoginState s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged();
    }
}

void WebLoginController::setStatus(const QString& t)
{
    if (m_statusText != t) {
        m_statusText = t;
        emit statusTextChanged();
    }
}

void WebLoginController::start()
{
    if (m_state == WebLoginState::StartingBrowser
        || m_state == WebLoginState::WaitingLogin
        || m_state == WebLoginState::Captured)
        return;

    cleanupBrowser();
    m_captured = false;
    m_verified = false;
    m_cookieCmdId = 0;
    m_wsUrl.clear();

    const QString browser = findBrowserExecutable();
    if (browser.isEmpty()) {
        setState(WebLoginState::Error);
        setStatus(QStringLiteral("未找到 Edge/Chrome 浏览器"));
        emit loginFailed(QStringLiteral("未找到 Edge/Chrome 浏览器"));
        return;
    }

    m_port = findFreePort();
    m_tempProfileDir = makeTempProfileDir();

    setState(WebLoginState::StartingBrowser);
    setStatus(QStringLiteral("正在启动浏览器登录..."));

    QStringList args;
    args << QStringLiteral("--remote-debugging-port=%1").arg(m_port)
         << QStringLiteral("--remote-allow-origins=*")
         << QStringLiteral("--user-data-dir=%1").arg(m_tempProfileDir)
         << QStringLiteral("--no-first-run")
         << QStringLiteral("--no-default-browser-check")
         << QStringLiteral("--disable-popup-blocking")
         << QStringLiteral("https://music.163.com/login");

    m_browser->setProcessChannelMode(QProcess::MergedChannels);
    m_browser->start(browser, args);

    NETWORK_LOG << "WebLoginController: launching browser port=" << m_port
                << " path=" << browser;
}

void WebLoginController::cancel()
{
    cleanupBrowser();
    setState(WebLoginState::Idle);
    setStatus(QStringLiteral(""));
}

void WebLoginController::onBrowserStarted()
{
    // 浏览器已启动，开始轮询 CDP /json 取调试地址
    m_probeTimer->start();
    m_probeTimeoutTimer->start();
}

void WebLoginController::onBrowserFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    NETWORK_LOG << "WebLoginController: browser exited status=" << status;
    // 注意：Edge 单例转发时，msedge.exe 启动器会立即退出(把参数转给已运行实例)，
    // 但浏览器窗口由现有实例打开、登录流程仍可继续。因此不在这里立即判定失败，
    // 而是继续探测 CDP；只有 m_probeTimeoutTimer 超时才报错。
    if (m_state == WebLoginState::WaitingLogin || m_state == WebLoginState::StartingBrowser) {
        if (!(m_ws && m_ws->state() == QAbstractSocket::ConnectedState))
            m_probeTimer->start();   // 浏览器进程退出，继续尝试连调试通道
    }
}

void WebLoginController::onBrowserError()
{
    if (m_state == WebLoginState::StartingBrowser) {
        m_probeTimer->stop();
        setState(WebLoginState::Error);
        setStatus(QStringLiteral("浏览器启动失败"));
        emit loginFailed(QStringLiteral("浏览器启动失败"));
    }
}

void WebLoginController::probeTargets()
{
    if (m_ws && m_ws->state() == QAbstractSocket::ConnectedState) {
        m_probeTimer->stop();
        return;
    }

    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:%1/json").arg(m_port)));
    req.setTransferTimeout(3000);
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onProbeReply(reply); });
}

void WebLoginController::onProbeReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray())
        return;

    const QJsonArray targets = doc.array();
    for (const QJsonValue& v : targets) {
        const QJsonObject t = v.toObject();
        if (t.value("type").toString() != QLatin1String("page"))
            continue;
        const QString url = t.value("url").toString();
        const QString wsUrl = t.value("webSocketDebuggerUrl").toString();
        if (!wsUrl.isEmpty()
            && (url.contains("music.163.com") || url.contains("163.com"))) {
            m_wsUrl = wsUrl;
            break;
        }
    }

    if (!m_wsUrl.isEmpty()) {
        m_probeTimer->stop();
        NETWORK_LOG << "WebLoginController: cdp target found, connecting...";
        m_ws->open(QUrl(m_wsUrl));
    }
}

void WebLoginController::onWsConnected()
{
    NETWORK_LOG << "WebLoginController: cdp websocket connected";
    m_probeTimer->stop();
    m_probeTimeoutTimer->stop();
    m_cookieCmdId = 0;
    m_captured = false;

    setState(WebLoginState::WaitingLogin);
    setStatus(QStringLiteral("请在打开的浏览器中登录网易云音乐\n可能会有延迟，扫码并授权成功后\n请耐心等待几秒（*＾-＾*）"));

    // 启用 Network 域，然后开始轮询 cookie
    sendCdp(++m_cmdId, QStringLiteral("Network.enable"), QJsonObject());
    m_cookieTimer->start();
    pollCookies(); // 立即轮询一次
}

void WebLoginController::onWsDisconnected()
{
    if (m_cleaningUp)
        return;
    NETWORK_LOG << "WebLoginController: cdp websocket disconnected";
    m_cookieTimer->stop();
    // 登录过程中页面 target 可能因导航/进程替换而断开：
    // 若尚未捕获凭据，重新探测并重连到新的 target
    if (m_captured) {
        // 已捕获凭据，交给 verifyAndFinish/verifyTimer 收尾，不再重连
    } else if (m_state == WebLoginState::WaitingLogin
               || m_state == WebLoginState::StartingBrowser) {
        m_wsUrl.clear();
        m_probeTimer->start();
        setStatus(QStringLiteral("浏览器已连接，等待登录..."));
    }
}

void WebLoginController::sendCdp(int id, const QString& method, const QJsonObject& params)
{
    if (!m_ws || m_ws->state() != QAbstractSocket::ConnectedState)
        return;
    QJsonObject msg;
    msg["id"] = id;
    msg["method"] = method;
    if (!params.isEmpty())
        msg["params"] = params;
    m_ws->sendTextMessage(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
}

void WebLoginController::pollCookies()
{
    if (!m_captured) {
        m_cookieCmdId = ++m_cmdId;
        sendCdp(m_cookieCmdId, QStringLiteral("Network.getAllCookies"), QJsonObject());
    }
}

void WebLoginController::onWsTextMessage(const QString& message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();
    const int id = obj["id"].toInt();
    if (id != m_cookieCmdId)
        return;
    m_cookieCmdId = 0;

    const QJsonValue result = obj["result"];
    if (result.isUndefined())
        return;
    const QJsonArray cookies = result.toObject()["cookies"].toArray();
    handleCookies(cookies);
}

void WebLoginController::handleCookies(const QJsonArray& cookies)
{
    if (m_captured)
        return;

    QString musicU;
    QString csrf;
    for (const QJsonValue& v : cookies) {
        const QJsonObject c = v.toObject();
        const QString name = c["name"].toString();
        if (name == QLatin1String("MUSIC_U") && musicU.isEmpty())
            musicU = c["value"].toString();
        else if (name == QLatin1String("__csrf") && csrf.isEmpty())
            csrf = c["value"].toString();
    }

    if (musicU.isEmpty())
        return; // 还没登录成功，继续等

    m_captured = true;
    m_cookieTimer->stop();
    setState(WebLoginState::Captured);
    setStatus(QStringLiteral("已获取登录凭据，正在验证..."));

    // 注入 CookieStore 并持久化
    m_session->cookieStore()->setAuthCookie(QStringLiteral("MUSIC_U"), musicU);
    m_session->cookieStore()->setAuthCookie(QStringLiteral("__csrf"), csrf);
    m_session->cookieStore()->save();

    // 关键：通知登录态变化，让 QML 的 NcmSession.isLoggedIn/userName 绑定刷新
    m_session->setLoggedIn(true);

    NETWORK_LOG << "WebLoginController: browser credentials captured";

    verifyAndFinish();
}

void WebLoginController::verifyAndFinish()
{
    // account/get 验证并取昵称（尽力而为），超时由 verifyTimer 兜底
    connect(m_userApi, &NcmUserApi::accountFetched, this,
            &WebLoginController::onAccountFetched, Qt::UniqueConnection);
    m_accountRequestId = NcmRequest::nextId();
    m_verifyTimer->start();
    m_userApi->fetchAccount(m_accountRequestId);
}

void WebLoginController::onAccountFetched(quint64 requestId, const QString& nickname)
{
    if (requestId != m_accountRequestId || m_state != WebLoginState::Captured || m_verified)
        return;
    m_verified = true;
    m_verifyTimer->stop();
    m_session->setUserName(nickname);
    m_session->save();
    finish(true, nickname, QString());
}

void WebLoginController::finish(bool success, const QString& nickname, const QString& msg)
{
    m_probeTimer->stop();
    m_cookieTimer->stop();
    m_verifyTimer->stop();

    if (success) {
        setState(WebLoginState::Success);
        setStatus(QStringLiteral("登录成功"));
        NETWORK_LOG << "WebLoginController: login success";
        emit loginSucceeded(nickname.isEmpty() ? QStringLiteral("网易云用户") : nickname);
    } else {
        setState(WebLoginState::Error);
        setStatus(msg);
        emit loginFailed(msg);
    }
    cleanupBrowser();
}

void WebLoginController::onWsError()
{
    if (m_cleaningUp)
        return;
    NETWORK_LOG << "WebLoginController: cdp websocket error, state=" << m_ws->state();
    m_cookieTimer->stop();

    // 连接失败时重试探测，而不是直接判死（可能只是首次握手抖动）
    if (m_state == WebLoginState::WaitingLogin || m_state == WebLoginState::StartingBrowser) {
        m_wsUrl.clear();
        m_probeTimer->start();
        setStatus(QStringLiteral("浏览器调试通道重连中..."));
    }
}

void WebLoginController::cleanupBrowser()
{
    m_cleaningUp = true;
    m_accountRequestId = 0;
    m_probeTimer->stop();
    m_cookieTimer->stop();
    m_verifyTimer->stop();
    m_probeTimeoutTimer->stop();
    m_cookieCmdId = 0;

    // 注意：不要 m_ws->disconnect(this)！构造函数里连的 connected/textMessageReceived/
    // errorOccurred 信号是终身连接，一旦断开后续就再也收不到回调。
    // 清理时只强制断开底层连接（abort），并用 m_cleaningUp 抑制断连/错误信号带来的副作用。
    if (m_ws) {
        m_ws->abort();
    }

    if (m_browser && m_browser->state() != QProcess::NotRunning) {
        m_browser->kill();
        m_browser->waitForFinished(3000);
    }

    if (!m_tempProfileDir.isEmpty()) {
        QDir d(m_tempProfileDir);
        if (d.exists())
            d.removeRecursively();
        m_tempProfileDir.clear();
    }
    m_cleaningUp = false;
}

QString WebLoginController::findBrowserExecutable()
{
    const QStringList candidates = {
        QStringLiteral("C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"),
        QStringLiteral("C:/Program Files/Microsoft/Edge/Application/msedge.exe"),
        QStringLiteral("C:/Program Files/Google/Chrome/Application/chrome.exe"),
        QStringLiteral("C:/Program Files (x86)/Google/Chrome/Application/chrome.exe"),
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return QString();
}

int WebLoginController::findFreePort()
{
    QTcpServer server;
    if (server.listen(QHostAddress::LocalHost, 0))
        return server.serverPort();
    return 9222; // 兜底
}

QString WebLoginController::makeTempProfileDir()
{
    const QString dir = QDir::tempPath()
        + QStringLiteral("/ncm_weblogin_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    QDir().mkpath(dir);
    return dir;
}
