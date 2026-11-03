#include "NcmProtocol.h"

#include "WeapiCrypto.h"

const QString NcmProtocol::kBaseUrl = QStringLiteral("https://music.163.com");

NcmRequest NcmProtocol::prepare(NcmApiType type, const QString& path,
                                const QJsonObject& payload) const
{
    switch (type) {
    case NcmApiType::WEAPI:
        return prepareWeapi(path, payload);
    case NcmApiType::EAPI: // v1 未实现
        return NcmRequest();
    }
    return NcmRequest();
}

NcmRequest NcmProtocol::prepareWeapi(const QString& path, const QJsonObject& payload) const
{
    const WeapiResult encrypted = WeapiCrypto::createParams(payload);

    NcmRequest req;
    req.url = QUrl(kBaseUrl + path);
    req.tag = req.url.path();
    req.body = encrypted.toFormBody();
    req.isPost = true;
    req.disableHttp2 = true; // 网易云部分端点对 HTTP/2 返回空响应，强制 1.1

    req.headers.insert(QByteArrayLiteral("Content-Type"),
                       QByteArrayLiteral("application/x-www-form-urlencoded"));
    req.headers.insert(QByteArrayLiteral("User-Agent"),
        QByteArrayLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                          "(KHTML, like Gecko) Chrome/120 Safari/537.36"));
    req.headers.insert(QByteArrayLiteral("Accept"),
                       QByteArrayLiteral("*/*"));
    req.headers.insert(QByteArrayLiteral("Accept-language"),
                       QByteArrayLiteral("zh-CN,zh-Hans;q=0.9"));
    req.headers.insert(QByteArrayLiteral("Referer"),
                       QByteArrayLiteral("https://music.163.com"));
    // 注意：不带 os=pc cookie（带了会导致部分端点返回空响应/触发风控）。
    // Accept-Encoding 不显式设置（curl 不带该头），避免 identity 被 WAF 当异常信号。
    return req;
}
