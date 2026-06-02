#include "BilibiliAuthManager.h"
#include "GlobalHelper.h"
#include "AppJsonConfigManager.h"

#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

static BilibiliAuthManager* s_instance = nullptr;

BilibiliAuthManager& BilibiliAuthManager::instance()
{
    if (!s_instance)
        s_instance = new BilibiliAuthManager();
    return *s_instance;
}

void BilibiliAuthManager::cleanup()
{
    if (s_instance) {
        // 注意：不能在这里调用 cancelLogin() 或 m_pollTimer->stop()
        // 因为 cleanup 在 QGuiApplication 析构期间被调用，此时事件循环已关闭，
        // QTimer::stop() 会卡死。直接 delete 即可，子对象（QTimer等）会自动销毁。
        delete s_instance;
        s_instance = nullptr;
        INFO_LOG << "[DTOR] BilibiliAuthManager cleanup finished.";
    }
}

BilibiliAuthManager::BilibiliAuthManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(3000); // 3秒轮询一次
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &BilibiliAuthManager::pollLoginStatus);
}

BilibiliAuthManager::~BilibiliAuthManager()
{
    // 不要调用 cancelLogin()！
    // 在 qAddPostRoutine 析构时 Qt 事件循环已关闭，QTimer::stop() 会卡死。
    // m_pollTimer 和 m_networkManager 是子对象，会自动销毁。
}

// ==================== 公共方法 ====================

bool BilibiliAuthManager::isLoggedIn() const
{
    return m_isLoggedIn;
}

const QString& BilibiliAuthManager::userName() const
{
    return m_userName;
}

const QString& BilibiliAuthManager::qrCodeUrl() const
{
    return m_qrCodeUrl;
}

BilibiliAuthManager::LoginState BilibiliAuthManager::loginState() const
{
    return m_loginState;
}

QString BilibiliAuthManager::cookieString() const
{
    if (!m_isLoggedIn || m_sessdata.isEmpty())
        return {};

    return QString("SESSDATA=%1; DedeUserID=%2; bili_jct=%3")
        .arg(m_sessdata, m_dedeUserId, m_biliJct);
}

QString BilibiliAuthManager::ffmpegCookieString() const
{
    // FFmpeg 的 cookies 选项格式与HTTP相同：name=value; name=value
    return cookieString();
}

// ==================== 登录流程 ====================

void BilibiliAuthManager::startLogin()
{
    if (m_loginState == LoginState::WaitingScan ||
        m_loginState == LoginState::Scanned ||
        m_loginState == LoginState::Generating)
    {
        NETWORK_LOG << "BilibiliAuth: Login already in progress";
        return;
    }

    setLoginState(LoginState::Generating);

    // 请求QR码
    QUrl url("https://passport.bilibili.com/x/passport-login/web/qrcode/generate");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onQrCodeReply(reply);
    });
}

void BilibiliAuthManager::cancelLogin()
{
    if (m_pollTimer && m_pollTimer->isActive())
        m_pollTimer->stop();
    if (m_loginState != LoginState::Success && m_loginState != LoginState::Idle)
    {
        setLoginState(LoginState::Idle);
    }
}

void BilibiliAuthManager::logout()
{
    cancelLogin();

    m_sessdata.clear();
    m_dedeUserId.clear();
    m_biliJct.clear();
    m_userName.clear();
    m_isLoggedIn = false;

    // 从配置中清除
    AppConfigManager::ins().setValueQVariant("bilibili_auth.sessdata", "");
    AppConfigManager::ins().setValueQVariant("bilibili_auth.dede_user_id", "");
    AppConfigManager::ins().setValueQVariant("bilibili_auth.bili_jct", "");
    AppConfigManager::ins().setValueQVariant("bilibili_auth.user_name", "");
    AppConfigManager::ins().saveConfig();

    setLoginState(LoginState::Idle);
    emit loginStatusChanged();

    NETWORK_LOG << "BilibiliAuth: Logged out, cookies cleared";
}

// ==================== 持久化 ====================

void BilibiliAuthManager::loadCookies()
{
    m_sessdata = AppConfigManager::ins().getValueQVariant("bilibili_auth.sessdata").toString();
    m_dedeUserId = AppConfigManager::ins().getValueQVariant("bilibili_auth.dede_user_id").toString();
    m_biliJct = AppConfigManager::ins().getValueQVariant("bilibili_auth.bili_jct").toString();
    m_userName = AppConfigManager::ins().getValueQVariant("bilibili_auth.user_name").toString();

    if (!m_sessdata.isEmpty())
    {
        NETWORK_LOG << "BilibiliAuth: Found stored cookies, validating...";
        validateStoredCookies();
    }
    else
    {
        NETWORK_LOG << "BilibiliAuth: No stored cookies found";
    }
}

void BilibiliAuthManager::saveCookies()
{
    if (m_sessdata.isEmpty())
        return;

    AppConfigManager::ins().setValueQVariant("bilibili_auth.sessdata", m_sessdata);
    AppConfigManager::ins().setValueQVariant("bilibili_auth.dede_user_id", m_dedeUserId);
    AppConfigManager::ins().setValueQVariant("bilibili_auth.bili_jct", m_biliJct);
    AppConfigManager::ins().setValueQVariant("bilibili_auth.user_name", m_userName);

    NETWORK_LOG << "BilibiliAuth: Cookies saved to config";
}

// ==================== 内部方法 ====================

void BilibiliAuthManager::setLoginState(LoginState state)
{
    if (m_loginState == state)
        return;

    m_loginState = state;
    emit loginStateChanged();
}

void BilibiliAuthManager::onQrCodeReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
    {
        NETWORK_LOG << "BilibiliAuth: QR code request failed:" << reply->errorString().toStdString();
        setLoginState(LoginState::Error);
        emit loginError(reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        NETWORK_LOG << "BilibiliAuth: QR code response parse error";
        setLoginState(LoginState::Error);
        emit loginError("JSON解析失败");
        return;
    }

    QJsonObject root = doc.object();
    int code = root["code"].toInt();
    if (code != 0)
    {
        NETWORK_LOG << "BilibiliAuth: QR code API error, code=" << code;
        setLoginState(LoginState::Error);
        emit loginError(QString("API错误: %1").arg(root["message"].toString()));
        return;
    }

    QJsonObject dataObj = root["data"].toObject();
    m_qrcodeKey = dataObj["qrcode_key"].toString();
    QString qrContentUrl = dataObj["url"].toString();

    if (m_qrcodeKey.isEmpty() || qrContentUrl.isEmpty())
    {
        NETWORK_LOG << "BilibiliAuth: QR code key or URL is empty";
        setLoginState(LoginState::Error);
        emit loginError("QR码数据不完整");
        return;
    }

    // 重要：不能直接把 qrContentUrl 赋值给 m_qrCodeUrl 让QML Image加载！
    // 因为加载这个URL会被B站服务器视为"扫码"，导致poll立刻返回登录成功。
    // 必须将URL转成真正的QR码图片，使用在线QR码生成API。
    m_qrCodeUrl = "https://api.qrserver.com/v1/create-qr-code/?size=200x200&data="
                  + QUrl::toPercentEncoding(qrContentUrl);

    NETWORK_LOG << "BilibiliAuth: QR code generated, starting poll...";
    setLoginState(LoginState::WaitingScan);
    emit qrCodeGenerated();

    // 开始轮询
    m_pollTimer->start();
}

void BilibiliAuthManager::pollLoginStatus()
{
    if (m_qrcodeKey.isEmpty())
    {
        m_pollTimer->stop();
        return;
    }

    QUrl url("https://passport.bilibili.com/x/passport-login/web/qrcode/poll");
    QUrlQuery query;
    query.addQueryItem("qrcode_key", m_qrcodeKey);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onPollReply(reply);
    });
}

void BilibiliAuthManager::onPollReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
    {
        NETWORK_LOG << "BilibiliAuth: Poll request failed:" << reply->errorString().toStdString();
        // 不立即设为Error，网络抖动可以重试
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
        return;

    QJsonObject root = doc.object();

    // 注意：顶层 code 是API调用状态（永远为0），真正的扫码状态在 data.code 里
    QJsonObject dataObj = root["data"].toObject();
    int code = dataObj["code"].toInt();

    switch (code)
    {
    case 0:
    {
        // 登录成功，提取Cookie
        m_pollTimer->stop();

        // 方法1：从 data.url 中提取Cookie参数
        QString redirectUrl = dataObj["url"].toString();
        if (!redirectUrl.isEmpty())
        {
            extractCookiesFromUrl(redirectUrl);
        }

        // 方法2：从 Set-Cookie 响应头中提取（作为补充）
        const auto& headers = reply->rawHeaderPairs();
        for (const auto& header : headers)
        {
            QString headerName = QString::fromUtf8(header.first).toLower();
            if (headerName == "set-cookie")
            {
                QString cookieValue = QString::fromUtf8(header.second);
                if (cookieValue.startsWith("SESSDATA=") && m_sessdata.isEmpty())
                {
                    int end = cookieValue.indexOf(';');
                    m_sessdata = cookieValue.mid(9, end > 9 ? end - 9 : -1);
                }
                else if (cookieValue.startsWith("DedeUserID=") && m_dedeUserId.isEmpty())
                {
                    int end = cookieValue.indexOf(';');
                    m_dedeUserId = cookieValue.mid(11, end > 11 ? end - 11 : -1);
                }
                else if (cookieValue.startsWith("bili_jct=") && m_biliJct.isEmpty())
                {
                    int end = cookieValue.indexOf(';');
                    m_biliJct = cookieValue.mid(9, end > 9 ? end - 9 : -1);
                }
            }
        }

        m_isLoggedIn = true;
        setLoginState(LoginState::Success);
        emit loginStatusChanged();

        // 保存Cookie
        saveCookies();

        NETWORK_LOG << "BilibiliAuth: Login successful!";
        break;
    }

    case 86101:
        // 未扫码，继续等待
        if (m_loginState != LoginState::WaitingScan)
            setLoginState(LoginState::WaitingScan);
        break;

    case 86090:
        // 已扫码，等待确认
        if (m_loginState != LoginState::Scanned)
        {
            setLoginState(LoginState::Scanned);
            NETWORK_LOG << "BilibiliAuth: QR code scanned, waiting for confirmation...";
        }
        break;

    case 86038:
        // 二维码已过期
        m_pollTimer->stop();
        setLoginState(LoginState::Expired);
        NETWORK_LOG << "BilibiliAuth: QR code expired";
        break;

    default:
        NETWORK_LOG << "BilibiliAuth: Unknown poll response code:" << code;
        break;
    }
}

void BilibiliAuthManager::extractCookiesFromUrl(const QString& url)
{
    // redirectUrl格式: https://passport.bilibili.com/login?SESSDATA=xxx&DedeUserID=xxx&bili_jct=xxx
    int queryStart = url.indexOf('?');
    if (queryStart < 0)
        return;

    QString queryString = url.mid(queryStart + 1);
    QStringList params = queryString.split('&');

    for (const QString& param : std::as_const(params))
    {
        int eqPos = param.indexOf('=');
        if (eqPos < 0)
            continue;

        QString key = param.left(eqPos);
        QString value = param.mid(eqPos + 1);

        if (key == "SESSDATA")
            m_sessdata = value;
        else if (key == "DedeUserID")
            m_dedeUserId = value;
        else if (key == "bili_jct")
            m_biliJct = value;
    }
}

void BilibiliAuthManager::validateStoredCookies()
{
    if (m_sessdata.isEmpty())
        return;

    QUrl url("https://api.bilibili.com/x/web-interface/nav");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    request.setRawHeader("Cookie", cookieString().toUtf8());
    request.setRawHeader("Referer", "https://www.bilibili.com");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onValidateReply(reply);
    });
}

void BilibiliAuthManager::onValidateReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
    {
        NETWORK_LOG << "BilibiliAuth: Cookie validation request failed:" << reply->errorString().toStdString();
        m_isLoggedIn = false;
        emit loginStatusChanged();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        m_isLoggedIn = false;
        emit loginStatusChanged();
        return;
    }

    QJsonObject root = doc.object();
    int code = root["code"].toInt();

    if (code == 0)
    {
        // Cookie有效
        m_isLoggedIn = true;

        // 提取用户名
        QJsonObject dataObj = root["data"].toObject();
        m_userName = dataObj["uname"].toString();

        setLoginState(LoginState::Success);
        emit loginStatusChanged();

        NETWORK_LOG << "BilibiliAuth: Stored cookies valid, user:" << m_userName.toStdString();
    }
    else
    {
        // Cookie已过期
        m_isLoggedIn = false;
        m_sessdata.clear();
        m_dedeUserId.clear();
        m_biliJct.clear();
        m_userName.clear();

        emit loginStatusChanged();

        NETWORK_LOG << "BilibiliAuth: Stored cookies expired, need re-login";
    }
}
