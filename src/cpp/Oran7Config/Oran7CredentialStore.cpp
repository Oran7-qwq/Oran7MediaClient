#include "Oran7CredentialStore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <utility>
#include <windows.h>
#include <wincrypt.h>

namespace
{
const QByteArray magic = QByteArrayLiteral("ORAN7-DPAPI-1\n");
constexpr qint64 maxBytes = 1024 * 1024;

struct Oran7DpapiBuffer
{
    DATA_BLOB blob{};
    ~Oran7DpapiBuffer()
    {
        if (blob.pbData)
        {
            SecureZeroMemory(blob.pbData, blob.cbData);
            LocalFree(blob.pbData);
        }
    }
};

bool crypt(const QByteArray& input, const QString& group, bool encrypt, QByteArray* output)
{
    if (input.size() > maxBytes)
        return false;
    const auto purpose = QByteArrayLiteral("Oran7MediaClient.credentials.v1/") + group.toUtf8();
    DATA_BLOB source{DWORD(input.size()), reinterpret_cast<BYTE*>(const_cast<char*>(input.constData()))};
    DATA_BLOB entropy{DWORD(purpose.size()), reinterpret_cast<BYTE*>(const_cast<char*>(purpose.constData()))};
    Oran7DpapiBuffer result;
    const BOOL ok = encrypt ? CryptProtectData(&source, L"Oran7MediaClient credentials", &entropy, nullptr, nullptr,
                                               CRYPTPROTECT_UI_FORBIDDEN, &result.blob)
                            : CryptUnprotectData(&source, nullptr, &entropy, nullptr, nullptr,
                                                 CRYPTPROTECT_UI_FORBIDDEN, &result.blob);
    if (!ok || result.blob.cbData > maxBytes)
        return false;
    *output = QByteArray(reinterpret_cast<const char*>(result.blob.pbData), int(result.blob.cbData));
    return true;
}

QStringList recordingKeys()
{
    return {"whip_url",      "bearer_token",  "stun_url",       "turn_url",          "turn_user",
            "turn_password", "signaling_url", "signaling_user", "signaling_password"};
}

QJsonObject nonempty(QJsonObject object)
{
    const auto keys = object.keys();
    for (const auto& key : keys)
        if (object.value(key).isNull() || (object.value(key).isString() && object.value(key).toString().isEmpty()))
            object.remove(key);
    return object;
}
} // namespace

Oran7CredentialStore::Oran7CredentialStore(QString directory) : m_directory(std::move(directory)) {}

QStringList Oran7CredentialStore::groups()
{
    return {"netease", "bilibili", "recording", "legacy"};
}

bool Oran7CredentialStore::validRevision(const QString& revision)
{
    static const QRegularExpression id(
        QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    return id.match(revision).hasMatch();
}

bool Oran7CredentialStore::read(const QString& group, const QString& revision, QJsonObject* data, QString* error) const
{
    *data = {};
    const auto fail = [&]
    {
        if (error)
            *error = QStringLiteral("凭据不可解密或已损坏，请重新登录或重新设置该组参数");
        return false;
    };
    if (!groups().contains(group) || !validRevision(revision))
        return fail();
    QFile file(QDir(m_directory).filePath(revision + ".dpapi"));
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxBytes)
        return fail();
    auto bytes = file.readAll();
    if (!bytes.startsWith(magic))
        return fail();
    QByteArray plain;
    if (!crypt(bytes.mid(magic.size()), group, false, &plain))
        return fail();
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(plain, &parse);
    plain.fill('\0');
    if (parse.error != QJsonParseError::NoError || !doc.isObject())
        return fail();
    const auto object = doc.object();
    if (object.value("version") != QJsonValue(1) || object.value("group").toString() != group ||
        !object.value("data").isObject())
        return fail();
    *data = object.value("data").toObject();
    if (error)
        error->clear();
    return true;
}

bool Oran7CredentialStore::writeVerified(const QString& group, const QJsonObject& data, QString* revision,
                                         QString* error) const
{
    const auto fail = [&]
    {
        if (error)
            *error = QStringLiteral("凭据加密保存失败，原配置与原凭据引用未更改");
        return false;
    };
    if (!groups().contains(group))
        return fail();
    auto plain =
        QJsonDocument(QJsonObject{{"version", 1}, {"group", group}, {"data", data}}).toJson(QJsonDocument::Compact);
    QByteArray encrypted;
    const bool protectedOk = crypt(plain, group, true, &encrypted);
    plain.fill('\0');
    if (!protectedOk || !QDir().mkpath(m_directory))
        return fail();
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSaveFile file(QDir(m_directory).filePath(id + ".dpapi"));
    file.setDirectWriteFallback(false);
    const auto bytes = magic + encrypted;
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail();
    QJsonObject verified;
    if (!read(group, id, &verified, error) || verified != data)
        return fail();
    *revision = id; // 验证成功后才允许普通配置引用它；失败留下的也只是无引用密文。
    if (error)
        error->clear();
    return true;
}

QJsonObject Oran7CredentialStore::extract(const QJsonObject& config, const QString& group)
{
    if (group == "netease" || group == "bilibili")
        return nonempty(config.value(group + "_auth").toObject());
    QJsonObject result;
    const auto source = config.value(group == "legacy" ? "user" : "recording").toObject();
    const auto keys = group == "legacy" ? QStringList{"username", "password"} : recordingKeys();
    for (const auto& key : keys)
    {
        const auto value = source.value(key);
        if (value.isUndefined() || value.isNull() || (value.isString() && value.toString().isEmpty()))
            continue;
        if (group == "legacy" && key == "username" && value.toString() == "default")
            continue;
        result[key] = value;
    }
    return result;
}

void Oran7CredentialStore::remove(QJsonObject* config, const QString& group)
{
    if (group == "netease" || group == "bilibili")
    {
        config->remove(group + "_auth");
        return;
    }
    const QString section = group == "legacy" ? "user" : "recording";
    auto object = config->value(section).toObject();
    for (const auto& key : group == "legacy" ? QStringList{"username", "password"} : recordingKeys())
        object.remove(key);
    (*config)[section] = object;
}

void Oran7CredentialStore::apply(QJsonObject* config, const QString& group, const QJsonObject& data)
{
    remove(config, group);
    if (group == "netease" || group == "bilibili")
    {
        (*config)[group + "_auth"] = data;
        return;
    }
    const QString section = group == "legacy" ? "user" : "recording";
    auto object = config->value(section).toObject();
    for (auto it = data.begin(); it != data.end(); ++it)
        object[it.key()] = it.value();
    (*config)[section] = object;
}

QJsonObject Oran7CredentialStore::publicConfig(QJsonObject config, const QJsonObject& refs)
{
    for (const auto& group : groups())
        if (refs.contains(group) || !extract(config, group).isEmpty())
            remove(&config, group);
    if (!refs.isEmpty())
        config["credentialStorage"] = QJsonObject{{"version", 1}, {"groups", refs}};
    else
        config.remove("credentialStorage");
    return config;
}
