#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>

// QQ 音乐凭据与协议值；与网易云的数字歌曲 ID、CookieStore 完全独立。
struct Oran7QQMusicAuth
{
    QJsonArray cookies;
    QString userId;
    QString nickname;
    static Oran7QQMusicAuth fromBrowserCookies(const QJsonArray& cookies, qint64 nowSeconds = -1);
    QMap<QString, QString> values() const;
    QByteArray cookieHeader() const;
    QString musicKey() const;
    bool isValid() const { return !userId.isEmpty() && !musicKey().isEmpty(); }
    QJsonObject verificationRequest() const;
    static bool accepted(const QJsonObject& response);
    static QString responseNickname(const QJsonObject& response);
};
