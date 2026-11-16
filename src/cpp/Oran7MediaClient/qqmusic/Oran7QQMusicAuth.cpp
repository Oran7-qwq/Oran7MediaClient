#include "Oran7QQMusicAuth.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

namespace
{
QString normalizedUin(QString value)
{
    if (value.startsWith('o'))
        value.remove(0, 1);
    while (value.startsWith('0') && value.size() > 1)
        value.remove(0, 1);
    static const QRegularExpression digits(QStringLiteral("^[1-9][0-9]{0,19}$"));
    return digits.match(value).hasMatch() ? value : QString();
}
} // namespace

Oran7QQMusicAuth Oran7QQMusicAuth::fromBrowserCookies(const QJsonArray& source, qint64 nowSeconds)
{
    Oran7QQMusicAuth result;
    if (nowSeconds < 0)
        nowSeconds = QDateTime::currentSecsSinceEpoch();
    const QSet<QString> names{"uin",
                              "qqmusic_uin",
                              "wxuin",
                              "login_type",
                              "qm_keyst",
                              "qqmusic_key",
                              "music_key",
                              "wxskey",
                              "skey",
                              "p_skey",
                              "psrf_qqunionid",
                              "psrf_qqopenid",
                              "psrf_qqaccess_token",
                              "psrf_qqrefresh_token",
                              "wxopenid",
                              "wxunionid",
                              "wxrefresh_token"};
    // 音乐域优先；QQ 根域兼容官网共享票据，不接受其它 QQ 子站的同名 Cookie。
    QMap<QString, QJsonObject> kept;
    QMap<QString, int> priorities;
    for (const auto& value : source)
    {
        auto item = value.toObject();
        const QString name = item.value("name").toString();
        const QString text = item.value("value").toString();
        QString domain = item.value("domain").toString().toLower();
        if (domain.startsWith('.'))
            domain.remove(0, 1);
        const bool musicDomain = domain == "y.qq.com" || domain.endsWith(".y.qq.com") || domain == "music.qq.com" ||
                                 domain.endsWith(".music.qq.com");
        const bool commonDomain = domain == "qq.com";
        const double expires = item.value("expires").toDouble(-1);
        if (!names.contains(name) || (!musicDomain && !commonDomain) || text.isEmpty() || text.size() > 8192 ||
            text.contains(QRegularExpression(QStringLiteral("[\\x00-\\x20\\x7f;]"))) ||
            (expires > 0 && expires <= nowSeconds))
            continue;
        const int priority = musicDomain ? (domain == "y.qq.com" ? 3 : 2) : 1;
        if (priorities.value(name, -1) >= priority)
            continue;
        kept[name] = QJsonObject{{"name", name}, {"value", text},  {"domain", domain},
                                 {"path", "/"},  {"secure", true}, {"expires", expires}};
        priorities[name] = priority;
    }
    for (const auto& item : kept)
        result.cookies.append(item);
    const auto fields = result.values();
    const auto candidates = fields.value("login_type") == "2"
                                ? QStringList{fields.value("wxuin"), fields.value("uin"), fields.value("qqmusic_uin")}
                                : QStringList{fields.value("uin"), fields.value("qqmusic_uin"), fields.value("wxuin")};
    for (const auto& candidate : candidates)
    {
        result.userId = normalizedUin(candidate);
        if (!result.userId.isEmpty())
            break;
    }
    return result;
}
QMap<QString, QString> Oran7QQMusicAuth::values() const
{
    QMap<QString, QString> result;
    for (const auto& item : cookies)
    {
        const auto object = item.toObject();
        result.insert(object.value("name").toString(), object.value("value").toString());
    }
    return result;
}
QString Oran7QQMusicAuth::musicKey() const
{
    const auto fields = values();
    for (const auto* key : {"qm_keyst", "qqmusic_key", "music_key", "wxskey"})
        if (!fields.value(key).isEmpty())
            return fields.value(key);
    return {}; // QQ 空间/普通 QQ 的 skey 不能冒充 QQ 音乐登录凭据。
}
QByteArray Oran7QQMusicAuth::cookieHeader() const
{
    QStringList result;
    const auto fields = values();
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
        result << it.key() + '=' + it.value();
    return result.join("; ").toUtf8();
}
QJsonObject Oran7QQMusicAuth::verificationRequest() const
{
    const auto fields = values();
    quint32 token = 5381;
    for (const auto ch : musicKey())
        token = token * 33 + ch.unicode();
    return {{"comm", QJsonObject{{"ct", 19},
                                 {"cv", 0},
                                 {"format", "json"},
                                 {"uin", userId},
                                 {"authst", musicKey()},
                                 {"g_tk", int(token & 0x7fffffff)},
                                 {"tmeLoginType", fields.value("login_type") == "2" ? 1 : 2}}},
            {"req", QJsonObject{{"module", "music.UserInfo.userInfoServer"},
                                {"method", "GetLoginUserInfo"},
                                {"param", QJsonObject{}}}}};
}
bool Oran7QQMusicAuth::accepted(const QJsonObject& response)
{
    const auto block = response.value("req").toObject();
    const auto data = block.value("data").toObject();
    return response.value("code") == QJsonValue(0) && block.value("code") == QJsonValue(0) && !data.isEmpty() &&
           (!data.contains("code") || data.value("code") == QJsonValue(0));
}
QString Oran7QQMusicAuth::responseNickname(const QJsonObject& response)
{
    const auto data = response.value("req").toObject().value("data").toObject();
    for (const auto& object :
         {data, data.value("info").toObject(), data.value("user_info").toObject(), data.value("base_info").toObject()})
        for (const auto* key : {"nick", "nickname", "nickName", "name"})
        {
            const auto name = object.value(key).toString();
            if (!name.isEmpty())
                return name.left(256);
        }
    return {};
}
