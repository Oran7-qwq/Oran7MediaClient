#include "Oran7UiPreferences.h"
#include "Oran7AppConfigManager.h"
#include "Oran7Theme.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QFontDatabase>
#include <QXmlStreamReader>
#include <QDirIterator>
#include <QRegularExpression>
#include <QJsonArray>
#include <QSet>
#include <QProcess>
#include <QJSValue>
#include <memory>
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
static QJsonObject json(const QString& path)
{
    QFile f(path);
    check(f.open(QIODevice::ReadOnly), "read fixture");
    return QJsonDocument::fromJson(f.readAll()).object();
}
static void write(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    check(f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(), "write fixture");
}
static QQuickItem* item(QQuickItem* root, const QString& name)
{
    if (root->objectName() == name)
        return root;
    for (auto* child : root->childItems())
        if (auto* found = item(child, name))
            return found;
    return nullptr;
}
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc");
    QTemporaryDir directory;
    check(directory.isValid(), "temporary storage");
    const auto defaults = json(QStringLiteral(APP_ROOT) + "/config/default.json");
    const auto path = directory.filePath("config.json");
    // 校验目录完整性：实际 QML 的静态翻译入口必须有非空英文译文。
    QFile catalog(QStringLiteral(APP_ROOT) + "/translations/oran7_en_US.ts");
    check(catalog.open(QIODevice::ReadOnly), "translation catalog readable");
    QXmlStreamReader xml(&catalog);
    QSet<QString> translatedSources;
    QString source;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"source")
            source = xml.readElementText();
        else if (xml.isStartElement() && xml.name() == u"translation")
        {
            const auto translated = xml.readElementText();
            check(!translated.isEmpty(), "finished English entry");
            translatedSources.insert(source);
            const QRegularExpression parameter("%[1-9][0-9]*");
            auto parameters = [&](const QString& text)
            {
                QSet<QString> out;
                auto matches = parameter.globalMatch(text);
                while (matches.hasNext())
                    out.insert(matches.next().captured());
                return out;
            };
            check(parameters(source) == parameters(translated), "translation placeholders preserved");
        }
    }
    check(!xml.hasError(), "valid TS document");
    // 使用 Qt 自己的解析器核对真实 QML 翻译入口，避免正则漏掉转义和多行文案。
    const auto extractedPath = directory.filePath("extracted.ts");
    QProcess extractor;
    extractor.start(QStringLiteral(ORAN7_LUPDATE_TOOL),
                    {QStringLiteral(APP_ROOT) + "/main.qml", QStringLiteral(APP_ROOT) + "/src/qml", "-locations",
                     "none", "-no-obsolete", "-ts", extractedPath});
    check(extractor.waitForFinished(30000) && extractor.exitStatus() == QProcess::NormalExit &&
              extractor.exitCode() == 0,
          "Qt parser extracts QML translations");
    QFile extracted(extractedPath);
    check(extracted.open(QIODevice::ReadOnly), "extracted catalog readable");
    QXmlStreamReader entries(&extracted);
    int extractedCount = 0;
    while (!entries.atEnd())
    {
        entries.readNext();
        if (entries.isStartElement() && entries.name() == u"source")
        {
            const auto source = entries.readElementText();
            check(translatedSources.contains(source), "actual QML source has English translation");
            ++extractedCount;
        }
    }
    check(!entries.hasError() && extractedCount > 200, "QML catalog audit covers real sources");
    Oran7AppConfigManager config(path, defaults);
    check(config.snapshot().appearance.language == "zh_CN", "Chinese default for legacy config");
    QJsonObject normalized;
    auto invalid = defaults;
    auto appearance = invalid["appearance"].toObject();
    appearance["language"] = "ja_JP";
    appearance["fontPixelSize"] = 999;
    invalid["appearance"] = appearance;
    check(Oran7AppConfig::normalize(invalid, defaults, &normalized), "normalize language preferences");
    check(Oran7AppConfig::fromJson(normalized).appearance.language == "zh_CN" &&
              Oran7AppConfig::fromJson(normalized).appearance.fontPixelSize == 16,
          "unsupported language and font size fall back");
    {
        Oran7UiPreferences preferences(config);
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(ORAN7_QML_IMPORT_ROOT));
        preferences.attachEngine(&engine);
        engine.rootContext()->setContextProperty("Oran7UiPreferences", &preferences);
        QQmlComponent keyboardFixture(&engine);
        keyboardFixture.setData(
            "import QtQml\nQtObject { signal enterKeyPressed(); signal keyCombinationTriggered(string combination) }",
            QUrl());
        std::unique_ptr<QObject> keyboard(keyboardFixture.create());
        check(bool(keyboard), "isolated keyboard fixture");
        engine.rootContext()->setContextProperty("globalEventFilter", keyboard.get());
        qmlRegisterSingletonType(
            QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/src/qml/Settings/GlobalSettings/Oran7MainUiSetting.qml"),
            "Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting");
        QQmlComponent binding(&engine);
        binding.setData(R"(import QtQuick
Item {
 property string caption: qsTranslate("Oran7Ui", "录屏工作台")
 property string unknown: qsTranslate("Oran7Ui", "Untranslated fixture")
 property string family: Oran7UiPreferences.font.family
 property int pixels: Oran7UiPreferences.font.pixelSize
 Text { objectName: "liveFontText"; font: Oran7UiPreferences.font; text: "Aa" }
})",
                        QUrl("qrc:/LanguageBinding.qml"));
        std::unique_ptr<QObject> object(binding.create());
        check(bool(object), "real QML translation binding");
        check(object->property("caption").toString() == QString::fromUtf8("录屏工作台"), "Chinese source display");
        QSignalSpy changes(&preferences, &Oran7UiPreferences::languageChanged);
        check(preferences.selectLanguage("en_US"), "select English");
        check(object->property("caption").toString() == "Recording studio",
              "existing binding retranslates immediately");
        check(object->property("unknown").toString() == "Untranslated fixture",
              "missing translation falls back to source");
        check(preferences.translate(QString::fromUtf8("录制参数已保存")) == "Recording settings saved",
              "backend status source translates in UI context");
        check(preferences.selectLanguage("en_US") && changes.count() == 1, "same language is silent");
        check(!preferences.selectLanguage("fr_FR") && preferences.language() == "en_US",
              "reject unsupported language without changing live state");
        check(json(path)["appearance"].toObject()["language"].toString() == "en_US", "language persisted");
        check(!preferences.selectFont("nonexistent fixture font", 16), "reject missing font");
        const auto family = preferences.fontFamilies().first();
        check(preferences.selectFont(family, 18), "select installed font and size");
        QCoreApplication::processEvents();
        check(object->property("family").toString().startsWith(family) && QGuiApplication::font().pixelSize() == 18,
              "live font binding updates");
        check(object->property("pixels").toInt() == 18 &&
                  object->findChild<QObject*>("liveFontText")->property("font").value<QFont>().pixelSize() == 18,
              "existing QML text receives font size notification without recreation");
        check(preferences.selectFont(family, 16), "restore size for UI layout fixture");
        QQuickWindow window;
        window.resize(280, 750);
        window.setColor(QColor("#181818"));
        const auto settingsPath =
            QStringLiteral(APP_ROOT) + "/src/qml/Settings/SettingWindows/Oran7MainUiSettingWindow.qml";
        QFile settingsFile(settingsPath);
        check(settingsFile.open(QIODevice::ReadOnly), "read actual settings panel");
        auto settingsBytes = settingsFile.readAll();
        // 软件离屏不支持 GPU 阴影与渐变；仅在 fixture 内关闭，保留实际控件及布局。
        settingsBytes.replace("layer.enabled: true", "layer.enabled: false");
        settingsBytes.replace("gradientMaskEnabled: true", "gradientMaskEnabled: false");
        settingsBytes.replace("preferences: root.preferences",
                              "preferences: root.preferences; headingGradientEnabled: false");
        QQmlComponent settings(&engine);
        settings.setData(settingsBytes, QUrl::fromLocalFile(settingsPath));
        std::unique_ptr<QObject> panel(settings.create());
        if (!panel)
            for (const auto& e : settings.errors())
                std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
        check(bool(panel), "actual main UI settings panel loads");
        auto* quick = qobject_cast<QQuickItem*>(panel.get());
        check(quick, "settings item");
        quick->setParentItem(window.contentItem());
        panel->setProperty("savedNormalX", 14);
        panel->setProperty("savedNormalY", 10);
        panel->setProperty("animationDuration", 1);
        panel->setProperty("transitionState", "Open");
        panel->setProperty("revealProgress", 1.0);
        window.show();
        QTest::qWait(150);
        auto* group = item(quick, "uiPreferencesGroup");
        check(group, "preferences use their own expandable group");
        check(!group->property("expanded").toBool(), "group initially collapsed like other settings sections");
        group->setProperty("expanded", true);
        QElapsedTimer expanded;
        expanded.start();
        while ((!item(quick, "uiLanguageChoice") || quick->height() < 400) && expanded.elapsed() < 3000)
            QTest::qWait(20);
        check(quick->isVisible() && quick->height() > 350, "expanded settings content participates in layout");
        QTest::qWait(260);
        auto* choice = item(quick, "uiLanguageChoice");
        check(choice, "language picker in actual settings panel");
        check(choice->property("currentIndex").toInt() == 1, "picker restores English selection");
        auto* fontSize = item(quick, "uiFontSizeChoice");
        check(fontSize, "font size uses existing slider group");
        auto* expansion = item(quick, "uiPreferencesExpansion");
        check(fontSize->mapToItem(expansion, QPointF(0, fontSize->height())).y() <= expansion->height() + 1,
              "slider stays inside expanded content");

        check(QMetaObject::invokeMethod(fontSize, "committed", Q_ARG(double, 18.0), Q_ARG(int, 0), Q_ARG(double, 0.5)),
              "slider commits font size");
        check(preferences.fontPixelSize() == 18, "slider saves font size preference");
        check(object->property("pixels").toInt() == 18, "slider updates existing QML font immediately");
        auto* sizeSlider = item(fontSize, "uiFontSizeChoiceSlider");
        check(sizeSlider, "actual base size slider");
        QTest::mouseClick(
            &window, Qt::LeftButton, Qt::NoModifier,
            sizeSlider->mapToScene(QPointF(sizeSlider->width() * 0.72, sizeSlider->height() / 2)).toPoint());
        check(preferences.fontPixelSize() >= 19 && object->property("pixels").toInt() == preferences.fontPixelSize(),
              "actual mouse release saves and applies base size immediately");
        QMetaObject::invokeMethod(fontSize, "committed", Q_ARG(double, 16.0), Q_ARG(int, 0), Q_ARG(double, 0.5));

        check(window.grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/settings-en.png"),
              "English settings screenshot");
        auto* languagePopup = choice->property("popup").value<QObject*>();
        check(languagePopup, "custom language popup");
        check(QMetaObject::invokeMethod(languagePopup, "open"), "open animated language popup");
        QTest::qWait(240);
        check(languagePopup->property("visible").toBool(), "language popup opens");
        auto* settingState =
            engine.singletonInstance<QObject*>(qmlTypeId("Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting"));
        check(settingState && settingState->property("activeOtherItemCount").toInt() == 1,
              "popup registers focused setting item");
        check(window.grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/language-popup-en.png"),
              "language popup screenshot");
        auto* firstOption = item(window.contentItem(), "uiLanguageChoiceOption0");
        check(firstOption, "actual language option delegate");
        QTest::mouseClick(
            &window, Qt::LeftButton, Qt::NoModifier,
            firstOption->mapToScene(QPointF(firstOption->width() / 2, firstOption->height() / 2)).toPoint());
        QTest::qWait(150);
        check(preferences.language() == "zh_CN" && !languagePopup->property("visible").toBool(),
              "mouse selection applies language and closes popup");
        QMetaObject::invokeMethod(choice, "activated", Q_ARG(int, 1));
        auto* fontChoice = item(quick, "uiFontChoice");
        auto* fontPopup = fontChoice->property("popup").value<QObject*>();
        check(fontPopup && QMetaObject::invokeMethod(fontPopup, "open"), "font popup opens with previews");
        QTest::qWait(240);
        check(window.grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/font-popup-en.png"),
              "font popup screenshot");
        group->setProperty("expanded", false);
        QTest::qWait(260);
        check(!fontPopup->property("visible").toBool(), "collapsing preferences closes floating font menu");
        check(!quick->property("preferencesPopupOpen").toBool(), "popup focus bookkeeping released");
        check(settingState->property("activeOtherItemCount").toInt() == 0, "popup counter released after collapse");
        group->setProperty("expanded", true);
        QTest::qWait(260);
        check(QMetaObject::invokeMethod(choice, "activated", Q_ARG(int, 0)), "activate Chinese in actual picker");
        QTest::qWait(30);
        check(preferences.language() == "zh_CN" &&
                  object->property("caption").toString() == QString::fromUtf8("录屏工作台"),
              "picker switches existing UI to Chinese");
        check(window.grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/settings-zh.png"),
              "Chinese settings screenshot");
        check(QMetaObject::invokeMethod(choice, "activated", Q_ARG(int, 1)) && preferences.language() == "en_US",
              "picker switches back to English");
        // 加载实际音乐面板；只替换 Client 边界，不启动播放器或读取账号。
        QQmlComponent clientFixture(&engine);
        clientFixture.setData("import QtQml\nQtObject { property QtObject musicQueue: QtObject { property bool roamFilterLiked:false } }", QUrl());
        std::unique_ptr<QObject> fakeClient(clientFixture.create());
        check(bool(fakeClient), "isolated music queue boundary");
        qmlRegisterSingletonInstance("Client", 1, 0, "Client", fakeClient.get());
        const auto musicPath = QStringLiteral(APP_ROOT) + "/src/qml/Settings/SettingWindows/Oran7MusicPlayerSettingWindow.qml";
        QFile musicFile(musicPath);
        check(musicFile.open(QIODevice::ReadOnly), "read actual music settings panel");
        auto musicBytes = musicFile.readAll();
        musicBytes.replace("layer.enabled: true", "layer.enabled: false");
        musicBytes.replace("gradientMaskEnabled: true", "gradientMaskEnabled: false");
        QQmlComponent musicComponent(&engine);
        musicComponent.setData(musicBytes, QUrl::fromLocalFile(musicPath));
        std::unique_ptr<QObject> musicPanel(musicComponent.create());
        if (!musicPanel) for (const auto& e : musicComponent.errors()) std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
        check(bool(musicPanel), "actual music settings loads with Client import");
        auto* musicItem = qobject_cast<QQuickItem*>(musicPanel.get());
        check(musicItem, "music panel is an item");
        QQuickWindow musicWindow;
        musicWindow.resize(350, 1800);
        musicItem->setParentItem(musicWindow.contentItem());
        musicPanel->setProperty("savedNormalX", 0);
        musicPanel->setProperty("transitionState", "Open");
        musicPanel->setProperty("revealProgress", 1.0);
        auto* roamGroup = item(musicItem, "neteaseRoamSettingsGroup");
        check(roamGroup && roamGroup->setProperty("expand", true), "expand actual roam settings");
        musicWindow.show();
        QTest::qWait(350);
        auto* roamSwitch = item(musicItem, "roamFilterLikedSwitch");
        check(roamSwitch && !roamSwitch->property("checked").toBool(), "roam switch initial backend binding");
        const auto lyricsBefore = Oran7Theme::instance().property("Oran7MusicLyricsIsland");
        QTest::mouseClick(&musicWindow, Qt::LeftButton, Qt::NoModifier,
                         roamSwitch->mapToScene(QPointF(roamSwitch->width()/2, roamSwitch->height()/2)).toPoint());
        auto* fakeQueue = fakeClient->property("musicQueue").value<QObject*>();
        check(fakeQueue && fakeQueue->property("roamFilterLiked").toBool(), "actual switch click sets queue preference");
        check(Oran7Theme::instance().property("Oran7MusicLyricsIsland") == lyricsBefore, "roam switch leaves lyrics theme unchanged");
        fakeQueue->setProperty("roamFilterLiked", false);
        check(!roamSwitch->property("checked").toBool(), "backend preference changes resynchronize switch after click");
        roamGroup->setProperty("expand", false);
        auto* playlistGroup = item(musicItem, "musicListViewSettings");
        check(playlistGroup && playlistGroup->setProperty("expand", true), "expand playlist appearance settings");
        auto& playlistTheme = Oran7Theme::instance();
        const auto mainGuiBefore = playlistTheme.Oran7MainGUI();
        playlistTheme.installComponentToken("Oran7MusicPlaylistView", "rowBackgroundColor", "#234567");
        QTest::qWait(350);
        auto* backgroundChoice = item(musicItem, "playlistBackgroundSettingsColorChoice");
        check(backgroundChoice && backgroundChoice->property("count").toInt() == 2,
              "playlist background popup contains only picker and ivory preset");
        QTest::mouseClick(&musicWindow, Qt::LeftButton, Qt::NoModifier,
                         backgroundChoice->mapToScene(QPointF(backgroundChoice->width()/2, backgroundChoice->height()/2)).toPoint());
        QTest::qWait(240);
        auto* ivoryOption = item(musicWindow.contentItem(), "playlistBackgroundSettingsColorChoiceOption1");
        check(ivoryOption && ivoryOption->property("text").toString() == "Ivory", "background preset translates to Ivory");
        QTest::mouseClick(&musicWindow, Qt::LeftButton, Qt::NoModifier,
                         ivoryOption->mapToScene(QPointF(ivoryOption->width()/2, ivoryOption->height()/2)).toPoint());
        QTest::qWait(160);
        check(playlistTheme.Oran7MusicPlaylistView().value("rowBackgroundColor").value<QColor>() == QColor("#ede7db"),
              "ivory click commits independent playlist background token");
        check(playlistTheme.Oran7MainGUI() == mainGuiBefore, "playlist background leaves main GUI settings unchanged");
        auto* textGroup = item(musicItem, "playlistTextSettings");
        auto* textChoice = item(musicItem, "playlistTextSettingsColorChoice");
        check(textGroup && textChoice && textGroup->property("checkedColor").value<QColor>() == QColor("#242a31"),
              "text settings preview uses actual playlist text instead of legacy generated color");
        check(QMetaObject::invokeMethod(textGroup, "colorReady", Q_ARG(QColor, QColor("#315e77"))),
              "custom text color submitted through actual settings group");
        check(playlistTheme.Oran7MusicPlaylistView().value("rowTextColor").value<QColor>() == QColor("#315e77"),
              "actual text color settings persist new authoritative token");
        QTest::mouseClick(&musicWindow, Qt::LeftButton, Qt::NoModifier,
                         textChoice->mapToScene(QPointF(textChoice->width()/2, textChoice->height()/2)).toPoint());
        QTest::qWait(240);
        auto* originalTextOption = item(musicWindow.contentItem(), "playlistTextSettingsColorChoiceOption1");
        check(originalTextOption && originalTextOption->property("text").toString() == "Original playlist palette",
              "actual displayed playlist palette is available as a named preset");
        QTest::mouseClick(&musicWindow, Qt::LeftButton, Qt::NoModifier,
                         originalTextOption->mapToScene(QPointF(originalTextOption->width()/2, originalTextOption->height()/2)).toPoint());
        QTest::qWait(160);
        check(playlistTheme.Oran7MusicPlaylistView().value("rowTextColor").toString().isEmpty() &&
                  textGroup->property("checkedColor").value<QColor>() == QColor("#242a31"),
              "original palette click restores original title and secondary text policy");
        check(playlistTheme.Oran7MainGUI() == mainGuiBefore, "text settings do not modify main GUI colors");
        auto* accentGroup = item(musicItem, "playlistAccentSettings");
        check(accentGroup && accentGroup->property("checkedColor").value<QColor>() == QColor("#22d3ee"),
              "playlist accent setting previews brighter cyan default");
        const auto backgroundBeforeAccent = playlistTheme.Oran7MusicPlaylistView().value("rowBackgroundColor");
        const auto textBeforeAccent = playlistTheme.Oran7MusicPlaylistView().value("rowTextColor");
        check(QMetaObject::invokeMethod(accentGroup, "enterOfTextFiled", Q_ARG(QString, "#a78bfa")),
              "accent setting accepts typed color");
        check(playlistTheme.Oran7MusicPlaylistView().value("rowAccentColor").value<QColor>() == QColor("#a78bfa") &&
                  playlistTheme.Oran7MusicPlaylistView().value("rowBackgroundColor") == backgroundBeforeAccent &&
                  playlistTheme.Oran7MusicPlaylistView().value("rowTextColor") == textBeforeAccent &&
                  playlistTheme.Oran7MainGUI() == mainGuiBefore,
              "accent setting saves independently of background text and main GUI");
        check(QMetaObject::invokeMethod(accentGroup, "colorReady", Q_ARG(QColor, QColor("#22d3ee"))),
              "accent palette selection commits brighter cyan");
        check(playlistTheme.Oran7MusicPlaylistView().value("rowAccentColor").value<QColor>() == QColor("#22d3ee"),
              "accent palette selection restores default exactly");
        musicWindow.hide();
        musicItem->setParentItem(nullptr);
        // 直接加载实际颜色组合，验证方块点击和浮层选择，不依赖主程序或真实配置。
        QStringList colorWarnings;
        const auto warningConnection =
            QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                             [&](const QList<QQmlError>& errors)
                             {
                                 for (const auto& error : errors)
                                 {
                                     colorWarnings.append(error.toString());
                                     std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
                                 }
                             });
        QQmlComponent colors(&engine);
        colors.setData(R"(import QtQuick
Item {
 width: 280; height: 500
 Oran7ColorSettingGroup {
  objectName: "fixtureColor"; y: 20; title: "Palette"
  colorToken: "colorPrimaryBase"; componentName: "Oran7MainGUI"
  checkedColor: "#336699"
  onColorReady: function(color) { checkedColor = color }
 }
})",
                       QUrl::fromLocalFile(QStringLiteral(APP_ROOT) +
                                           "/src/qml/Components/Oran7SettingUiWindowItems/ColorFixture.qml"));
        std::unique_ptr<QObject> colorObject(colors.create());
        if (!colorObject)
            for (const auto& e : colors.errors())
                std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
        check(bool(colorObject), "actual color settings group loads");
        auto* colorRoot = qobject_cast<QQuickItem*>(colorObject.get());
        QQuickWindow colorWindow;
        colorWindow.resize(280, 500);
        colorRoot->setParentItem(colorWindow.contentItem());
        colorWindow.show();
        QTest::qWait(50);
        auto* colorGroup = item(colorRoot, "fixtureColor");
        auto* colorChoice = item(colorRoot, "fixtureColorColorChoice");
        check(colorChoice && colorChoice->property("count").toInt() == 11,
              "color popup provides ten fixed presets plus custom picker");
        check(colorChoice->property("currentIndex").toInt() == 0 &&
                  colorChoice->property("displayText").toString() == "Custom",
              "non-preset color selects custom at first index");
        const auto colorGroupHeight = colorGroup->height();
        auto* colorPopup = colorChoice->property("popup").value<QObject*>();
        QTest::mouseClick(
            &colorWindow, Qt::LeftButton, Qt::NoModifier,
            colorChoice->mapToScene(QPointF(colorChoice->width() / 2, colorChoice->height() / 2)).toPoint());
        QTest::qWait(240);
        check(colorPopup->property("visible").toBool() && colorGroup->height() == colorGroupHeight,
              "preview square opens floating menu without stretching settings layout");
        check(colorWindow.grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/color-popup-en.png"),
              "color menu screenshot");
        auto* colorOption = item(colorWindow.contentItem(), "fixtureColorColorChoiceOption1");
        check(colorOption && colorOption->property("text").toString() == "Iris violet",
              "first preset displays translated color name");
        QTest::mouseClick(
            &colorWindow, Qt::LeftButton, Qt::NoModifier,
            colorOption->mapToScene(QPointF(colorOption->width() / 2, colorOption->height() / 2)).toPoint());
        QTest::qWait(160);
        check(!colorPopup->property("visible").toBool() && colorChoice->property("currentIndex").toInt() == 1 &&
                  colorGroup->property("checkedColor").value<QColor>() == QColor("#A78BFA"),
              "mouse preset selection commits exact fixed color and closes popup");
        QMetaObject::invokeMethod(colorPopup, "open");
        QTest::qWait(240);
        QTest::keyClick(&colorWindow, Qt::Key_Escape);
        QTest::qWait(160);
        check(!colorPopup->property("visible").toBool() && settingState->property("activeOtherItemCount").toInt() == 0,
              "color menu Escape releases focus count");
        QMetaObject::invokeMethod(colorPopup, "open");
        QTest::qWait(240);
        colorGroup->setVisible(false);
        QTest::qWait(160);
        check(!colorPopup->property("visible").toBool(), "hidden color group closes overlay menu");
        colorGroup->setVisible(true);
        QMetaObject::invokeMethod(colorPopup, "open");
        QTest::qWait(240);
        auto* customOption = item(colorWindow.contentItem(), "fixtureColorColorChoiceOption0");
        colorWindow.grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/color-popup-reopen.png");
        check(customOption && customOption->property("text").toString() == "Custom" &&
                  customOption->mapToScene(QPointF(0, 0)).y() > 0,
              "custom picker is first visible option after selecting a preset");
        QTest::mouseClick(
            &colorWindow, Qt::LeftButton, Qt::NoModifier,
            customOption->mapToScene(QPointF(customOption->width() / 2, customOption->height() / 2)).toPoint());
        QTest::qWait(350);
        auto* picker = colorChoice->property("picker").value<QObject*>();
        check(picker && picker->property("visible").toBool(), "custom color window opens");
        check(QMetaObject::invokeMethod(picker, "close"), "cancel custom color window");
        QTest::qWait(350);
        check(settingState->property("activeOtherItemCount").toInt() == 0,
              "custom color cancel releases focus bookkeeping");
        check(QMetaObject::invokeMethod(colorChoice, "openColorPicker"), "reopen custom picker after cancel");
        QTest::qWait(350);
        auto* pickerWindow = qobject_cast<QQuickWindow*>(colorChoice->property("picker").value<QObject*>());
        check(pickerWindow, "custom picker can reopen");
        auto* accept = item(pickerWindow->contentItem(), "colorPickerAccept");
        check(accept, "custom picker accept control");
        QTest::mouseClick(pickerWindow, Qt::LeftButton, Qt::NoModifier,
                          accept->mapToScene(QPointF(accept->width() / 2, accept->height() / 2)).toPoint());
        QTest::qWait(350);
        check(settingState->property("activeOtherItemCount").toInt() == 0 &&
                  settingState->property("activeSettingsPopup").isNull(),
              "custom picker confirmation finishes close animation and releases count and handle");
        check(colorWarnings.isEmpty(), "color controls emit no QML warnings");
        QObject::disconnect(warningConnection);
        colorRoot->setParentItem(nullptr);
        qmlRegisterSingletonType(
            QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/FakeClient.qml"), "Client", 1, 0,
            "Client");
        QQmlComponent studioComponent(
            &engine, QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/Fixture.qml"));
        std::unique_ptr<QObject> studio(studioComponent.create());
        if (!studio)
            for (const auto& e : studioComponent.errors())
                std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
        check(bool(studio), "actual recording workspace with fixture backend");
        QTest::qWait(100);
        auto* studioWindow = qobject_cast<QQuickWindow*>(studio.get());
        auto* workspace = item(studioWindow->contentItem(), "workspace");
        auto* monitor = item(workspace, "recordingMonitor");
        check(monitor && monitor->property("displayText").toString().startsWith("Display"),
              "display labels translated without reloading capture");
        // 显示器控件可通过后续显示文案检查；文件名前缀是用户数据，不能翻译。
        auto* state = workspace->property("pageState").value<QObject*>();
        check(state, "recording page state");
        const auto snapshot = [](const QVariant& value)
        { return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant() : value; };
        const auto draft = snapshot(state->property("outputDraft"));
        check(studioWindow->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/studio-en.png"),
              "English recording workspace screenshot");
        check(preferences.selectLanguage("zh_CN"), "switch studio to Chinese");
        QTest::qWait(30);
        check(monitor->property("displayText").toString().startsWith(QString::fromUtf8("显示器")),
              "existing monitor labels retranslate to Chinese");
        check(snapshot(state->property("outputDraft")) == draft,
              "language switching preserves recording draft and user filename");
        check(preferences.selectLanguage("en_US"), "restore English for restart check");
    }
    Oran7AppConfigManager restoredConfig(path, defaults);
    {
        Oran7UiPreferences restored(restoredConfig);
        check(restored.language() == "en_US" && restored.translate(QString::fromUtf8("界面语言")) == "Language",
              "restart restores translator before QML creation");
        check(restored.fontPixelSize() == 16, "font preferences restored");
    }
    const auto fontPath = directory.filePath("missing-font.json");
    Oran7AppConfigManager fontConfig(fontPath, defaults);
    auto missingFont = fontConfig.snapshot();
    missingFont.appearance.fontFamily = "Missing fixture font";
    fontConfig.updateConfig(missingFont);
    {
        Oran7UiPreferences fallback(fontConfig);
        check(fallback.fontFamilies().contains(fallback.fontFamily()),
              "missing configured font resolves to installed fallback");
        check(fontConfig.snapshot().appearance.fontFamily == "Missing fixture font",
              "fallback does not overwrite user's stored font choice");
    }
    const auto blockedPath = directory.filePath("future.json");
    write(blockedPath, "{\"schemaVersion\":99}");
    Oran7AppConfigManager blockedConfig(blockedPath, defaults);
    Oran7UiPreferences blocked(blockedConfig);
    check(!blocked.selectLanguage("en_US") && blocked.language() == "zh_CN",
          "failed save does not change translator or live selection");
    check(blockedConfig.snapshot().appearance.language == "zh_CN", "failed save restores memory snapshot");
    std::printf("PASS: %d localization checks\n", checks);
}
