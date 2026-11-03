#include "CookieStore.h"
#include "Oran7AppConfigManager.h"
#include "Oran7CredentialStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include <wincrypt.h>

namespace
{
int checks = 0;
void check(bool value, const char* reason)
{
    if (!value)
    {
        std::fprintf(stderr, "FAIL: %s\n", reason);
        std::exit(1);
    }
    ++checks;
}
QByteArray read(const QString& path)
{
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "read fixture");
    return file.readAll();
}
void write(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "write fixture");
}
QJsonObject json(const QString& path)
{
    return QJsonDocument::fromJson(read(path)).object();
}
QJsonObject refs(const QString& path)
{
    return json(path).value("credentialStorage").toObject().value("groups").toObject();
}
QString cipherPath(const QString& path, const QString& revision)
{
    return path + ".credentials/" + revision + ".dpapi";
}

void dpapi()
{
    QTemporaryDir dir;
    Oran7CredentialStore store(dir.path());
    QString revision, error;
    const QJsonObject secret{{"MUSIC_U", "DPAPI_SENTINEL"}, {"__csrf", "CSRF_SENTINEL"}};
    check(store.writeVerified("netease", secret, &revision, &error), "real current-user DPAPI verified write");
    check(Oran7CredentialStore::validRevision(revision) && error.isEmpty(), "valid immutable revision");
    const auto file = dir.filePath(revision + ".dpapi");
    const auto bytes = read(file);
    check(!bytes.contains("DPAPI_SENTINEL") && !bytes.contains("MUSIC_U"), "file contains ciphertext only");
    QJsonObject restored;
    check(store.read("netease", revision, &restored, &error) && restored == secret, "DPAPI roundtrip");
    check(!store.read("bilibili", revision, &restored, &error), "group binding rejects swapped references");
    check(!store.read("netease", "../../config", &restored, &error), "path traversal rejected");
    auto corrupt = bytes;
    corrupt[corrupt.size() - 1] = char(corrupt.back() ^ 0x80);
    write(file, corrupt);
    check(!store.read("netease", revision, &restored, &error) && !error.isEmpty(),
          "DPAPI integrity failure is fail closed");
    write(file, "ORAN7-DPAPI-2\nfuture-format");
    check(!store.read("netease", revision, &restored, &error), "unknown encrypted format rejected");

    // 不切换真实 Windows 账号；用不同 purpose 的实际 DPAPI blob 覆盖相同不可解密分支。
    QByteArray source("{\"version\":1}");
    QByteArray entropy("foreign-application-purpose");
    DATA_BLOB plain{DWORD(source.size()), reinterpret_cast<BYTE*>(source.data())};
    DATA_BLOB purpose{DWORD(entropy.size()), reinterpret_cast<BYTE*>(entropy.data())};
    DATA_BLOB encrypted{};
    check(CryptProtectData(&plain, L"fixture", &purpose, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted),
          "foreign-purpose fixture");
    const auto foreign =
        QByteArray("ORAN7-DPAPI-1\n") + QByteArray(reinterpret_cast<char*>(encrypted.pbData), int(encrypted.cbData));
    LocalFree(encrypted.pbData);
    write(file, foreign);
    check(!store.read("netease", revision, &restored, &error), "unprotect failure does not expose a credential");
}

void migration(const QJsonObject& defaults)
{
    QTemporaryDir dir;
    const auto path = dir.filePath("config.json");
    auto legacy = defaults;
    legacy["netease_auth"] = QJsonObject{{"MUSIC_U", "NCM_SENTINEL"},
                                         {"__csrf", "CSRF_SENTINEL"},
                                         {"nickname", "账号甲"},
                                         {"future_cookie", "FUTURE_SENTINEL"}};
    legacy["bilibili_auth"] = QJsonObject{
        {"sessdata", "BILI_SENTINEL"}, {"bili_jct", "JCT_SENTINEL"}, {"dede_user_id", "123"}, {"user_name", "账号乙"}};
    auto recording = legacy.value("recording").toObject();
    for (const auto& key : {"bearer_token", "turn_user", "turn_password", "signaling_user", "signaling_password"})
        recording[key] = QString(key) + "_SENTINEL";
    for (const auto& key : {"whip_url", "stun_url", "turn_url", "signaling_url"})
        recording[key] = "https://user:URL_SENTINEL@example.test/private?auth=QUERY_SENTINEL";
    recording["width"] = 2560;
    recording["height"] = 1440;
    recording["future_option"] = 42;
    legacy["recording"] = recording;
    legacy["user"] = QJsonObject{
        {"username", "LEGACY_USER_SENTINEL"}, {"password", "LEGACY_PASSWORD_SENTINEL"}, {"remember_password", true}};
    legacy["extension"] = QJsonObject{{"keep", 42}};
    write(path, QJsonDocument(legacy).toJson());
    const auto original = read(path);
    QJsonObject firstRefs;
    {
        Oran7AppConfigManager config(path, defaults);
        check(config.loadConfig() && config.credentialError().isEmpty(), "legacy startup migrates immediately");
        check(config.snapshot().auth.netease.musicU == "NCM_SENTINEL" &&
                  config.snapshot().auth.bilibili.sessdata == "BILI_SENTINEL",
              "in-memory auth preserved");
        check(config.snapshot().recording.turnPassword == "turn_password_SENTINEL" &&
                  config.snapshot().recording.width == 2560,
              "recording values restored");
        check(!read(path).contains("SENTINEL") && !json(path).contains("netease_auth") &&
                  !json(path).contains("bilibili_auth"),
              "no plaintext credentials in config");
        check(json(path).value("extension") == legacy.value("extension") &&
                  json(path).value("recording").toObject().value("future_option") == QJsonValue(42),
              "ordinary unknown fields retained");
        firstRefs = refs(path);
        check(firstRefs.size() == 4, "all credential groups migrated");
        for (const auto& name : QDir(config.credentialDirectory()).entryList({"*.dpapi"}, QDir::Files))
            check(!read(QDir(config.credentialDirectory()).filePath(name)).contains("SENTINEL"),
                  "encrypted generations contain no plaintext");
        const auto backups = QDir(dir.path()).entryList({"config.json.bak-*"}, QDir::Files);
        check(backups.size() == 1 && read(dir.filePath(backups.first())) == original,
              "original historical plaintext backup retained exactly");
        auto value = config.snapshot();
        value.player.volume = 66;
        config.updateConfig(value);
        check(config.saveConfig() && refs(path) == firstRefs, "ordinary setting does not rotate credentials");
        CookieStore cookies(std::make_unique<Oran7DpapiCookiePersistence>(config));
        check(cookies.load() && cookies.musicU() == "NCM_SENTINEL", "real CookiePersistence uses secure snapshot");
        cookies.setAuthCookie("MUSIC_U", "NEW_NCM_SENTINEL");
        check(cookies.save() && !read(path).contains("NEW_NCM_SENTINEL"), "real cookie save is encrypted");
        check(refs(path).value("bilibili") == firstRefs.value("bilibili"), "other credential group unchanged");
        cookies.clear();
        check(cookies.persistenceError().isEmpty() && !cookies.isLoggedIn(), "logout persists tombstone");
    }
    {
        const auto before = read(path);
        const auto count = QDir(path + ".credentials").entryList({"*.dpapi"}, QDir::Files).size();
        Oran7AppConfigManager config(path, defaults);
        check(config.snapshot().auth.netease.musicU.isEmpty() &&
                  config.snapshot().auth.bilibili.sessdata == "BILI_SENTINEL",
              "logout survives restart without affecting Bilibili");
        check(config.snapshot().recording.whipUrl.contains("URL_SENTINEL") &&
                  config.getValueQVariant("user.password") == "LEGACY_PASSWORD_SENTINEL",
              "other secure groups restored");
        check(config.saveConfig() && read(path) == before &&
                  QDir(path + ".credentials").entryList({"*.dpapi"}, QDir::Files).size() == count,
              "repeat migration idempotent");
    }
}

void failures(const QJsonObject& defaults)
{
    QTemporaryDir dir;
    const auto path = dir.filePath("config.json");
    Oran7AppConfigManager config(path, defaults);
    const auto initial = read(path);
    write(config.credentialDirectory(), "not-a-directory");
    auto value = config.snapshot();
    value.auth.netease = {"PENDING_SENTINEL", "csrf", "name"};
    config.updateConfig(value);
    check(!config.saveConfig() && read(path) == initial && !config.lastError().isEmpty(),
          "encryption storage failure preserves config");
    check(config.snapshot().auth.netease.musicU == "PENDING_SENTINEL", "failed save keeps pending in-memory state");
    check(QFile::remove(config.credentialDirectory()) && config.saveConfig(),
          "save retry recovers without losing intent");
    const auto old = read(path);
    const auto oldRefs = refs(path);
    value = config.snapshot();
    value.auth.netease.musicU = "UNCOMMITTED_SENTINEL";
    config.updateConfig(value);
    HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(lock != INVALID_HANDLE_VALUE, "lock config commit fixture");
    check(!config.saveConfig() && read(path) == old && refs(path) == oldRefs,
          "interrupted final commit preserves old reference");
    {
        Oran7AppConfigManager reader(path, defaults);
        check(reader.snapshot().auth.netease.musicU == "PENDING_SENTINEL",
              "orphan new ciphertext cannot replace committed generation");
    }
    CloseHandle(lock);
    check(config.saveConfig() && refs(path).value("netease") != oldRefs.value("netease"),
          "commit retry publishes verified ciphertext");
    {
        Oran7AppConfigManager stale(path, defaults);
        auto changed = config.snapshot();
        changed.auth.bilibili.sessdata = "CONCURRENT_SENTINEL";
        config.updateConfig(changed);
        check(config.saveConfig(), "newer instance commits credential reference");
        const auto latest = read(path);
        changed = stale.snapshot();
        changed.player.volume = 17;
        stale.updateConfig(changed);
        check(!stale.saveConfig() && read(path) == latest, "stale instance cannot resurrect earlier login references");
    }
    const auto currentRefs = refs(path);
    const auto currentCipher = cipherPath(path, currentRefs.value("netease").toString());
    auto corrupt = read(currentCipher);
    corrupt[corrupt.size() - 1] = char(corrupt.back() ^ 0x40);
    write(currentCipher, corrupt);
    auto injected = json(path);
    injected["netease_auth"] = QJsonObject{{"MUSIC_U", "STALE_PLAINTEXT_SENTINEL"}};
    write(path, QJsonDocument(injected).toJson());
    {
        Oran7AppConfigManager broken(path, defaults);
        check(broken.snapshot().auth.netease.musicU.isEmpty() && !broken.credentialError("netease").isEmpty(),
              "corrupt credential never falls back to stale plaintext");
        auto settings = broken.snapshot();
        settings.player.volume = 21;
        broken.updateConfig(settings);
        check(broken.saveConfig() && refs(path).value("netease") == currentRefs.value("netease") &&
                  read(currentCipher) == corrupt,
              "ordinary save preserves unreadable ciphertext and reference");
        check(!read(path).contains("STALE_PLAINTEXT_SENTINEL"), "stale plaintext removed without using it");
        settings = broken.snapshot();
        settings.auth.bilibili.sessdata = "OTHER_ACCOUNT_SENTINEL";
        broken.updateConfig(settings);
        check(broken.saveConfig() && refs(path).value("netease") == currentRefs.value("netease"),
              "another login preserves unreadable group");
        CookieStore cookies(std::make_unique<Oran7DpapiCookiePersistence>(broken));
        check(!cookies.load() && !cookies.isLoggedIn(), "cookie load reports decryption failure");
        cookies.setAuthCookie("MUSIC_U", "RELOGIN_SENTINEL");
        check(cookies.save() && broken.credentialError("netease").isEmpty(), "explicit relogin repairs only its group");
        check(read(currentCipher) == corrupt, "recovery preserves previous encrypted evidence");
        check(broken.clearCredentialGroup("netease"), "explicit logout succeeds after relogin");
    }
    {
        Oran7AppConfigManager restored(path, defaults);
        check(restored.snapshot().auth.netease.musicU.isEmpty() &&
                  restored.snapshot().auth.bilibili.sessdata == "OTHER_ACCOUNT_SENTINEL",
              "recovery and clear survive restart");
        restored.resetToDefaults();
        Oran7AppConfigManager reset(path, defaults);
        check(reset.snapshot().auth.bilibili.sessdata.isEmpty() && reset.snapshot().player.volume == 25,
              "reset clears active credentials securely");
    }
    for (const QJsonObject marker : {QJsonObject{{"version", 99}, {"groups", QJsonObject{}}},
                                     QJsonObject{{"version", 1}, {"groups", QJsonObject{{"netease", "../invalid"}}}}})
    {
        auto unknown = defaults;
        unknown["credentialStorage"] = marker;
        unknown["netease_auth"] = QJsonObject{{"MUSIC_U", "UNTRUSTED_SENTINEL"}};
        const auto target = dir.filePath("future.json");
        write(target, QJsonDocument(unknown).toJson());
        const auto original = read(target);
        Oran7AppConfigManager future(target, defaults);
        check(!future.isWritable() && !future.saveConfig() && read(target) == original,
              "unknown marker or malformed reference cannot be overwritten");
        check(future.snapshot().auth.netease.musicU.isEmpty(), "unknown format fails closed in memory");
    }
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto defaults = json(QStringLiteral(ORAN7_TEST_SOURCE_ROOT) + "/config/default.json");
    dpapi();
    migration(defaults);
    failures(defaults);
    std::printf("PASS: %d credential assertions\n", checks);
}
