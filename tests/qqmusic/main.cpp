#include "Oran7QQMusicSession.h"
#include "Oran7AppConfigManager.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJSEngine>
#include <QNetworkProxy>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QTest>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void check(bool ok, const char* label)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
    ++checks;
}
template <class F> static void until(F condition)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!condition() && elapsed.elapsed() < 6000)
        QTest::qWait(5);
    check(condition(), "asynchronous phase reached");
}
static QByteArray read(const QString& path)
{
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "read fixture");
    return file.readAll();
}
static void write(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "write fixture");
}
static QJsonArray cookies()
{
    return {QJsonObject{{"name", "uin"}, {"value", "o00123456"}, {"domain", ".qq.com"}},
            QJsonObject{{"name", "qm_keyst"}, {"value", "FICTIONAL_QQ_MUSIC_KEY"}, {"domain", ".y.qq.com"}},
            QJsonObject{{"name", "login_type"}, {"value", "1"}, {"domain", ".y.qq.com"}}};
}
// 独立的假浏览器进程，仅开本地 CDP WebSocket，不启动真实浏览器或读取账号。
static int fakeBrowser(QCoreApplication& app)
{
    QString profile, mode;
    for (const auto& argument : app.arguments())
    {
        if (argument.startsWith("--user-data-dir="))
            profile = argument.mid(16);
        if (argument.startsWith("--fake-browser="))
            mode = argument.mid(15);
    }
    if (profile.isEmpty())
        return 2;
    // 使用生产浏览器启动参数，避免离线替身掩盖错误的网页入口。
    if (!app.arguments().contains("https://y.qq.com/") || app.arguments().contains("https://y.qq.com/n/ryqq"))
        return 5;
    QWebSocketServer server("QQ fixture", QWebSocketServer::NonSecureMode);
    if (!server.listen(QHostAddress::LocalHost))
        return 3;
    QFile portFile(profile + "/DevToolsActivePort");
    if (!portFile.open(QIODevice::WriteOnly))
        return 4;
    portFile.write(QByteArray::number(server.serverPort()) + "\n/devtools/browser/fixture\n");
    portFile.close();
    QObject::connect(
        &server, &QWebSocketServer::newConnection, &app,
        [&]
        {
            auto* socket = server.nextPendingConnection();
            socket->setParent(&server);
            auto* javascript = new QJSEngine(socket);
            javascript->evaluate(R"JS(
                             var clicks = 0;
                             var location = {protocol:'https:',hostname:'y.qq.com',href:'https://y.qq.com/'};
                             var button = {onclick:null,parentElement:null,getBoundingClientRect:()=>({width:50,height:20}),
                                 click:()=>{clicks++},__reactProps$fixture:{onClick:()=>{}}};
                             var document = {readyState:'loading',querySelector: selector=>
                                 selector === '.mod_top_login .top_login__link' ? button : null,
                                 querySelectorAll:()=>[]};
                         )JS");
            auto opened = std::make_shared<bool>(false);
            auto evaluations = std::make_shared<int>(0);
            QObject::connect(
                socket, &QWebSocket::textMessageReceived, socket,
                [&, socket, mode, javascript, opened, evaluations](const QString& text)
                {
                    const auto request = QJsonDocument::fromJson(text.toUtf8()).object();
                    const auto respond = [socket, &request](const QJsonObject& result)
                    {
                        socket->sendTextMessage(QString::fromUtf8(
                            QJsonDocument(QJsonObject{{"id", request.value("id")}, {"result", result}})
                                .toJson(QJsonDocument::Compact)));
                    };
                    if (request.value("method") == "Browser.close")
                    {
                        socket->close();
                        QTimer::singleShot(0, &app, &QCoreApplication::quit);
                        return;
                    }
                    if (request.value("method") == "Target.getTargets")
                    {
                        respond(QJsonObject{
                            {"targetInfos", QJsonArray{QJsonObject{{"type", "page"},
                                                                   {"targetId", "foreign"},
                                                                   {"url", "https://y.qq.com.evil.invalid/"}},
                                                       QJsonObject{{"type", "page"},
                                                                   {"targetId", "official"},
                                                                   {"url", "https://y.qq.com/"}}}}});
                        return;
                    }
                    if (request.value("method") == "Target.attachToTarget")
                    {
                        if (request.value("params").toObject().value("targetId") != "official" ||
                            request.value("params").toObject().value("flatten") != QJsonValue(true))
                            app.exit(6);
                        respond(QJsonObject{{"sessionId", "official-session"}});
                        return;
                    }
                    if (request.value("method") == "Runtime.evaluate")
                    {
                        if (request.value("sessionId") != "official-session" || *opened)
                            app.exit(7);
                        if (++*evaluations >= 2)
                            javascript->evaluate("document.readyState='complete'");
                        const auto value =
                            javascript->evaluate(request.value("params").toObject().value("expression").toString());
                        if (value.isError())
                            app.exit(8);
                        *opened = value.toString() == "opened";
                        if (*opened && javascript->globalObject().property("clicks").toInt() != 1)
                            app.exit(9);
                        respond(QJsonObject{{"result", QJsonObject{{"type", "string"}, {"value", value.toString()}}}});
                        return;
                    }
                    if (request.value("method") != "Storage.getCookies" || mode == "silent")
                        return;
                    const auto result = mode == "waiting" || !*opened ? QJsonArray{} : cookies();
                    socket->sendTextMessage(
                        QString::fromUtf8(QJsonDocument(QJsonObject{{"id", request.value("id")},
                                                                    {"result", QJsonObject{{"cookies", result}}}})
                                              .toJson(QJsonDocument::Compact)));
                });
        });
    QTimer::singleShot(12000, &app, &QCoreApplication::quit);
    return app.exec();
}
class Oran7AuthServer : public QTcpServer
{
  public:
    int requests = 0;
    int delayMs = 0;
    bool rejected = false, malformed = false, redirect = false;
    QByteArray lastRequest;
    Oran7AuthServer()
    {
        check(listen(QHostAddress::LocalHost), "local verification endpoint");
        connect(
            this, &QTcpServer::newConnection, this,
            [this]
            {
                auto* socket = nextPendingConnection();
                socket->setParent(this);
                auto request = std::make_shared<QByteArray>();
                auto handled = std::make_shared<bool>(false);
                connect(
                    socket, &QTcpSocket::readyRead, socket,
                    [this, socket, request, handled]
                    {
                        request->append(socket->readAll());
                        if (*handled)
                            return;
                        const auto end = request->indexOf("\r\n\r\n");
                        if (end < 0)
                            return;
                        int length = 0;
                        for (auto line : request->left(end).split('\n'))
                        {
                            if (line.toLower().startsWith("content-length:"))
                                length = line.mid(15).trimmed().toInt();
                        }
                        if (request->size() < end + 4 + length)
                            return;
                        *handled = true;
                        ++requests;
                        lastRequest = *request;
                        QJsonObject block{{"code", rejected ? 1000 : 0}};
                        if (!rejected)
                            block["data"] =
                                QJsonObject{{"info", QJsonObject{{"uin", "123456"}, {"nick", "QQ fixture"}}}};
                        const auto response = malformed ? QByteArray("{bad-json")
                                                        : QJsonDocument(QJsonObject{{"code", 0}, {"req", block}})
                                                              .toJson(QJsonDocument::Compact);
                        const bool shouldRedirect = redirect;
                        QTimer::singleShot(
                            delayMs, socket,
                            [socket, response, shouldRedirect]
                            {
                                if (socket->state() != QAbstractSocket::ConnectedState)
                                    return;
                                socket->write(
                                    shouldRedirect
                                        ? QByteArray("HTTP/1.1 302 Found\r\nLocation: "
                                                     "https://example.invalid/\r\nContent-Length: 0\r\nConnection: "
                                                     "close\r\n\r\n")
                                        : QByteArray(
                                              "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                                              QByteArray::number(response.size()) +
                                              "\r\nSet-Cookie: SERVER_SHOULD_NOT_PERSIST=1\r\nConnection: "
                                              "close\r\n\r\n" +
                                              response);
                                socket->disconnectFromHost();
                            });
                    });
            });
    }
    QUrl endpoint() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/verify").arg(serverPort())); }
};
static Oran7QQMusicSession::Oran7Options options(Oran7AuthServer& server, const QString& mode = "valid")
{
    Oran7QQMusicSession::Oran7Options result;
    result.browser.executable = QCoreApplication::applicationFilePath();
    result.browser.arguments = {"--fake-browser=" + mode};
    result.browser.startupTimeoutMs = 5000;
    result.browser.loginTimeoutMs = 5000;
    result.browser.pollIntervalMs = 30;
    result.verificationEndpoint = server.endpoint();
    return result;
}
static void protocol()
{
    auto auth = Oran7QQMusicAuth::fromBrowserCookies(cookies(), 100);
    check(auth.isValid() && auth.userId == "123456", "music credential and normalized UIN");
    auto rootDomain = cookies();
    rootDomain[1] = QJsonObject{{"name", "qm_keyst"}, {"value", "ROOT_DOMAIN_KEY"}, {"domain", ".qq.com"}};
    check(Oran7QQMusicAuth::fromBrowserCookies(rootDomain).isValid(),
          "official shared QQ root-domain music ticket is accepted");
    auto polluted = cookies();
    polluted.append(
        QJsonObject{{"name", "qm_keyst"}, {"value", "FOREIGN_COOKIE"}, {"domain", "y.qq.com.evil.invalid"}});
    polluted.append(QJsonObject{{"name", "MUSIC_U"}, {"value", "NETEASE_COOKIE"}, {"domain", ".music.163.com"}});
    polluted.append(QJsonObject{{"name", "qqmusic_key"}, {"value", "BAD\r\nHEADER"}, {"domain", "y.qq.com"}});
    polluted.append(QJsonObject{{"name", "music_key"}, {"value", "EXPIRED"}, {"domain", "y.qq.com"}, {"expires", 99}});
    auth = Oran7QQMusicAuth::fromBrowserCookies(polluted, 100);
    check(auth.cookies.size() == 3 && !auth.cookieHeader().contains("FOREIGN") &&
              !auth.cookieHeader().contains("NETEASE"),
          "strict QQ music domain/field/header/expiry filtering");
    auto ordinary = cookies();
    ordinary.removeAt(1);
    ordinary.append(QJsonObject{{"name", "p_skey"}, {"value", "GENERAL_QQ_KEY"}, {"domain", ".qq.com"}});
    check(!Oran7QQMusicAuth::fromBrowserCookies(ordinary).isValid(),
          "ordinary QQ login is not QQ music authentication");
    auto wx = cookies();
    wx[2] = QJsonObject{{"name", "login_type"}, {"value", "2"}, {"domain", "y.qq.com"}};
    wx.append(QJsonObject{{"name", "wxuin"}, {"value", "987654"}, {"domain", "y.qq.com"}});
    check(Oran7QQMusicAuth::fromBrowserCookies(wx).userId == "987654", "WeChat uses its own music UIN");
    check(!Oran7QQMusicAuth::accepted(QJsonObject{{"code", 0}, {"req", QJsonObject{{"code", 1000}}}}) &&
              !Oran7QQMusicAuth::accepted(QJsonObject{{"code", 0}, {"req", QJsonObject{{"code", 0}}}}),
          "top-level success and incomplete responses cannot fake authenticated success");
}
static void lifecycle(const QJsonObject& defaults)
{
    QTemporaryDir dir;
    Oran7AuthServer server;
    const auto path = dir.filePath("config.json");
    Oran7AppConfigManager config(path, defaults);
    const auto netease = config.snapshot().auth.netease;
    Oran7QQMusicSession session(config, options(server));
    QSignalSpy success(&session, &Oran7QQMusicSession::loginSucceeded);
    session.startLogin();
    session.startLogin();
    until([&] { return session.loggedIn(); });
    check(!session.loginBusy() && success.count() == 1 && session.userName() == "QQ fixture",
          "official-browser handoff verifies before publishing login");
    check(server.requests == 1 && server.lastRequest.contains("GetLoginUserInfo") &&
              server.lastRequest.contains("FICTIONAL_QQ_MUSIC_KEY") && !server.lastRequest.contains("MUSIC_U"),
          "QQ-only credentials reach the authentication endpoint once");
    check(config.snapshot().auth.netease.musicU == netease.musicU &&
              config.snapshot().auth.netease.nickname == netease.nickname,
          "QQ login does not change NetEase authentication");
    const auto publicBytes = read(path);
    const auto refs =
        QJsonDocument::fromJson(publicBytes).object().value("credentialStorage").toObject().value("groups").toObject();
    check(!publicBytes.contains("FICTIONAL_QQ_MUSIC_KEY") && !publicBytes.contains("QQ fixture") &&
              refs.contains("qqmusic"),
          "public JSON contains only a separate QQ credential reference");
    const auto cipher = path + ".credentials/" + refs.value("qqmusic").toString() + ".dpapi";
    check(!read(cipher).contains("FICTIONAL_QQ_MUSIC_KEY"), "real DPAPI file contains no cookie plaintext");
    Oran7AppConfigManager reloaded(path, defaults);
    auto restoreOptions = options(server);
    restoreOptions.browser.executable = dir.filePath("not-a-browser.exe");
    Oran7QQMusicSession restored(reloaded, restoreOptions);
    restored.restore();
    restored.restore();
    until([&] { return restored.loggedIn(); });
    check(server.requests == 2 && restored.userName() == "QQ fixture",
          "restart restores encrypted account with one verification and no browser launch");
    Oran7AppConfigManager stale(path, defaults);
    restored.logout();
    check(!restored.loggedIn() && !restored.loginBusy() && reloaded.snapshot().auth.qqMusic.cookies.isEmpty(),
          "logout clears QQ account after committing empty credential generation");
    auto staleEdit = stale.snapshot();
    staleEdit.player.volume = 47;
    stale.updateConfig(staleEdit);
    check(!stale.saveConfig(), "stale configuration instance cannot restore a logged-out QQ account");
    Oran7AppConfigManager afterLogout(path, defaults);
    Oran7QQMusicSession empty(afterLogout, options(server));
    empty.restore();
    QTest::qWait(50);
    check(!empty.loggedIn() && !empty.loginBusy() && server.requests == 2,
          "logout survives restart without an authentication request");
    session.logout();
    check(session.loggedIn() && !session.statusText().isEmpty(),
          "logout commit conflict preserves the previous account and reports failure");
    session.shutdown();
    restored.shutdown();
    empty.shutdown();
}
static void cancellation(const QJsonObject& defaults)
{
    QTemporaryDir dir;
    Oran7AuthServer server;
    server.delayMs = 350;
    Oran7AppConfigManager config(dir.filePath("config.json"), defaults);
    Oran7QQMusicSession session(config, options(server));
    session.startLogin();
    until([&] { return server.requests == 1; });
    std::fprintf(stderr, "phase: cancel pending verification\n");
    QElapsedTimer duration;
    duration.start();
    session.cancelLogin();
    check(duration.elapsed() < 100 && !session.loginBusy(),
          "cancel does not wait for browser or network on the GUI thread");
    QTest::qWait(500);
    check(!session.loggedIn() && config.snapshot().auth.qqMusic.cookies.isEmpty(),
          "late verification cannot persist or publish a cancelled account");
    server.delayMs = 0;
    session.startLogin();
    until([&] { return session.loggedIn(); });
    check(server.requests == 2, "cancelled login can start a fresh browser and request generation");
    session.logout();
    session.shutdown();
}
static void immediateRestart(const QJsonObject& defaults)
{
    QTemporaryDir dir;
    Oran7AuthServer server;
    Oran7AppConfigManager config(dir.filePath("config.json"), defaults);
    Oran7QQMusicSession session(config, options(server));
    session.startLogin();
    session.cancelLogin();
    session.startLogin();
    session.cancelLogin();
    QTest::qWait(200);
    check(!session.loginBusy() && !session.loggedIn() && server.requests == 0,
          "cancelling a queued restart prevents browser and authentication revival");
    session.startLogin();
    until([&] { return session.loggedIn(); });
    check(server.requests == 1, "immediate restart serializes old browser cleanup and starts one fresh login");
    session.shutdown();
}
static void failures(const QJsonObject& defaults)
{
    for (const auto mode : {"reject", "malformed", "redirect", "timeout", "browser-timeout", "save"})
    {
        std::fprintf(stderr, "phase: failure %s\n", mode);
        QTemporaryDir dir;
        Oran7AuthServer server;
        server.rejected = QByteArray(mode) == "reject";
        server.malformed = QByteArray(mode) == "malformed";
        server.redirect = QByteArray(mode) == "redirect";
        server.delayMs = QByteArray(mode) == "timeout" ? 500 : 0;
        QString path = dir.filePath("config.json");
        if (QByteArray(mode) == "save")
        {
            write(dir.filePath("blocker"), "file");
            path = dir.filePath("blocker/config.json");
        }
        Oran7AppConfigManager config(path, defaults);
        auto opts = options(server, QByteArray(mode) == "browser-timeout" ? "waiting" : "valid");
        opts.browser.loginTimeoutMs = 300;
        opts.verificationTimeoutMs = QByteArray(mode) == "timeout" ? 100 : 1000;
        Oran7QQMusicSession session(config, opts);
        session.startLogin();
        until([&] { return !session.loginBusy(); });
        check(!session.loggedIn() && !session.statusText().isEmpty() &&
                  config.snapshot().auth.qqMusic.cookies.isEmpty(),
              "rejection/malformed/redirect/deadline/storage errors never publish or persist login");
        check(server.requests <= 1, "failure cannot cause repeated verification or credential-bearing redirects");
        session.shutdown();
    }
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    for (const auto& argument : app.arguments())
        if (argument.startsWith("--fake-browser="))
            return fakeBrowser(app);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    const auto defaults = QJsonDocument::fromJson(read(QStringLiteral(APP_ROOT) + "/config/default.json")).object();
    protocol();
    std::fprintf(stderr, "phase: lifecycle\n");
    lifecycle(defaults);
    std::fprintf(stderr, "phase: cancellation\n");
    cancellation(defaults);
    immediateRestart(defaults);
    failures(defaults);
    Oran7Async::waitForDone();
    std::printf("PASS: %d isolated QQ music login checks\n", checks);
}
