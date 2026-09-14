#ifndef WEAPICRYPTO_H
#define WEAPICRYPTO_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

/**
 * @brief WEAPI 加密参数构造
 *
 * 双层 AES-128-CBC + 随机 16 位 secKey 反转后 RSA 无 padding，
 * 最终输出 HTTP form body：params=<base64>&encSecKey=<hex>
 */
struct WeapiResult
{
    QString params;
    QString encSecKey;

    /// "params=..&encSecKey=.." 的 URL 编码 form body（UTF-8）
    QByteArray toFormBody() const;
};

class WeapiCrypto
{
public:
    WeapiCrypto() = delete;

    /// 把 payload 加密成 WEAPI 的 params/encSecKey
    static WeapiResult createParams(const QJsonObject& payload);
};

#endif // WEAPICRYPTO_H
