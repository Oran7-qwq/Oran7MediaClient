#ifndef CRYPTOPROVIDER_H
#define CRYPTOPROVIDER_H

#include <QByteArray>

/**
 * @brief 网易云加密原语（OpenSSL）
 *
 * AES-128-CBC / RSA 无 padding，供 WeapiCrypto 使用。
 * 纯静态工具类，不依赖任何业务概念（不知道 WEAPI、不知道扫码登录）。
 */
class CryptoProvider
{
public:
    CryptoProvider() = delete;

    /// 62 字符（a-zA-Z0-9）随机串，用于生成 weapi 的 secKey
    static QByteArray randomString(int length);

    /// AES-128-CBC 加密（PKCS#7 padding，EVP 默认处理）
    static QByteArray aesCbcEncrypt(const QByteArray& data,
                                    const QByteArray& key, // 16 字节
                                    const QByteArray& iv);

    /// 网易云 weapi 公钥 RSA 无 padding 加密（1024-bit，输出 128 字节）
    static QByteArray rsaNoPaddingEncrypt(const QByteArray& data);

    /// 字节反转。手写循环，不用 std::reverse（避免 MSYS2 头文件遮蔽 MinGW libstdc++）
    static QByteArray reverseBytes(const QByteArray& data);
};

#endif // CRYPTOPROVIDER_H
