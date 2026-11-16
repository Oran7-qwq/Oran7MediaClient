#include "Oran7QQMusicWebLogin.h"
#include "Oran7QQMusicAuth.h"
#include "Oran7Async.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QRegularExpression>

Oran7QQMusicWebLogin::Oran7QQMusicWebLogin(QObject* parent) : Oran7QQMusicWebLogin(Oran7Options{}, parent) {}
Oran7QQMusicWebLogin::Oran7QQMusicWebLogin(Oran7Options options, QObject* parent)
    : QObject(parent), m_options(std::move(options)), m_socket(QStringLiteral("http://127.0.0.1"))
{
    m_socket.setProxy(QNetworkProxy::NoProxy);
    m_socket.setMaxAllowedIncomingMessageSize(2 * 1024 * 1024);
    m_pollTimer.setInterval(m_options.pollIntervalMs);
    m_timeout.setSingleShot(true);
    m_cleanupTimer.setInterval(25);
    connect(&m_pollTimer, &QTimer::timeout, this, &Oran7QQMusicWebLogin::poll);
    connect(&m_timeout, &QTimer::timeout, this,
            [this]
            {
                fail(m_socket.state() == QAbstractSocket::ConnectedState
                         ? QStringLiteral("扫码登录已超时，请重新打开登录")
                         : QStringLiteral("无法连接登录浏览器，请重试"));
            });
    connect(&m_socket, &QWebSocket::connected, this,
            [this]
            {
                if (!m_active)
                {
                    m_socket.abort();
                    return;
                }
                m_timeout.start(m_options.loginTimeoutMs);
                m_promptClock.restart();
                emit statusChanged(QStringLiteral("正在打开官网扫码登录窗口…"));
                poll();
            });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &Oran7QQMusicWebLogin::receive);
    connect(&m_socket, &QWebSocket::disconnected, this,
            [this]
            {
                if (m_active)
                    fail(QStringLiteral("登录浏览器已关闭，请重新打开登录"));
            });
    connect(&m_socket, &QWebSocket::errorOccurred, this,
            [this]
            {
                if (m_active)
                    fail(QStringLiteral("登录浏览器连接失败，请重试"));
            });
    connect(&m_cleanupTimer, &QTimer::timeout, this, &Oran7QQMusicWebLogin::finishCleanup);
}
Oran7QQMusicWebLogin::~Oran7QQMusicWebLogin()
{
    shutdown();
}
QString Oran7QQMusicWebLogin::browserExecutable()
{
    const QStringList candidates{
        qEnvironmentVariable("ProgramFiles(x86)", "C:/Program Files (x86)") + "/Microsoft/Edge/Application/msedge.exe",
        qEnvironmentVariable("ProgramFiles", "C:/Program Files") + "/Microsoft/Edge/Application/msedge.exe",
        qEnvironmentVariable("ProgramFiles", "C:/Program Files") + "/Google/Chrome/Application/chrome.exe",
        qEnvironmentVariable("LOCALAPPDATA") + "/Google/Chrome/Application/chrome.exe"};
    for (const auto& path : candidates)
        if (QFileInfo::exists(path))
            return path;
    return {};
}
bool Oran7QQMusicWebLogin::start()
{
    if (m_closed || m_active)
        return false;
    if (m_finishing)
    {
        m_restartPending = true;
        emit statusChanged(QStringLiteral("正在准备新的登录窗口…"));
        return true;
    }
    const auto executable = m_options.executable.isEmpty() ? browserExecutable() : m_options.executable;
    if (executable.isEmpty())
    {
        emit failed(QStringLiteral("未找到 Edge/Chrome 浏览器"));
        return false;
    }
    m_profile = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/oran7-qq-login-XXXXXX");
    if (!m_profile->isValid())
    {
        m_profile.reset();
        emit failed(QStringLiteral("无法创建独立登录环境，请检查临时目录"));
        return false;
    }
    m_process = std::make_unique<QProcess>();
    m_process->setStandardOutputFile(QProcess::nullDevice());
    m_process->setStandardErrorFile(QProcess::nullDevice());
    connect(m_process.get(), &QProcess::errorOccurred, this,
            [this]
            {
                if (m_active)
                    fail(QStringLiteral("登录浏览器启动失败，请重试"));
            });
    // 某些 Chromium 启动器会先退出；实际窗口是否关闭以独立 CDP 连接为准。
    connect(m_process.get(), &QProcess::started, this,
            [this]
            {
                if (m_finishing)
                    m_process->kill();
            });
    m_active = true;
    m_lastCandidate.clear();
    m_cookieCommand = 0;
    m_targetsCommand = m_attachCommand = m_promptCommand = 0;
    m_pageSession.clear();
    m_promptDone = false;
    m_socket.abort();
    auto arguments = m_options.arguments;
    arguments << "--remote-debugging-address=127.0.0.1" << "--remote-debugging-port=0"
              << "--remote-allow-origins=http://127.0.0.1"
              << "--user-data-dir=" + m_profile->path() << "--no-first-run" << "--no-default-browser-check"
              << "--disable-background-mode"
              << "--new-window" << "https://y.qq.com/";
    m_process->start(executable, arguments);
    m_pollTimer.start();
    m_timeout.start(m_options.startupTimeoutMs);
    emit statusChanged(QStringLiteral("正在打开 QQ 音乐官方登录页…"));
    return true;
}
void Oran7QQMusicWebLogin::send(int id, const QString& method, const QJsonObject& parameters, const QString& session)
{
    QJsonObject message{{"id", id}, {"method", method}};
    if (!parameters.isEmpty())
        message["params"] = parameters;
    if (!session.isEmpty())
        message["sessionId"] = session;
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}
void Oran7QQMusicWebLogin::openLoginPrompt()
{
    if (m_promptDone)
        return;
    // 有界等待页面脚本就绪；不反复点击已经打开的弹窗，也不替用户提交授权确认。
    if (m_promptClock.elapsed() > 20000)
    {
        m_promptDone = true;
        emit statusChanged(QStringLiteral("官网登录入口尚未就绪，请在网页中点击登录"));
        return;
    }
    if (m_targetsCommand || m_attachCommand || m_promptCommand)
    {
        if (m_promptCommandClock.elapsed() < 5000)
            return;
        m_targetsCommand = m_attachCommand = m_promptCommand = 0;
        m_pageSession.clear();
    }
    m_promptCommandClock.restart();
    if (m_pageSession.isEmpty())
    {
        m_targetsCommand = ++m_command;
        send(m_targetsCommand, QStringLiteral("Target.getTargets"));
        return;
    }
    // 官方首页当前使用 .top_login__link；等待 React/原生点击处理器安装后再触发。
    // 仅在准确的官网 origin 执行，跨站导航不注入脚本，不读取页面密码/授权码。
    const auto expression = QStringLiteral(R"JS((() => {
        if (location.protocol !== 'https:' || location.hostname !== 'y.qq.com') return 'waiting';
        for (const frame of document.querySelectorAll('iframe')) {
            try {
                const url = new URL(frame.src, location.href);
                const visible = frame.getBoundingClientRect();
                if (visible.width && visible.height &&
                    (url.hostname.endsWith('.ptlogin2.qq.com') || url.hostname === 'open.weixin.qq.com'
                    || (url.hostname === 'graph.qq.com' && url.pathname === '/oauth2.0/authorize')
                    || (url.hostname === 'y.qq.com' && /login/i.test(url.pathname)))) return 'opened';
            } catch (_) {}
        }
        if (document.readyState !== 'complete') return 'waiting';
        const button = document.querySelector('.mod_top_login .top_login__link');
        if (!button || !button.getBoundingClientRect().width) return 'waiting';
        let ready = typeof button.onclick === 'function';
        for (let node = button, depth = 0; node && depth < 5; node = node.parentElement, ++depth) {
            for (const key of Object.keys(node)) {
                if (/^__react(?:Props|EventHandlers)\$/.test(key) && typeof node[key]?.onClick === 'function') ready = true;
            }
        }
        if (!ready) return 'waiting';
        button.click();
        return 'opened';
    })())JS");
    m_promptCommand = ++m_command;
    send(m_promptCommand, QStringLiteral("Runtime.evaluate"),
         QJsonObject{{"expression", expression}, {"returnByValue", true}}, m_pageSession);
}
void Oran7QQMusicWebLogin::poll()
{
    if (!m_active)
        return;
    if (m_socket.state() == QAbstractSocket::ConnectedState)
    {
        openLoginPrompt();
        if (m_cookieCommand)
        {
            if (m_commandClock.elapsed() > 5000)
                fail(QStringLiteral("登录浏览器响应超时，请重试"));
            return;
        }
        m_cookieCommand = ++m_command;
        m_commandClock.restart();
        send(m_cookieCommand, QStringLiteral("Storage.getCookies"));
        return;
    }
    if (m_socket.state() != QAbstractSocket::UnconnectedState || !m_profile)
        return;
    QFile portFile(m_profile->filePath("DevToolsActivePort"));
    if (!portFile.open(QIODevice::ReadOnly) || portFile.size() > 4096)
        return;
    const auto lines = portFile.readAll().split('\n');
    if (lines.size() < 2)
        return;
    bool valid = false;
    const auto port = lines[0].trimmed().toUInt(&valid);
    const QString path = QString::fromUtf8(lines[1].trimmed());
    static const QRegularExpression browserPath(QStringLiteral("^/devtools/browser/[a-zA-Z0-9_-]+$"));
    if (!valid || !port || port > 65535 || !browserPath.match(path).hasMatch())
        return;
    m_socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1%2").arg(port).arg(path)));
}
void Oran7QQMusicWebLogin::receive(const QString& message)
{
    if (!m_active)
        return;
    const auto object = QJsonDocument::fromJson(message.toUtf8()).object();
    const auto id = object.value("id").toInt(-1);
    if (m_targetsCommand && id == m_targetsCommand)
    {
        m_targetsCommand = 0;
        for (const auto& target : object.value("result").toObject().value("targetInfos").toArray())
        {
            const auto info = target.toObject();
            const QUrl url(info.value("url").toString());
            if (info.value("type") != "page" || url.scheme() != "https" || url.host() != "y.qq.com" ||
                info.value("targetId").toString().isEmpty())
                continue;
            m_attachCommand = ++m_command;
            m_promptCommandClock.restart();
            send(m_attachCommand, QStringLiteral("Target.attachToTarget"),
                 QJsonObject{{"targetId", info.value("targetId")}, {"flatten", true}});
            break;
        }
        return;
    }
    if (m_attachCommand && id == m_attachCommand)
    {
        m_attachCommand = 0;
        m_pageSession = object.value("result").toObject().value("sessionId").toString();
        return;
    }
    if (m_promptCommand && id == m_promptCommand)
    {
        m_promptCommand = 0;
        if (object.contains("error"))
        {
            m_pageSession.clear();
            return;
        }
        if (object.value("result").toObject().value("result").toObject().value("value") == "opened")
        {
            m_promptDone = true;
            emit statusChanged(QStringLiteral("请在官网登录窗口中使用 QQ 或微信扫码并确认"));
        }
        return;
    }
    if (!m_cookieCommand)
        return;
    if (object.value("id").toInt(-1) != m_cookieCommand)
        return;
    m_cookieCommand = 0;
    if (object.contains("error"))
    {
        fail(QStringLiteral("无法读取登录结果，请更新浏览器后重试"));
        return;
    }
    const auto auth =
        Oran7QQMusicAuth::fromBrowserCookies(object.value("result").toObject().value("cookies").toArray());
    if (!auth.isValid())
        return;
    const auto digest = QCryptographicHash::hash(QJsonDocument(auth.cookies).toJson(QJsonDocument::Compact),
                                                 QCryptographicHash::Sha256);
    if (digest == m_lastCandidate)
        return;
    m_lastCandidate = digest;
    emit cookiesFound(auth.cookies);
}
void Oran7QQMusicWebLogin::fail(const QString& message)
{
    if (!m_active)
        return;
    cancel();
    emit failed(message);
}
void Oran7QQMusicWebLogin::cancel()
{
    m_restartPending = false;
    m_active = false; // 先失效，断线信号不得把主动取消变成失败。
    m_pollTimer.stop();
    m_timeout.stop();
    m_cookieCommand = 0;
    m_lastCandidate.clear();
    m_targetsCommand = m_attachCommand = m_promptCommand = 0;
    m_pageSession.clear();
    if (!m_process && !m_profile)
        return;
    if (m_finishing)
        return;
    m_finishing = true;
    if (m_socket.state() == QAbstractSocket::ConnectedState)
    {
        send(++m_command, QStringLiteral("Browser.close"));
        m_socket.flush();
        m_socket.close();
    }
    m_cleanupClock.restart();
    m_cleanupTimer.start();
}
void Oran7QQMusicWebLogin::finishCleanup()
{
    if (!m_finishing)
        return;
    if (m_cleanupClock.elapsed() > 600 && m_process && m_process->state() != QProcess::NotRunning)
        m_process->kill();
    if ((m_process && m_process->state() != QProcess::NotRunning) ||
        (m_socket.state() != QAbstractSocket::UnconnectedState && m_cleanupClock.elapsed() < 1000))
        return;
    m_socket.abort();
    m_process.reset(); // 仅 finished 之后释放，普通取消不 wait，不阻塞 GUI。
    auto profile = std::move(m_profile);
    if (profile)
        Oran7Async::runDetached([profile = std::move(profile)]() mutable { profile.reset(); });
    m_finishing = false;
    m_cleanupTimer.stop();
    if (m_restartPending && !m_closed)
    {
        m_restartPending = false;
        start();
    }
}
void Oran7QQMusicWebLogin::shutdown()
{
    if (m_closed)
        return;
    m_closed = true;
    cancel();
    m_cleanupTimer.stop();
    m_socket.abort();
    // 只有最终退出允许等待；运行时取消/成功交接均由上面的定时器异步回收。
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    m_process.reset();
    auto profile = std::move(m_profile);
    if (profile)
        Oran7Async::runDetached([profile = std::move(profile)]() mutable { profile.reset(); });
    m_finishing = false;
}
