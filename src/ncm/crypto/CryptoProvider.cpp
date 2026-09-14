#include "CryptoProvider.h"

#include <QRandomGenerator>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

// OpenSSL 3.0 起低层 RSA API 标记 deprecated，屏蔽告警避免 -Werror
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

QByteArray CryptoProvider::randomString(int length)
{
    static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QByteArray result;
    result.reserve(length);
    for (int i = 0; i < length; ++i)
        result.append(chars[QRandomGenerator::global()->bounded(62)]);
    return result;
}

QByteArray CryptoProvider::aesCbcEncrypt(const QByteArray& data,
                                         const QByteArray& key,
                                         const QByteArray& iv)
{
    QByteArray out(data.size() + 16, Qt::Uninitialized);
    int outLen = 0, finalLen = 0;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return QByteArray();
    EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr,
                       reinterpret_cast<const unsigned char*>(key.constData()),
                       reinterpret_cast<const unsigned char*>(iv.constData()));
    EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &outLen,
                      reinterpret_cast<const unsigned char*>(data.constData()), data.size());
    EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + outLen, &finalLen);
    outLen += finalLen;
    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen);
    return out;
}

QByteArray CryptoProvider::rsaNoPaddingEncrypt(const QByteArray& data)
{
    static const char* modulusHex =
        "00e0b509f6259df8642dbc35662901477df22677ec152b5ff68ace615bb7b7251"
        "52b3ab17a876aea8a5aa76d2e417629ec4ee341f56135fccf695280104e0312ec"
        "bda92557c93870114af6c9d05c4f7f0c3685b7a46bee255932575cce10b424d81"
        "3cfe4875d3e82047b97ddef52741d546b8e289dc6935b3ece0462db0a22b8e7";
    BIGNUM* n = nullptr;
    BIGNUM* e = nullptr;
    BN_hex2bn(&n, modulusHex);
    BN_hex2bn(&e, "010001");
    RSA* rsa = RSA_new();
    RSA_set0_key(rsa, n, e, nullptr);

    const int keySize = RSA_size(rsa);
    QByteArray input(keySize - data.size(), '\0');
    input += data;

    QByteArray out(keySize, Qt::Uninitialized);
    const int outLen = RSA_public_encrypt(keySize,
        reinterpret_cast<const unsigned char*>(input.constData()),
        reinterpret_cast<unsigned char*>(out.data()), rsa, RSA_NO_PADDING);
    RSA_free(rsa);
    if (outLen < 0)
        return QByteArray();
    out.resize(outLen);
    return out;
}

QByteArray CryptoProvider::reverseBytes(const QByteArray& data)
{
    QByteArray result = data;
    for (int i = 0; i < result.size() / 2; ++i) {
        const char tmp = result[i];
        result[i] = result[result.size() - 1 - i];
        result[result.size() - 1 - i] = tmp;
    }
    return result;
}

#pragma GCC diagnostic pop
