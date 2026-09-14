#include "WeapiCrypto.h"

#include "CryptoProvider.h"

#include <QJsonDocument>
#include <QUrl>

QByteArray WeapiResult::toFormBody() const
{
    // 关键：必须用 QUrl::toPercentEncoding 逐个编码。
    // base64 里的 '+' '/' '=' 若不转成 %2B %2F %3D，在 application/x-www-form-urlencoded
    // 下 '+' 会被服务端解码成空格，params 损坏 → 网易云返回 200 空 body（表现为登录/取歌失败）。
    QByteArray body;
    body += "params=";
    body += QUrl::toPercentEncoding(params);
    body += "&encSecKey=";
    body += QUrl::toPercentEncoding(encSecKey);
    return body;
}

WeapiResult WeapiCrypto::createParams(const QJsonObject& payload)
{
    const QByteArray presetKey = "0CoJUm6Qyw8W8jud"; // 第一层固定 AES key
    const QByteArray iv        = "0102030405060708";
    const QByteArray secKey    = CryptoProvider::randomString(16);

    const QByteArray text = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray params = CryptoProvider::aesCbcEncrypt(text, presetKey, iv);        // 第一轮
    params = CryptoProvider::aesCbcEncrypt(params.toBase64(), secKey, iv);         // 第二轮

    const QByteArray encSecKey =
        CryptoProvider::rsaNoPaddingEncrypt(CryptoProvider::reverseBytes(secKey));

    return { QString::fromLatin1(params.toBase64()),
             QString::fromLatin1(encSecKey.toHex()) };
}
