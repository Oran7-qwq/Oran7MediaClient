#include "Oran7Theme.h"
#include "Oran7ThemeProfileManager.h"
#include <QGuiApplication>
#include <QFontDatabase>
#include <QStyleHints>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QQmlEngine>
#include <QQmlComponent>
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

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Oran7ThemeChecks");
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc");
    QTemporaryDir temp;
    check(temp.isValid(), "temporary profile storage");
    Oran7ThemeProfileManager storage(temp.path());
    storage.initializeDefaultTheme(nullptr);
    check(object(temp.filePath("profile-default.json")) == QJsonObject{{"profileVersion", 2}},
          "empty delta default profile");
    auto source = object(":/config/themeJson/Oran7MainGUI.json");
    auto style = source["__style__"].toObject();
    style["isDarkMode"] = false;
    style["futureExtension"] = QJsonArray{1, true, "text"};
    source["__style__"] = style;
    QJsonObject legacy{{"__component__", QJsonObject{{"Oran7MainGUI", source}}}};
    const auto legacyBytes = QJsonDocument(legacy).toJson();
    write(temp.filePath("profile-old.json"), legacyBytes);
    const auto delta = storage.loadThemeProfile("profile-old");
    check(delta["profileVersion"].toInt() == 2 && delta["darkMode"].toInt() == 0, "legacy dark mode migrated");
    const auto migrated = delta["__component__"].toObject()["Oran7MainGUI"].toObject()["__style__"].toObject();
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
    check(storage.saveThemeProfile("profile-typed", typed) && storage.loadThemeProfile("profile-typed") == typed,
          "native JSON types and whitespace roundtrip");

    auto& theme = Oran7Theme::instance();
    auto& profiles = Oran7ThemeProfileManager::instance();
    const auto previous = theme.currentThemeProfile();
    const auto name = "profile-check-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    check(profiles.saveThemeProfile(name, {{"profileVersion", 2}, {"darkMode", 1}}),
          "isolated facade profile in checks directory");
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
    theme.saveComponentToken("Oran7ProgressSlider", "moveThreshold", 11);
    check(profiles.loadThemeProfile(name)["__component__"].toObject().size() == 1,
          "save contains only edited component");
    const QColor savedColor(20, 40, 60, 128);
    theme.saveComponentToken("Oran7ProgressSlider", "colorToken", savedColor);
    theme.saveComponentToken("Oran7ProgressSlider", "persistedFalse", false);
    theme.saveIndexToken("durationFast", 88);
    theme.saveIndexToken("nativeBool", true);
    theme.reloadTheme();
    check(theme.Oran7ProgressSlider()["colorToken"].value<QColor>() == savedColor &&
              theme.Oran7ProgressSlider()["persistedFalse"].metaType().id() == QMetaType::Bool &&
              !theme.Oran7ProgressSlider()["persistedFalse"].toBool(),
          "color alpha and false persist through actual facade");
    check(theme.Primary()["durationFast"].toInt() == 88 &&
              theme.Primary()["nativeBool"].metaType().id() == QMetaType::Bool,
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
    QQmlComponent component(&engine);
    component.setData(R"(import QtQml
import Oran7UI.Impl 1.0
QtObject {
 property int evaluations: 0
 property var theme: Oran7Theme
 property var threshold: { evaluations++; return Oran7Theme.Oran7ProgressSlider.moveThreshold }
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
    check(theme.Oran7ProgressSlider()["moveThreshold"].toInt() == 2 && !theme.Primary().contains("nativeBool"),
          "switch clears previous global and component overrides");
    theme.setCurrentThemeProfile(name);
    check(theme.Oran7ProgressSlider()["moveThreshold"].toInt() == 15, "switch back restores override");
    check(theme.deleteThemeProfile(name) && theme.currentThemeProfile() == "profile-default",
          "deleting active profile falls back");
    theme.deleteThemeProfile(name + "-clean");
    theme.setCurrentThemeProfile(previous);
    std::printf("PASS: %d theme checks\n", checks);
}
