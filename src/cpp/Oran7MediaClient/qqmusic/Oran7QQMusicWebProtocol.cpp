/*
 * zzc/ag-1 protocol adapted from https://github.com/jixunmoe/qmweb-sign
 * Copyright (c) 2024 Jixun Wu. MIT License.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "Oran7QQMusicWebProtocol.h"
#include <QCryptographicHash>
#include <windows.h>
#include <bcrypt.h>
namespace
{
struct Oran7CngAlgorithm
{
    BCRYPT_ALG_HANDLE value = nullptr;
    ~Oran7CngAlgorithm()
    {
        if (value)
            BCryptCloseAlgorithmProvider(value, 0);
    }
};
struct Oran7CngKey
{
    BCRYPT_KEY_HANDLE value = nullptr;
    ~Oran7CngKey()
    {
        if (value)
            BCryptDestroyKey(value);
    }
};
} // namespace
QString Oran7QQMusicWebProtocol::signature(const QByteArray& json)
{
    const auto digest = QCryptographicHash::hash(json, QCryptographicHash::Sha1);
    const auto hex = digest.toHex().toUpper();
    const unsigned char scramble[]{89,  39,  179, 150, 218, 82,  58,  252, 177, 52,
                                   186, 123, 120, 64,  242, 133, 143, 161, 121, 179};
    QByteArray middle;
    for (int i = 0; i < 20; ++i)
        middle.append(char(scramble[i] ^ static_cast<unsigned char>(digest[i])));
    middle = middle.toBase64();
    middle.replace("/", "");
    middle.replace("+", "");
    middle.replace("=", "");
    middle.replace("\\", "");
    QByteArray result("zzc");
    for (int i : {23, 14, 6, 36, 16, 7, 19})
        result += hex[i];
    result += middle;
    for (int i : {16, 1, 32, 12, 19, 27, 8, 5})
        result += hex[i];
    return QString::fromLatin1(result.toLower());
}
QByteArray Oran7QQMusicWebProtocol::encrypt(const QByteArray& json)
{
    unsigned char secret[]{189, 48, 95, 16, 208, 255, 116, 182, 239, 84, 218, 184, 53, 181, 225, 207};
    QByteArray iv(12, '\0'), tag(16, '\0'), output(json.size(), '\0');
    Oran7CngAlgorithm algorithm;
    Oran7CngKey key;
    if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0 ||
        BCryptSetProperty(algorithm.value, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) < 0 ||
        BCryptGenerateSymmetricKey(algorithm.value, &key.value, nullptr, 0, secret, sizeof(secret), 0) < 0 ||
        BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(iv.data()), ULONG(iv.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return {};
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = reinterpret_cast<PUCHAR>(iv.data());
    info.cbNonce = ULONG(iv.size());
    info.pbTag = reinterpret_cast<PUCHAR>(tag.data());
    info.cbTag = ULONG(tag.size());
    ULONG written = 0;
    if (BCryptEncrypt(key.value, reinterpret_cast<PUCHAR>(const_cast<char*>(json.constData())), ULONG(json.size()),
                      &info, nullptr, 0, reinterpret_cast<PUCHAR>(output.data()), ULONG(output.size()), &written,
                      0) < 0)
        return {};
    output.resize(written);
    return (iv + output + tag).toBase64();
}
QByteArray Oran7QQMusicWebProtocol::decodeResponse(QByteArray response)
{
    const unsigned char key[]{122, 63, 140, 29, 94,  155, 47, 10,  108, 77, 126,
                              139, 31, 58,  92, 157, 14,  43, 111, 74,  129};
    for (qsizetype i = 0; i < response.size(); ++i)
        response[i] = char(static_cast<unsigned char>(response[i]) ^ key[i % sizeof(key)]);
    return response;
}
