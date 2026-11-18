#include "Oran7Theme.h"
#include "Oran7AppConfigManager.h"
#include "Oran7ThemeProfileManager.h"
#include <QGuiApplication>
#include <QFontDatabase>
#include <QStyleHints>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QCryptographicHash>
#include <QProcess>
#include <windows.h>
#include <QDir>
#include <QJsonArray>
#include <QUuid>
#include <cstdio>
#include <cstdlib>
#include <memory>

static int checks = 0;
static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
    ++checks;
}
static QByteArray bytes(const QString& path)
{
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "read fixture");
    return file.readAll();
}
static void write(const QString& path, const QByteArray& value)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(value) == value.size(), "write fixture");
}
static QJsonObject object(const QString& path)
{
    return QJsonDocument::fromJson(bytes(path)).object();
}

static constexpr auto LegacyDefaults = ":/config/themeJson/legacy-profile-defaults.json";
static constexpr auto BuiltInIndex = ":/config/themeJson/Index.json";
static constexpr auto BuiltInPlaylist = ":/config/themeJson/components/Oran7MusicPlaylistView.json";

static QString writeLegacySources(const QTemporaryDir& temp)
{
    auto index = object(LegacyDefaults);
    const auto components = index.value("__component__").toObject();
    check(QDir().mkpath(temp.filePath("legacy")), "create legacy source fixture directory");
    QJsonObject refs;
    for (auto it = components.begin(); it != components.end(); ++it)
    {
        const auto file = temp.filePath("legacy/" + it.key() + ".json");
        write(file, QJsonDocument(it->toObject()).toJson());
        refs[it.key()] = file;
    }
    index["__component__"] = refs;
    const auto file = temp.filePath("legacy/index.json");
    write(file, QJsonDocument(index).toJson());
    return file;
}

static QString backupPath(const QString& profile, const QByteArray& data)
{
    return profile + ".bak-" + QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

static void checkMigrationFailures(const QTemporaryDir& temp, Oran7ThemeProfileManager& storage)
{
    const QByteArray v2 = "{  \"profileVersion\" : 2, \"darkMode\":0 }\r\n";
    const QJsonObject v3{{"__profileVersion__", 3}, {"__globals__", QJsonObject{{"darkMode", 0}}}};
    const auto path = temp.filePath("profile-retry.json");
    const auto backup = backupPath(path, v2);
    write(path, v2);
    check(QDir().mkdir(backup), "place obstruction at backup path");
    check(storage.loadThemeProfile("profile-retry") == v3 && bytes(path) == v2 &&
              !storage.saveThemeProfile("profile-retry", {}),
          "backup failure preserves original and reading still returns equivalent v3 values");
    check(QDir().rmdir(backup), "remove only temporary backup obstruction");

    HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(lock != INVALID_HANDLE_VALUE, "hold readable profile without atomic replace sharing");
    check(storage.loadThemeProfile("profile-retry") == v3 && !storage.saveThemeProfile("profile-retry", {}) &&
              bytes(path) == v2 && bytes(backup) == v2,
          "atomic replace failure preserves both original and completed backup");
    CloseHandle(lock);
    Oran7ThemeProfileManager restarted(temp.path());
    check(restarted.loadThemeProfile("profile-retry") == v3 && object(path) == v3 && bytes(backup) == v2 &&
              QDir(temp.path()).entryList({"profile-retry.json.bak-*"}, QDir::Files).size() == 1,
          "restart retries migration without making a second backup");
    check(restarted.saveThemeProfile("profile-retry", {}) && bytes(backup) == v2 &&
              QDir(temp.path()).entryList({"profile-retry.json.bak-*"}, QDir::Files).size() == 1,
          "ordinary v3 writes leave migration backup alone");

    const auto malformed = temp.filePath("profile-structure.json");
    const QByteArray invalid = R"({"__profileVersion__":3,"__primary__":{"__modes__":{"dark":false}}})";
    write(malformed, invalid);
    check(!restarted.isThemeProfileReadable("profile-structure") && !restarted.saveThemeProfile("profile-structure", {}) &&
              restarted.importThemeProfile(malformed, "invalid-v3").isEmpty() && bytes(malformed) == invalid,
          "invalid v3 structural nodes cannot be imported or overwritten");
    const auto future = temp.filePath("profile-future-v3.json");
    const QByteArray futureBytes = R"({"__profileVersion__":4,"__globals__":{"darkMode":2}})";
    write(future, futureBytes);
    check(!restarted.isThemeProfileReadable("profile-future-v3") &&
              !restarted.duplicateThemeProfile("profile-future-v3", "profile-unexpected-copy") &&
              !restarted.exportThemeProfile("profile-future-v3", temp.filePath("future-export.json")) &&
              !restarted.saveThemeProfile("profile-future-v3", {}) && bytes(future) == futureBytes,
          "future v3 document is protected through read, write, copy and export");
    check(!restarted.exportThemeProfile("profile-retry", future) && bytes(future) == futureBytes,
          "export cannot bypass protection of a managed destination");

    lock = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, 0, nullptr,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(lock != INVALID_HANDLE_VALUE, "hold v3 profile without read sharing");
    check(restarted.loadThemeProfile("profile-retry").isEmpty() && !restarted.saveThemeProfile("profile-retry", v3),
          "unreadable current profile cannot be loaded or replaced");
    CloseHandle(lock);
    check(object(path) == QJsonObject{{"__profileVersion__", 3}}, "unreadable profile is unchanged");

    const auto exportPath = temp.filePath("protected-export.json");
    write(exportPath, "existing export");
    lock = CreateFileW(reinterpret_cast<LPCWSTR>(exportPath.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(lock != INVALID_HANDLE_VALUE, "hold export target without replace sharing");
    check(!restarted.exportThemeProfile("profile-retry", exportPath) && bytes(exportPath) == "existing export",
          "failed atomic export preserves previous file");
    CloseHandle(lock);
}

static void checkModeDifference(const QTemporaryDir& temp, Oran7ThemeProfileManager& storage)
{
    const auto input = QJsonDocument::fromJson(R"({
        "__profileVersion__":3,"__component__":{"Oran7MusicPlaylistView":{
            "__tokens__":{"rowAccentBase":"#aabbcc","listViewFontPixelSize":16,"extension":null},
            "__modes__":{"light":{"rowAccentBase":"#aabbcc"},"dark":{"rowAccentBase":"#ead16c"}}}}})").object();
    check(!input.isEmpty() && storage.saveThemeProfile("profile-delta", input), "save a mode layered v3 document");
    const auto saved = object(temp.filePath("profile-delta.json"));
    const auto group = saved["__component__"].toObject()["Oran7MusicPlaylistView"].toObject();
    const auto common = group["__tokens__"].toObject();
    const auto modes = group["__modes__"].toObject();
    check(common["rowAccentBase"] == "#aabbcc" && common["extension"].isNull() &&
              !common.contains("listViewFontPixelSize") && !modes.contains("light") &&
              modes["dark"].toObject()["rowAccentBase"] == "#ead16c",
          "pruning compares mode with inherited override before source default and preserves unknown null");
}

static void checkRestart(const QTemporaryDir& temp, const QString& name, bool reset)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QCoreApplication::applicationFilePath(),
                  {"--check-profile-after-restart", temp.path(), name, reset ? "reset" : "edited"});
    const bool exited = process.waitForStarted(5000) && process.waitForFinished(10000);
    const bool passed = exited && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (!passed)
        std::fprintf(stderr, "%s\n", process.readAll().constData());
    check(passed, reset ? "whole reset survives process restart" : "edited v3 profile survives process restart");
}

static void checkProfileLifecycle(const QTemporaryDir& temp, Oran7ThemeProfileManager& storage,
                                  Oran7Theme& theme)
{
    const auto v2 = QByteArray(R"json({
        "profileVersion":2,"darkMode":0,"customRoot":{"value":"  retained  "},
        "__init__":{"__base__":{"colorTextBase":"#112233|#ddeeff"}},
        "__component__":{"Oran7MusicPlaylistView":{"__style__":{
            "rowAccentLight":"#223344","rowAccentDark":"#667788","listViewFontPixelSize":18,
            "unknownLight":"$genColor(#ffffff)"},
            "__init__":{"__vars__":{"rowSecondaryTextDark":"#001122"}}}}})json");
    const auto sourcePath = temp.filePath("incoming.json");
    write(sourcePath, v2);
    const auto name = theme.importThemeProfile(sourcePath, "lifecycle");
    check(name == "profile-lifecycle" && bytes(sourcePath) == v2, "import v2 preserves external original");
    check(theme.importThemeProfile(sourcePath, "lifecycle") == name + "-1", "import avoids overwriting an existing name");
    auto document = storage.loadThemeProfile(name);
    auto group = document["__component__"].toObject()["Oran7MusicPlaylistView"].toObject();
    auto modes = group["__modes__"].toObject();
    auto common = group["__tokens__"].toObject();
    check(document["__profileVersion__"] == 3 && document["customRoot"].toObject()["value"] == "  retained  " &&
              modes["light"].toObject()["rowAccent"] == "#223344" &&
              modes["dark"].toObject()["rowAccent"] == "#667788" &&
              !modes["dark"].toObject().contains("rowSecondaryText") &&
              common["unknownLight"] == "$genColor(#ffffff)" &&
              document["__primary__"].toObject()["__modes__"].toObject()["light"].toObject()["colorTextBase"] == "#112233",
          "v2 explicit aliases and primary modes mapped without rewriting unknown suffixes");
    const auto exportFile = temp.filePath("lifecycle-export.json");
    check(theme.exportThemeProfile(name, exportFile) && object(exportFile) == document &&
              theme.importThemeProfile(exportFile, "v3-roundtrip") == "profile-v3-roundtrip" &&
              storage.loadThemeProfile("profile-v3-roundtrip") == document,
          "v3 export and reimport preserve the same canonical delta");
    theme.setCurrentThemeProfile(name);
    check(!theme.isDark() && theme.Primary()["colorTextBase"].value<QColor>() == QColor("#112233") &&
              theme.Oran7MusicPlaylistView()["rowAccentLight"].value<QColor>() == QColor("#223344") &&
              !theme.Oran7MusicPlaylistView().contains("unknownLight"),
          "legacy source consumes v3 mode and keeps unknown token inert");

    theme.saveComponentToken("Oran7MusicPlaylistView", "rowAccent", "#ffcc00");
    theme.installComponentToken("Oran7MusicPlaylistView", "rowAccentLight", "#abcdef");
    check(theme.Oran7MusicPlaylistView()["rowAccentLight"].value<QColor>() == QColor("#abcdef"),
          "playlist transient values override profile layers");
    check(theme.resetComponentToken("Oran7MusicPlaylistView", "rowAccent", true) &&
              theme.Oran7MusicPlaylistView()["rowAccentLight"].value<QColor>() == QColor("#ffcc00") &&
              theme.Oran7MusicPlaylistView()["rowAccentDark"].value<QColor>() == QColor("#667788"),
          "mode reset drops the current mode only and falls through to common");
    theme.setDarkMode(Oran7Theme::Dark);
    theme.installThemeColorTextBase("#8899aa|#ccddee");
    check(theme.Primary()["colorTextBase"].value<QColor>() == QColor("#ccddee"), "base color preview wins over mode");

    checkRestart(temp, name, false);

    check(theme.resetIndexToken("colorTextBase", true) &&
              theme.Primary()["colorTextBase"].value<QColor>() == QColor("#f3f3f3"),
          "reset current primary mode clears the corresponding preview");
    const auto activePath = temp.filePath(name + ".json");
    const auto before = bytes(activePath);
    auto fileLock = CreateFileW(reinterpret_cast<LPCWSTR>(activePath.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(fileLock != INVALID_HANDLE_VALUE, "lock current profile for reset failure");
    theme.installComponentToken("Oran7MusicPlaylistView", "listViewFontPixelSize", 19);
    check(!theme.resetComponentTheme("Oran7MusicPlaylistView"), "component reset reports atomic write failure");
    theme.resetToDefaultTheme();
    check(bytes(activePath) == before && theme.Oran7MusicPlaylistView()["listViewFontPixelSize"].toInt() == 19,
          "failed resets preserve disk, visible snapshot and transient values");
    CloseHandle(fileLock);
    check(theme.resetComponentTheme("Oran7MusicPlaylistView") &&
              theme.Oran7MusicPlaylistView()["listViewFontPixelSize"].toInt() == 16 &&
              theme.Oran7MusicPlaylistView()["rowAccentDark"].value<QColor>() == QColor("#ead16c"),
          "component reset clears common mode overrides and preview");
    const auto resetGroup = object(activePath)["__component__"].toObject()["Oran7MusicPlaylistView"].toObject();
    check(resetGroup["__tokens__"].toObject()["unknownLight"] == "$genColor(#ffffff)",
          "component reset preserves unknown extensions");
    theme.resetToDefaultTheme();
    check(object(activePath) == QJsonObject{{"__profileVersion__", 3}},
          "whole reset saves an empty v3 delta");
    checkRestart(temp, name, true);
    theme.setCurrentThemeProfile("profile-default");
}

static bool keepsLegacyValues(const QVariantMap& actual, const QVariantMap& legacy)
{
    for (auto it = legacy.cbegin(); it != legacy.cend(); ++it)
    {
        const auto value = actual.constFind(it.key());
        if (value == actual.cend() || value.value() != it.value() || value->metaType() != it->metaType())
            return false;
    }
    return true;
}

static void checkProductionSources(Oran7Theme& theme, Oran7ThemeProfileManager& profiles,
                                   const QString& legacyPath, const QTemporaryDir& temp)
{
    auto index = object(BuiltInIndex);
    const auto refs = index.value("__component__").toObject();
    const auto legacyRefs = object(legacyPath).value("__component__").toObject();
    check(index["__schemaVersion__"] == 1 && refs.size() == 9 && refs.keys() == legacyRefs.keys(),
          "production Index has a versioned resource for every existing group");
    const auto root = QStringLiteral(":/config/themeJson/");
    for (auto it = refs.begin(); it != refs.end(); ++it)
    {
        const auto source = object(root + it->toString());
        check(source["__schemaVersion__"] == 1 && source["__name__"] == it.key(),
              "embedded component has correct source version and identity");
    }
    QMap<QString, QVariantMap> legacy;
    QStringList groups = refs.keys();
    groups.prepend("Primary");
    for (const auto mode : {Oran7Theme::Light, Oran7Theme::Dark})
    {
        theme.setDarkMode(mode);
        theme.installIndexTheme(legacyPath);
        for (const auto& group : groups)
            legacy[group] = theme.property(group.toLatin1().constData()).toMap();
        check(legacy["Primary"].size() > 100 && legacy["Oran7ProgressSlider"].size() == 1
                  && !legacy["Oran7MusicPlaylistView"].contains("rowText"),
              "legacy bundle loaded from temporary files");
        theme.installIndexTheme({});
        for (const auto& group : groups)
        {
            const auto actual = theme.property(group.toLatin1().constData()).toMap();
            if (!keepsLegacyValues(actual, legacy.value(group)))
                std::fprintf(stderr, "FAIL group %s in mode %d\n", qPrintable(group), int(mode));
            check(keepsLegacyValues(actual, legacy.value(group)), "each legacy key, value and QVariant type is preserved");
            if (group != "Primary" && group != "Oran7MusicPlaylistView")
                check(actual.size() == legacy.value(group).size(), "component publishes exactly its existing fields");
        }
        const auto primary = theme.Primary();
        for (const auto& pair : {std::pair{"accent", "colorPrimary"}, {"surfacePanel", "colorBgContainer"},
                                 {"borderSubtle", "colorBorderSecondary"}, {"textPrimary", "colorTextPrimary"}})
        {
            if (primary.value(pair.first) != primary.value(pair.second))
                qWarning() << pair.first << primary.value(pair.first) << pair.second << primary.value(pair.second);
            check(primary.value(pair.first) == primary.value(pair.second), "semantic color resolves through compatible token");
        }
        const auto rows = theme.Oran7MusicPlaylistView();
        check(rows["rowText"] == rows[mode == Oran7Theme::Dark ? "rowTextDark" : "rowTextLight"],
              "built-in playlist publishes semantic and both compatibility modes");
    }

    auto incompatible = object(root + refs.value("Oran7ScreenCapture").toString());
    auto tokens = incompatible.value("__tokens__").toObject();
    tokens["ScaleMode"] = "Stretch";
    incompatible["__tokens__"] = tokens;
    const auto broken = temp.filePath("broken-capture.json");
    write(broken, QJsonDocument(incompatible).toJson());
    const auto ready = theme.Oran7ScreenCapture();
    theme.installComponentTheme("Oran7ScreenCapture", broken);
    check(theme.Oran7ScreenCapture() == ready, "invalid built-in enum cannot replace installed component");
    theme.saveComponentToken("Oran7ScreenCapture", "ScaleMode", "Stretch");
    check(theme.Oran7ScreenCapture() == ready, "invalid enum edit does not persist");
    tokens["ScaleMode"] = "Fill";
    incompatible["__tokens__"] = tokens;
    write(broken, QJsonDocument(incompatible).toJson());
    theme.installComponentTheme("Oran7ScreenCapture", broken);
    check(theme.Oran7ScreenCapture().value("ScaleMode") == "Fill", "valid versioned replacement is installed");
    theme.installComponentTheme("Oran7ScreenCapture", ":/config/themeJson/Oran7ScreenCapture.json");
    check(theme.Oran7ScreenCapture() == ready, "old embedded component reference remains usable");

    const auto layered = QJsonDocument::fromJson(R"({
        "__profileVersion__":3,"__component__":{"Oran7ScreenCapture":{
            "__modes__":{"light":{"ScaleMode":"Fill"},"dark":{"ScaleMode":"Fill"}}}}})").object();
    check(!layered.isEmpty() && profiles.saveThemeProfile("profile-layered", layered), "persist both mode overrides");
    theme.setCurrentThemeProfile("profile-layered");
    const auto layeredPath = temp.filePath("profile-layered.json");
    const auto before = bytes(layeredPath);
    theme.saveComponentToken("Oran7ScreenCapture", "ScaleMode", "Stretch");
    check(theme.Oran7ScreenCapture().value("ScaleMode") == "Fill" && bytes(layeredPath) == before,
          "validation rejects a common value even when both mode layers shadow it");
    theme.setCurrentThemeProfile("profile-default");
    theme.resetToDefaultTheme();
}

static void checkSourcePilot(Oran7Theme& theme, Oran7ThemeProfileManager& profiles, const QTemporaryDir& temp,
                             const QString& legacyPath, const QString& profileName)
{
    check(profiles.saveThemeProfile(profileName, {{"profileVersion", 2}, {"darkMode", 1}}),
          "pilot uses an isolated v3 profile");
    theme.setCurrentThemeProfile(profileName);
    theme.installIndexTheme(legacyPath);
    const auto darkLegacy = theme.Oran7MusicPlaylistView();
    theme.setDarkMode(Oran7Theme::Light);
    const auto lightLegacy = theme.Oran7MusicPlaylistView();
    auto legacyIndex = object(legacyPath);
    auto refs = legacyIndex.value("__component__").toObject();
    refs["Oran7MusicPlaylistView"] = "playlist.json";
    legacyIndex["__component__"] = refs;
    const auto pilot = object(BuiltInPlaylist);
    const auto componentPath = temp.filePath("playlist.json");
    const auto indexPath = temp.filePath("mixed-index.json");
    write(componentPath, QJsonDocument(pilot).toJson());
    write(indexPath, QJsonDocument(legacyIndex).toJson());

    theme.installIndexTheme(indexPath);
    const auto light = theme.Oran7MusicPlaylistView();
    check(keepsLegacyValues(light, lightLegacy) && light["rowText"] == light["rowTextLight"],
          "mixed source preserves every old light value and type and supplies the active semantic token");
    theme.setDarkMode(Oran7Theme::Dark);
    const auto dark = theme.Oran7MusicPlaylistView();
    check(keepsLegacyValues(dark, darkLegacy) && dark["rowText"] == dark["rowTextDark"],
          "mixed source preserves every old dark value and type");

    QSignalSpy playlist(&theme, &Oran7Theme::Oran7MusicPlaylistViewChanged);
    QSignalSpy caption(&theme, &Oran7Theme::Oran7CaptionBarChanged);
    theme.saveComponentToken("Oran7MusicPlaylistView", "rowAccent", "#315e91");
    check(playlist.count() == 1 && caption.isEmpty() &&
              theme.Oran7MusicPlaylistView()["rowAccent"].value<QColor>() == QColor("#315e91"),
          "common user overlay wins over dark source and only notifies the edited group");
    theme.saveComponentToken("Oran7MusicPlaylistView", "rowAccent", "#315e91");
    check(playlist.count() == 1, "versioned source keeps same-value edits silent");
    theme.saveComponentToken("Oran7MusicPlaylistView", "rowAccentDark", "#703849");
    theme.saveComponentToken("Oran7MusicPlaylistView", "listViewFontPixelSize", 18);
    theme.saveComponentToken("Oran7MusicPlaylistView", "listViewFontPixelSize", 21);
    theme.saveComponentToken("Oran7MusicPlaylistView", "listViewFontPixelSize", "18");
    check(theme.Oran7MusicPlaylistView()["listViewFontPixelSize"].toInt() == 18 &&
              profiles.loadThemeProfile(profileName)["__profileVersion__"].toInt() == 3,
          "out-of-range and mistyped writes do not replace valid v3 data");
    theme.reloadTheme();
    check(theme.Oran7MusicPlaylistView()["rowAccent"].value<QColor>() == QColor("#703849"),
          "legacy mode override wins after reload");
    theme.setDarkMode(Oran7Theme::Light);
    check(theme.Oran7MusicPlaylistView()["rowAccent"].value<QColor>() == QColor("#315e91"),
          "legacy dark override leaves the light value at the common override");

    const auto before = theme.Oran7MusicPlaylistView();
    auto invalidPilot = pilot;
    auto common = invalidPilot.value("__tokens__").toObject();
    common["rowSurface"] = "@rowSurface";
    invalidPilot["__tokens__"] = common;
    write(componentPath, QJsonDocument(invalidPilot).toJson());
    playlist.clear();
    theme.reloadTheme();
    check(theme.Oran7MusicPlaylistView() == before && playlist.isEmpty(),
          "cyclic defaults reject the bundle without clearing the last good snapshots");
    write(componentPath, QJsonDocument(pilot).toJson());

    auto overrides = profiles.loadThemeProfile(profileName);
    auto byGroup = overrides["__component__"].toObject();
    auto group = byGroup["Oran7MusicPlaylistView"].toObject();
    auto values = group["__tokens__"].toObject();
    values["listViewFontPixelSize"] = 999;
    values["rowText"] = "@missingPilotToken";
    values["unknownExtension"] = "$genColor(#ffffff)";
    group["__tokens__"] = values;
    byGroup["Oran7MusicPlaylistView"] = group;
    overrides["__component__"] = byGroup;
    check(profiles.saveThemeProfile(profileName, overrides), "store invalid v3 values and extension overrides");
    theme.reloadTheme();
    auto result = theme.Oran7MusicPlaylistView();
    check(result["listViewFontPixelSize"].toInt() == 16 &&
          result["rowText"].value<QColor>() == lightLegacy["rowTextLight"].value<QColor>() &&
          !result.contains("unknownExtension") && !result.contains("unknownExtension-1") &&
          profiles.loadThemeProfile(profileName) == overrides,
          "invalid values fall back and preserved unknown values are never evaluated");

    const auto foundationsPath = temp.filePath("foundations.json");
    const auto colorsPath = temp.filePath("colors.json");
    const auto splitIndexPath = temp.filePath("split-index.json");
    write(foundationsPath, R"({"__schemaVersion__":1,"__tokens__":{"radiusSmall":6,"durationFast":150,"durationNormal":250}})");
    write(colorsPath, R"({"__schemaVersion__":1,
      "__tokens__":{"accent":"#6979bc","textSecondary":"@textPrimary","surfacePanel":"@colorBgBase","borderSubtle":"@accent"},
      "__modes__":{"light":{"colorBgBase":"#f5f5f5","colorTextBase":"#000000","textPrimary":"@colorTextBase"},
                   "dark":{"colorBgBase":"#181818","colorTextBase":"#f3f3f3","textPrimary":"@colorTextBase"}}})");
    const QJsonObject splitIndex{{"__schemaVersion__", 1}, {"__foundations__", "foundations.json"},
                                 {"__colors__", "colors.json"}, {"__component__", refs}};
    write(splitIndexPath, QJsonDocument(splitIndex).toJson());
    theme.installIndexTheme(splitIndexPath);
    check(theme.Primary()["radiusSmall"].toInt() == 6 && theme.Primary()["durationFast"].toInt() == 150 &&
          theme.Primary()["textSecondary"].value<QColor>() == QColor("#000000"),
          "versioned index resolves split foundations and colors");
    theme.setDarkMode(Oran7Theme::Dark);
    check(theme.Primary()["surfacePanel"].value<QColor>() == QColor("#181818") &&
          theme.Primary()["textPrimary"].value<QColor>() == QColor("#f3f3f3"),
          "split index publishes effective colors and aliases in dark mode");
    const auto published = theme.Primary();
    write(foundationsPath, R"({"__schemaVersion__":1,"__tokens__":{"radiusSmall":6,"accent":"#ffffff"}})");
    theme.reloadTheme();
    check(theme.Primary() == published, "malformed split defaults cannot replace the published snapshot");
    theme.setCurrentThemeProfile("profile-default");
    check(theme.currentThemeProfile() == profileName && theme.Primary() == published,
          "switch with malformed sources leaves the selected profile and published values intact");
    theme.installIndexTheme({});
    check(theme.deleteThemeProfile(profileName), "remove isolated pilot profile");
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Oran7ThemeChecks");
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc");
    const auto args = QCoreApplication::arguments();
    if (args.size() == 5 && args.at(1) == "--check-profile-after-restart")
    {
        Oran7ThemeProfileManager reopened(args.at(2));
        Oran7AppConfigManager config(QDir(args.at(2)).filePath("config.json"), object(":/config/default.json"));
        Oran7Theme restarted(reopened, config);
        const bool reset = args.at(4) == "reset";
        const auto playlist = restarted.Oran7MusicPlaylistView();
        check(restarted.isDark() && restarted.currentThemeProfile() == args.at(3), "persisted profile and mode selected");
        check(restarted.Primary()["colorTextBase"].value<QColor>() == QColor(reset ? "#f3f3f3" : "#ddeeff") &&
                  playlist["listViewFontPixelSize"].toInt() == (reset ? 16 : 18) &&
                  playlist["rowAccentDark"].value<QColor>() == QColor(reset ? "#ead16c" : "#667788") &&
                  playlist["rowAccentLight"].value<QColor>() == QColor(reset ? "#92751b" : "#ffcc00"),
              "restarted theme loads committed values without preview");
        return 0;
    }
    QTemporaryDir temp;
    check(temp.isValid(), "temporary profile storage");
    Oran7ThemeProfileManager storage(temp.path());
    storage.initializeDefaultTheme();
    check(object(temp.filePath("profile-default.json")) == QJsonObject{{"__profileVersion__", 3}},
          "empty delta default profile");
    auto source = object(LegacyDefaults)["__component__"].toObject()["Oran7MainGUI"].toObject();
    auto style = source["__style__"].toObject();
    style["isDarkMode"] = false;
    style["futureExtension"] = QJsonArray{1, true, "text"};
    source["__style__"] = style;
    QJsonObject legacy{{"__component__", QJsonObject{{"Oran7MainGUI", source}}}};
    const auto legacyBytes = QJsonDocument(legacy).toJson();
    write(temp.filePath("profile-old.json"), legacyBytes);
    const auto delta = storage.loadThemeProfile("profile-old");
    check(delta["__profileVersion__"].toInt() == 3 && delta["__globals__"].toObject()["darkMode"].toInt() == 0, "legacy dark mode migrated");
    const auto migrated = delta["__component__"].toObject()["Oran7MainGUI"].toObject()["__tokens__"].toObject();
    check(migrated.size() == 1 && migrated["futureExtension"].isArray(),
          "full legacy reduced and unknown field retained");
    const auto backups = QDir(temp.path()).entryList({"profile-old.json.bak-*"}, QDir::Files);
    check(backups.size() == 1 && bytes(temp.filePath(backups.first())) == legacyBytes,
          "legacy original backup is exact");
    check(storage.loadThemeProfile("profile-old") == delta &&
              QDir(temp.path()).entryList({"*.bak-*"}, QDir::Files).size() == 1,
          "migration idempotent");
    check(!storage.saveThemeProfile("../escape", {}), "profile path traversal rejected");
    write(temp.filePath("profile-broken.json"), "{broken");
    check(!storage.saveThemeProfile("profile-broken", {}) && bytes(temp.filePath("profile-broken.json")) == "{broken",
          "corrupt source not overwritten");
    write(temp.filePath("profile-future.json"), "{\"profileVersion\":99}");
    check(storage.loadThemeProfile("profile-future").isEmpty() && !storage.saveThemeProfile("profile-future", {}),
          "future format protected");
    check(storage.duplicateThemeProfile("profile-old", "profile-copy") &&
              storage.loadThemeProfile("profile-copy") == delta,
          "copy delta");
    check(storage.exportThemeProfile("profile-old", temp.filePath("export.json")), "export delta");
    const auto imported = storage.importThemeProfile(temp.filePath("export.json"), "roundtrip");
    check(!imported.isEmpty() && storage.loadThemeProfile(imported) == delta, "import roundtrip");
    const QJsonObject typed{
        {"profileVersion", 2},
        {"__style__",
         QJsonObject{{"nativeBool", false}, {"nativeInt", 42}, {"nativeReal", 1.25}, {"literal", "  literal  "}}}};
    check(storage.saveThemeProfile("profile-typed", typed) && storage.loadThemeProfile("profile-typed")["__primary__"].toObject()["__tokens__"] == typed["__style__"],
          "native JSON types and whitespace roundtrip");
    checkMigrationFailures(temp, storage);
    checkModeDifference(temp, storage);

    auto& profiles = storage;
    const auto defaults = object(":/config/default.json");
    Oran7AppConfigManager config(temp.filePath("config.json"), defaults);
    Oran7Theme theme(profiles, config);
    const auto legacyPath = writeLegacySources(temp);
    checkProductionSources(theme, profiles, legacyPath, temp);
    checkProfileLifecycle(temp, storage, theme);
    const auto previous = theme.currentThemeProfile();
    const auto name = "profile-check-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    check(profiles.saveThemeProfile(name, {{"profileVersion", 2}, {"darkMode", 1}}),
          "isolated facade profile in temporary directory");
    theme.setCurrentThemeProfile(name);
    check(theme.currentThemeProfile() == name, "switch profile");
    QSignalSpy slider(&theme, &Oran7Theme::Oran7ProgressSliderChanged);
    QSignalSpy caption(&theme, &Oran7Theme::Oran7CaptionBarChanged);
    QSignalSpy primary(&theme, &Oran7Theme::PrimaryChanged);
    QSignalSpy gui(&theme, &Oran7Theme::Oran7MainGUIChanged);
    const auto playlistColors = theme.Oran7MusicPlaylistView();
    check(playlistColors.value("rowSurfaceDark").value<QColor>().isValid() &&
              playlistColors.value("rowAccentLight").value<QColor>().isValid(),
          "playlist semantic colors evaluate from own component defaults");
    check(theme.Oran7MainGUI().value("backgroundParallaxEnabled").toBool()
        && theme.Oran7MainGUI().value("backgroundParallaxAmplitude").toInt() == 30,
        "legacy profiles inherit shared parallax defaults");
    check(theme.Oran7ScreenCapture().value("workspaceSkin").toString() == "default",
        "existing theme profiles inherit default studio skin");
    const auto originalMainColor = theme.Oran7MainGUI().value("themeColor");
    theme.installComponentToken("Oran7MainGUI", "themeColor", "#12ab34");
    check(theme.Oran7MusicPlaylistView() == playlistColors,
          "main GUI accent changes do not recolor playlist component");
    theme.installComponentToken("Oran7MainGUI", "themeColor", originalMainColor);
    gui.clear();
    check(theme.Primary().value("colorTextBase").value<QColor>().isValid(), "base color typed");
    check(theme.Oran7ProgressSlider().size() == 1, "component map contains no unrelated global tokens");
    theme.installComponentToken("Oran7ProgressSlider", "moveThreshold", 7);
    check(slider.count() == 1 && caption.count() == 0 && primary.count() == 0, "local update notifies only own group");
    theme.installComponentToken("Oran7ProgressSlider", "moveThreshold", 7);
    check(slider.count() == 1, "identical local update silent");
    theme.installComponentTheme("Oran7ProgressSlider", temp.filePath("legacy/Oran7ProgressSlider.json"));
    theme.installComponentToken("Oran7ProgressSlider", "flag", false);
    const int flagCount = slider.count();
    theme.installComponentToken("Oran7ProgressSlider", "flag", 0);
    check(slider.count() == flagCount + 1 && theme.Oran7ProgressSlider()["flag"].metaType().id() == QMetaType::Int,
          "type change not suppressed by QVariant coercion");
    theme.installComponentToken("Oran7ProgressSlider", "flag", false);
    theme.installComponentToken("Oran7ProgressSlider", "real", 1.25);
    theme.installComponentToken("Oran7ProgressSlider", "label", "  text  ");
    auto values = theme.Oran7ProgressSlider();
    check(values["flag"].metaType().id() == QMetaType::Bool && !values["flag"].toBool(), "native false preserved");
    check(values["real"].toDouble() == 1.25 && values["label"].toString() == "  text  ",
          "real and literal spaces preserved");
    theme.installComponentToken("Oran7ProgressSlider", "aForward", "@zValue");
    theme.installComponentToken("Oran7ProgressSlider", "zValue", 12);
    check(theme.Oran7ProgressSlider()["aForward"].toInt() == 12, "forward reference independent of key sort");
    theme.installComponentToken("Oran7ProgressSlider", "formula", "$multiply(3, $multiply(2, 4))");
    check(theme.Oran7ProgressSlider()["formula"].toDouble() == 24, "nested expression");
    theme.installComponentToken("Oran7ProgressSlider", "moveThreshold", "$invalid(1)");
    check(theme.Oran7ProgressSlider()["moveThreshold"].toInt() == 2, "invalid expression falls back to source default");
    theme.installComponentToken("Oran7ProgressSlider", "cycleA", "@cycleB");
    theme.installComponentToken("Oran7ProgressSlider", "cycleB", "@cycleA");
    check(!theme.Oran7ProgressSlider().contains("cycleA") && !theme.Oran7ProgressSlider().contains("cycleB"),
          "cycles safely rejected");
    theme.reloadTheme();
    theme.installComponentToken("Oran7ProgressSlider", "moveThreshold", "$invalid(1)");
    check(theme.Oran7ProgressSlider()["moveThreshold"].toString() == "2", "versioned evaluator falls back on bad expression");
    theme.saveComponentToken("Oran7ProgressSlider", "moveThreshold", 11);
    check(profiles.loadThemeProfile(name)["__component__"].toObject().size() == 1,
          "save contains only edited component");
    theme.saveComponentToken("Oran7MainGUI", "backgroundParallaxEnabled", false);
    theme.saveComponentToken("Oran7MainGUI", "backgroundParallaxAmplitude", 17);
    theme.reloadTheme();
    check(!theme.Oran7MainGUI().value("backgroundParallaxEnabled").toBool()
        && theme.Oran7MainGUI().value("backgroundParallaxAmplitude").toInt() == 17,
        "parallax switch and amplitude restore from isolated saved profile");

    const QColor savedColor(20, 40, 60, 128);
    theme.saveComponentToken("Oran7MainGUI", "themeColor", savedColor);
    theme.saveIndexToken("durationFast", 88);
    theme.reloadTheme();
    check(theme.Oran7MainGUI()["themeColor"].value<QColor>() == savedColor &&
              theme.Oran7MainGUI()["backgroundParallaxEnabled"].metaType().id() == QMetaType::Bool &&
              !theme.Oran7MainGUI()["backgroundParallaxEnabled"].toBool(),
          "color alpha and false persist through actual facade");
    check(theme.Primary()["durationFast"].toInt() == 88,
          "global overrides applied before first calculation");
    check(theme.Oran7ProgressSlider()["moveThreshold"].toInt() == 11 && !theme.Oran7ProgressSlider().contains("flag"),
          "reload restores saved values and clears transient state");
    slider.clear();
    caption.clear();
    primary.clear();
    gui.clear();
    theme.reloadTheme();
    check(slider.isEmpty() && caption.isEmpty() && primary.isEmpty() && gui.isEmpty(),
          "no-op full reload sends zero group notifications");
    theme.setDarkMode(Oran7Theme::Light);
    check(!theme.isDark() && !theme.Oran7MainGUI()["isDarkMode"].toBool() &&
              theme.Primary()["colorTextBase"].value<QColor>() == QColor("#000000"),
          "dark mode has one authority including compatibility alias");
    check(slider.isEmpty(), "global dark change skips unaffected group");
    theme.setDarkMode(Oran7Theme::System);
    check(theme.isDark() == (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark),
          "system follow uses Qt style hints");
    theme.reloadTheme();
    check(theme.darkMode() == Oran7Theme::System, "mode restored from profile");
    QSignalSpy darkSignals(&theme, &Oran7Theme::isDarkChanged);
    QMetaObject::invokeMethod(QGuiApplication::styleHints(), "colorSchemeChanged", Qt::DirectConnection,
                              Q_ARG(Qt::ColorScheme, QGuiApplication::styleHints()->colorScheme()));
    check(darkSignals.isEmpty(), "unchanged system scheme sends no effective dark notification");
    theme.installIndexToken("durationFast", "99");
    check(theme.Primary()["durationFast"].toInt() == 99, "transient global overrides saved value");
    theme.reloadTheme();
    check(theme.Primary()["durationFast"].toInt() == 88, "reload clears transient global override");
    theme.installThemePrimaryRadiusBase(9);
    check(theme.Primary()["radiusPrimary"].toInt() > 0, "radius generator spelling and reference");

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ORAN7_QML_IMPORT_ROOT));
    QQmlEngine::setObjectOwnership(&theme, QQmlEngine::CppOwnership);
    engine.rootContext()->setContextProperty("testTheme", &theme);
    QQmlComponent component(&engine);
    component.setData(R"(import QtQml
QtObject {
 property int evaluations: 0
 property var theme: testTheme
 property var threshold: { evaluations++; return theme.Oran7ProgressSlider.moveThreshold }
})",
                      QUrl("qrc:/ThemeFixture.qml"));
    std::unique_ptr<QObject> qml(component.create());
    if (!qml)
        for (const auto& e : component.errors())
            std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
    check(bool(qml), "real QML binding fixture");
    const int before = qml->property("evaluations").toInt();
    theme.saveComponentToken("Oran7ProgressSlider", "moveThreshold", 15);
    check(qml->property("threshold").toInt() == 15 && qml->property("evaluations").toInt() == before + 1,
          "real QML evaluates changed group once");
    theme.saveComponentToken("Oran7ProgressSlider", "moveThreshold", 15);
    check(qml->property("evaluations").toInt() == before + 1, "real QML unchanged group not reevaluated");
    const auto extraSource = QJsonObject{{"__style__", QJsonObject{{"moveThreshold", "2"}, {"newDefault", 42}}}};
    write(temp.filePath("component.json"), QJsonDocument(extraSource).toJson());
    theme.installComponentTheme("Oran7ProgressSlider", temp.filePath("component.json"));
    check(theme.Oran7ProgressSlider()["newDefault"].toInt() == 42 &&
              theme.Oran7ProgressSlider()["moveThreshold"].toInt() == 15,
          "new source tokens remain available beside profile delta");
    theme.reloadTheme();
    check(theme.createThemeProfile(name + "-clean"), "create clean profile");
    theme.setCurrentThemeProfile(name + "-clean");
    check(theme.Oran7ProgressSlider()["moveThreshold"].toInt() == 2 && theme.Primary()["durationFast"].toInt() == 100,
          "switch clears previous global and component overrides");
    theme.setCurrentThemeProfile(name);
    check(theme.Oran7ProgressSlider()["moveThreshold"].toInt() == 15, "switch back restores override");
    const auto legacyCollision = R"json({"profileVersion":2,"__component__":{"Oran7MusicLyricsIsland":{
        "__init__":{"__vars__":{"floatingBackdropColor":"$genColor(#000000)"}},
        "__style__":{"floatingBackdropColor":"#111111","floatingOpacity":0.7}}}})json";
    const auto collisionName = name + "-legacy";
    write(temp.filePath(collisionName + ".json"), legacyCollision);
    theme.setCurrentThemeProfile(collisionName);
    check(theme.Oran7MusicLyricsIsland()["floatingBackdropColor"].value<QColor>() == QColor("#111111"),
          "v2 style wins over same-name variable at migration");
    QSignalSpy island(&theme, &Oran7Theme::Oran7MusicLyricsIslandChanged);
    theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingBackdropColor", "$genColor(#3a86ff)");
    const auto islandColors = theme.Oran7MusicLyricsIsland();
    check(islandColors["floatingBackdropColor"].value<QColor>() == QColor("#3a86ff") &&
              islandColors["floatingBackdropColor-6"].value<QColor>().isValid() && island.count() == 1,
          "confirmed picker color replaces legacy style duplicate and regenerates visible palette immediately");
    const auto savedIsland = profiles.loadThemeProfile(collisionName)["__component__"].toObject()["Oran7MusicLyricsIsland"].toObject();
    check(savedIsland["__tokens__"].toObject()["floatingBackdropColor"] == "$genColor(#3a86ff)" &&
              savedIsland["__tokens__"].toObject()["floatingOpacity"].toDouble() == 0.7,
          "color save removes only shadowing token and preserves other style settings");
    theme.reloadTheme();
    check(theme.Oran7MusicLyricsIsland() == islandColors, "selected backdrop color survives theme reload");
    check(theme.deleteThemeProfile(collisionName) && theme.currentThemeProfile() == "profile-default",
          "deleting active profile falls back");
    theme.deleteThemeProfile(name);
    theme.deleteThemeProfile(name + "-clean");
    checkSourcePilot(theme, profiles, temp, legacyPath, name + "-pilot");
    theme.setCurrentThemeProfile(previous);
    std::printf("PASS: %d theme checks\n", checks);
}
