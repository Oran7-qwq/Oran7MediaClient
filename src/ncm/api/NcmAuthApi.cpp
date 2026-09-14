#include "NcmAuthApi.h"

#include "CurlHttpClient.h"
#include "GlobalHelper.h"
#include "NcmProtocol.h"

#include <QDateTime>
#include <QJsonDocument>

// 网易云偶发在 JSON 外层包二进制帧（如 !\x90\x00\x04{...}\x03），
// 从首个 '{' 到最后一个 '}' 截取 JSON 再解析，兼容两种形态
static QJsonObject extractJsonObject(const QByteArray& raw)
{
    const int first = raw.indexOf('{');
    const int last = raw.lastIndexOf('}');
    if (first >= 0 && last > first)
        return QJsonDocument::fromJson(raw.mid(first, last - first + 1)).object();
    return QJsonDocument::fromJson(raw).object();
}

NcmAuthApi::NcmAuthApi(CurlHttpClient* curl, QObject* parent)
    : QObject(parent)
    , m_curl(curl)
{
    connect(m_curl, &CurlHttpClient::requestFinished, this,
            [this](int httpStatus, const QByteArray& body, const QString& setCookie,
                   const QString& tag) {
                if (tag == QLatin1String("unikey"))
                    onUnikeyResult(httpStatus, body);
                else if (tag == QLatin1String("check"))
                    onCheckResult(httpStatus, body, setCookie);
            });
    connect(m_curl, &CurlHttpClient::requestError, this,
            [this](const QString& msg, const QString&) { emit apiError(msg); });
}

void NcmAuthApi::createQrKey()
{
    // 网页端登录 key（type=1 web 端）：与二维码 URL https://music.163.com/login?codekey=<unikey> 配对。
    // 注意：经典端点 /weapi/login/qr/unikey 已从线上移除（返回 404），必须用 /weapi/login/qrcode/unikey
    NcmProtocol protocol;
    QJsonObject payload;
    payload["type"] = 1;
    payload["time"] = QDateTime::currentMSecsSinceEpoch();

    m_curl->send(protocol.prepareWeapi("/weapi/login/qrcode/unikey", payload), QStringLiteral("unikey"));
}

void NcmAuthApi::onUnikeyResult(int httpStatus, const QByteArray& body)
{
    NETWORK_LOG << "NcmAuthApi: unikey http=" << httpStatus
                << " body=" << QString::fromUtf8(body).left(120);

    if (httpStatus != 200) {
        emit apiError(QStringLiteral("获取二维码失败: http=%1").arg(httpStatus));
        return;
    }
    const QJsonObject root = extractJsonObject(body);

    // 防御性双读：unikey 可能在顶层或 data 里
    QString unikey = root["unikey"].toString();
    if (unikey.isEmpty())
        unikey = root["data"].toObject()["unikey"].toString();

    NETWORK_LOG << "NcmAuthApi: unikey=" << unikey;
    if (unikey.isEmpty()) {
        emit apiError(QStringLiteral("获取二维码失败: unikey为空"));
        return;
    }

    const QString loginUrl = QStringLiteral("https://music.163.com/login?codekey=%1").arg(unikey);
    emit qrKeyReady(unikey, loginUrl);
}

void NcmAuthApi::checkQr(const QString& unikey)
{
    // 网页端登录状态轮询：800/801/802/803（顶层 code，与 /weapi/login/qrcode/unikey 配对）
    NcmProtocol protocol;
    QJsonObject payload;
    payload["key"]  = unikey;
    payload["type"] = 1;
    payload["time"] = QDateTime::currentMSecsSinceEpoch();

    m_curl->send(protocol.prepareWeapi("/weapi/login/qrcode/client/login", payload),
                 QStringLiteral("check"));
}

void NcmAuthApi::onCheckResult(int httpStatus, const QByteArray& body, const QString& setCookie)
{
    if (httpStatus != 200) {
        emit apiError(QStringLiteral("扫码状态查询失败: http=%1").arg(httpStatus));
        return;
    }
    const QJsonObject root = extractJsonObject(body);

    // 防御性双读：code 可能在顶层或 data 里
    int code = root.contains("code") ? root["code"].toInt()
                                     : root["data"].toObject()["code"].toInt();
    NETWORK_LOG << "NcmAuthApi: check code=" << code
                << " body=" << QString::fromUtf8(body).left(120);

    if (code == 803 && !setCookie.isEmpty()) {
        // 登录成功但 body 未带 cookie：把 Set-Cookie 头注入 data，供控制器提取 MUSIC_U/__csrf
        const bool bodyHasCookie = !root["cookie"].toString().isEmpty()
            || !root["data"].toObject()["cookie"].toString().isEmpty();
        if (!bodyHasCookie) {
            QJsonObject data = root;
            data["cookie"] = setCookie;
            emit qrCheckFinished(code, data);
            return;
        }
    }
    emit qrCheckFinished(code, root);
}

