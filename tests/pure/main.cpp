#include "CryptoProvider.h"
#include "Oran7LrcParser.h"
#include "WeapiCrypto.h"
#include <QCoreApplication>
#include <QUrlQuery>
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

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Oran7LrcParser lrc;
    check(!lrc.parseContent("[ar:artist]\ninvalid") && lrc.currentLineIndex(0) == -1,
          "metadata-only lyrics have no current line");
    check(lrc.parseContent("[00:02.500]second\n[00:01.25] first "), "valid lyrics parsed");
    check(lrc.lines().size() == 2 && lrc.lines()[0].timestampMs == 1250 && lrc.lines()[0].text == "first",
          "timestamps sorted with centisecond conversion and trimmed text");
    check(lrc.currentLineIndex(0.949) == -1 && lrc.currentLineIndex(0.950) == 0,
          "existing 300ms compensation boundary preserved");
    check(lrc.currentLineIndex(2.200) == 1 && lrc.currentLineIndex(100) == 1, "seek and final lyric boundary");
    lrc.clear();
    check(!lrc.hasLyrics() && lrc.currentLineIndex(0) == -1, "clear releases previous lyrics");
    // NIST SP 800-38A F.2.1 首个 AES-CBC 密文块，避免用实现自身生成预期值。
    const auto cipher = CryptoProvider::aesCbcEncrypt(QByteArray::fromHex("6bc1bee22e409f96e93d7e117393172a"),
                                                      QByteArray::fromHex("2b7e151628aed2a6abf7158809cf4f3c"),
                                                      QByteArray::fromHex("000102030405060708090a0b0c0d0e0f"));
    check(cipher.size() == 32 && cipher.left(16).toHex() == "7649abac8119b246cee98e9b12e9197d",
          "AES CBC published vector and PKCS7 padding");
    check(CryptoProvider::reverseBytes("abc") == "cba", "RSA input byte reversal");
    const WeapiResult escaped{"+/= 中文", "ab12"};
    const auto form = escaped.toFormBody();
    check(form.contains("%2B") && form.contains("%2F") && form.contains("%3D"), "base64 form escaping");
    const QUrlQuery decoded(QString::fromLatin1(form));
    check(decoded.queryItemValue("params", QUrl::FullyDecoded) == escaped.params, "form roundtrip");
    const auto first = WeapiCrypto::createParams({{"message", "fixture"}});
    const auto second = WeapiCrypto::createParams({{"message", "fixture"}});
    check(first.encSecKey.size() == 256 && !QByteArray::fromBase64(first.params.toLatin1()).isEmpty(),
          "WEAPI output contains 1024-bit RSA and base64 AES ciphertext");
    check(first.params != second.params && first.encSecKey != second.encSecKey, "fresh per-request secret key");
    std::printf("PASS: %d pure logic checks\n", checks);
}
