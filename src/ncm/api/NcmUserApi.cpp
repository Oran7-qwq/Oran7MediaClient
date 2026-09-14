#include "NcmUserApi.h"

#include "CurlHttpClient.h"
#include "GlobalHelper.h"
#include "NcmProtocol.h"

#include <QJsonDocument>

#include <utility>

NcmUserApi::NcmUserApi(CurlHttpClient* curl, QObject* parent)
    : QObject(parent)
    , m_curl(curl)
{
    connect(m_curl, &CurlHttpClient::requestFinished, this,
            [this](int httpStatus, const QByteArray& body, const QString&, const QString& tag) {
                if (tag == QLatin1String("account"))
                    onAccountResult(httpStatus, body);
                else if (tag == QLatin1String("verifyLogin"))
                    onVerifyLoginResult(httpStatus, body);
            });
    connect(m_curl, &CurlHttpClient::requestError, this,
            [this](const QString& msg, const QString& tag) {
                if (tag == QLatin1String("account"))
                    emit apiError(msg);
                else if (tag == QLatin1String("verifyLogin"))
                    emit verifyResult(-1);   // 网络异常，不判定为失效
            });
}

void NcmUserApi::setCookieProvider(std::function<QString()> provider)
{
    m_cookieProvider = std::move(provider);
}

void NcmUserApi::setCsrfProvider(std::function<QString()> provider)
{
    m_csrfProvider = std::move(provider);
}

void NcmUserApi::fetchAccount()
{
    QJsonObject payload;
    if (m_csrfProvider) {
        const QString csrf = m_csrfProvider();
        if (!csrf.isEmpty())
            payload["csrf_token"] = csrf;
    }

    NcmProtocol protocol;
    NcmRequest req = protocol.prepareWeapi("/weapi/w/nuser/account/get", payload);

    // 关键：带上 MUSIC_U/__csrf Cookie（QNAM 通道被 WAF 拦空，改走 curl 并手动注入）
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }

    m_curl->send(req, QStringLiteral("account"));
}

void NcmUserApi::onAccountResult(int httpStatus, const QByteArray& body)
{
    NETWORK_LOG << "NcmUserApi: account reply http=" << httpStatus
                << " body=" << QString::fromUtf8(body).left(160);

    if (httpStatus != 200 || body.trimmed().isEmpty()) {
        emit apiError(QStringLiteral("获取用户信息失败"));
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    if (root["code"].toInt() != 200) {
        emit apiError(QStringLiteral("获取用户信息失败: code=%1").arg(root["code"].toInt()));
        return;
    }

    // profile 可能在顶层或 data 里
    QJsonObject profile = root["profile"].toObject();
    QString nickname = profile["nickname"].toString();
    if (nickname.isEmpty())
        nickname = root["data"].toObject()["profile"].toObject()["nickname"].toString();
    if (nickname.isEmpty())
        nickname = QStringLiteral("网易云用户");

    emit accountFetched(nickname);
}

void NcmUserApi::verifyLogin()
{
    QString csrf;
    if (m_csrfProvider)
        csrf = m_csrfProvider();

    // 与 NcmPlaylistApi 阶段1 相同：明文 account/get 必须带 Cookie，
    // 返回 profile:null 即 Cookie 已失效；网络异常则由 requestError 上报 -1。
    NcmRequest req;
    req.url = QUrl(QStringLiteral("https://music.163.com/api/nuser/account/get"));
    req.isPost = true;
    req.body = QStringLiteral("csrf_token=%1").arg(csrf).toUtf8();
    req.headers.insert(QByteArrayLiteral("Content-Type"),
                       QByteArrayLiteral("application/x-www-form-urlencoded"));
    req.headers.insert(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://music.163.com"));
    req.headers.insert(QByteArrayLiteral("User-Agent"),
                       QByteArrayLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                         "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120 Safari/537.36"));
    if (m_cookieProvider) {
        const QString cookie = m_cookieProvider();
        if (!cookie.isEmpty())
            req.headers.insert(QByteArrayLiteral("Cookie"), cookie.toUtf8());
    }

    m_curl->send(req, QStringLiteral("verifyLogin"));
}

void NcmUserApi::onVerifyLoginResult(int httpStatus, const QByteArray& body)
{
    if (httpStatus != 200 || body.trimmed().isEmpty()) {
        emit verifyResult(-1);   // 网络/服务器异常，不判定为失效
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    if (root["code"].toInt() != 200) {
        emit verifyResult(-1);
        return;
    }
    const QJsonObject profile = root["profile"].toObject();
    const qint64 uid = profile["userId"].toVariant().toLongLong();
    emit verifyResult(uid > 0 ? 1 : 0);
}
