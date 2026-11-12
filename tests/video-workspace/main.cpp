#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QFile>
#include <QTest>
#include <QJSValue>
#include <QFontDatabase>
#include <QProcess>
#include <QElapsedTimer>
#include "FakeRoomNetwork.h"
#include "Oran7AppConfigManager.h"
#include "Oran7VideoHistory.h"
#include <QJsonDocument>
#include <memory>
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void write(const QString& path, const QByteArray& data)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "write isolated fixture");
    file.write(data);
}
static QQuickItem* find(QQuickItem* parent, const QString& name)
{
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems()) if (auto* result = find(child, name)) return result;
    return nullptr;
}
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    check(QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc") >= 0,
          "load Chinese font for isolated visual inspection");
    app.setFont(QFont("Microsoft YaHei"));
    QTemporaryDir dir;
    const auto source = QStringLiteral(APP_ROOT);
    const auto fixtures = source + "/tests/video-workspace/";
    auto fixtureUrl = [&](const char* name) { return QUrl::fromLocalFile(fixtures + name); };
    const int clientId = qmlRegisterSingletonType(fixtureUrl("FakeClient.qml"), "Client", 1, 0, "Client");
    qmlRegisterType(fixtureUrl("FakeRoom.qml"), "BilibiliRoomAddressCatch", 1, 0, "BilibiliRoomAddressCatch");
    qmlRegisterType(fixtureUrl("FakeDanmaku.qml"), "BilibiliDanmakuClient", 1, 0, "BilibiliDanmakuClient");
    QQmlPropertyMap theme, auth;
    theme.insert("Primary", QVariantMap{{"durationFast", 100},{"durationVerySlow",100}});
    theme.insert("Oran7ProgressSlider", QVariantMap{{"trackColor","#888888"},{"trackOpacity",0.3},{"moveThreshold",2}});
    theme.insert("Oran7MainGUI", QVariantMap{{"topBarDefaultHeight",30}});
    theme.insert("Oran7VideoPlayer", QVariantMap{{"ScaleMode","Fill"}});
    auth.insert("isLoggedIn", false);
    qmlRegisterModule("D3D11VideoItem", 1, 0);
    qmlRegisterSingletonInstance("Oran7UI.Impl", 1, 0, "Oran7Theme", &theme);
    qmlRegisterSingletonInstance("BilibiliAuth", 1, 0, "BilibiliAuthManager", &auth);
    write(dir.filePath("FakeFileHelper.qml"), "import QtQml\nQtObject { function fileExists(path) { return path.length > 0 && path.indexOf(\"missing\") < 0 } }");
    qmlRegisterType(QUrl::fromLocalFile(dir.filePath("FakeFileHelper.qml")), "Oran7UI.Impl", 1, 0, "Oran7FileHelper");
    write(dir.filePath("FakeWindowAgent.qml"), R"qml(import QtQml
QtObject {
    function setTitleBar(item) {}
    function setHitTestVisible(item, value) {}
    function setSystemButton(role, item) {}
})qml");
    qmlRegisterType(QUrl::fromLocalFile(dir.filePath("FakeWindowAgent.qml")), "Oran7UI.Impl", 1, 0, "Oran7WindowAgent");
    for (const auto& name : {"Oran7RoundedImage.qml", "Oran7CaptionBar.qml", "Oran7VideoLibraryPage.qml", "Oran7VideoPlaybackToolbar.qml", "Oran7ValueSlider.qml",
                            "Oran7StudioButton.qml","Oran7ProgressSlider.qml","Oran7GradientMask.qml","Oran7PlayerVolumeComponent.qml"}) {
        QFile file(source + "/src/qml/Components/Oran7GlobalComponents/" + name);
        check(file.open(QIODevice::ReadOnly), "read production component");
        auto componentData=file.readAll();
        componentData.replace("import \"../../Basic\"", "");
        componentData.replace("import \"../../Settings/GlobalSettings\"", "");
        write(dir.filePath(name), componentData);
    }
    QFile entry(source + "/src/qml/RightPage/MainStackView/VideoPlayerStack.qml");
    check(entry.open(QIODevice::ReadOnly), "read video page");
    auto data = entry.readAll();
    data.replace("import \"../../Basic\"", "");
    data.replace("import \"../../Components/Oran7GlobalComponents\"", "import \".\"");
    write(dir.filePath("VideoPlayerStack.qml"), data);
    QFile productionRenderer(source + "/src/qml/Components/Oran7GlobalComponents/Oran7VideoRendererItem.qml");
    check(productionRenderer.open(QIODevice::ReadOnly), "read actual video renderer for configuration regression");
    write(dir.filePath("Oran7VideoRendererItem.qml"), productionRenderer.readAll());
    write(dir.filePath("Oran7BlurCard.qml"), R"qml(import QtQuick
Item {
    property Item blurSource
    property bool blurEnabled: false
    property bool forceLocalSourceRect: false
    property color themeColor
})qml");
    write(dir.filePath("Oran7DanmakuLayer.qml"), R"qml(import QtQuick
Item {
    property bool active: false
    function spawn(text,color) {}
    function showSuperChat(text,uname,price,color) {}
})qml");
    write(dir.filePath("Oran7FileDialog.qml"), R"qml(import QtQml
QtObject {
    property var nameFilters
    property string selectedFile: ""
    property bool visible: false
    function open() { visible = true }
    signal ready()
})qml");
    write(dir.filePath("Fixture.qml"), R"qml(import QtQuick
import QtQuick.Controls
import Oran7UI.Impl 1.0
ApplicationWindow {
    width: 760; height: 490; visible: true
    property rect normalWindowGeometry: Qt.rect(0,0,width,height)
    StackView {
        id: stack
        anchors.fill: parent
        initialItem: VideoPlayerStack { objectName: "videoPage"; windowButtonsHovered: fixtureCaption.windowButtonsHovered }
    }
    Oran7CaptionBar {
        id: fixtureCaption
        width: parent.width; height: 30; z: 65535
        targetWindow: Window.window
        showWinTitle: false; showWinIcon: false
        windowAgent: Oran7WindowAgent {}
        windowButtonsOpacity: stack.currentItem ? stack.currentItem.windowControlsOpacity : 1
        onWindowButtonsHoveredChanged: {
            if (stack.currentItem && stack.currentItem.playbackView) stack.currentItem.wakeControls()
        }
    }
})qml");
    QQmlEngine engine;
    QFile defaultsFile(source + "/config/default.json");
    check(defaultsFile.open(QIODevice::ReadOnly),"read defaults for isolated history");
    Oran7AppConfigManager config(dir.filePath("config.json"),QJsonDocument::fromJson(defaultsFile.readAll()).object());
    auto initial=config.snapshot();
    initial.videoHistory={
        QVariantMap{{"mediaKind","local"},{"target","C:/fixture video.mp4"},{"lastOpenedAt","2026-11-10T12:00:00.000Z"},
                    {"positionMs",84000},{"durationMs",300000},{"completed",false}},
        QVariantMap{{"mediaKind","live"},{"target","456"},{"lastOpenedAt","2026-11-09T12:00:00.000Z"}}
    };
    config.updateConfig(initial);
    check(config.saveConfig(),"seed previous-run watching history in temporary config");
    Oran7VideoHistory history(config,dir.filePath("thumbnails"),source + "/ffmpeg6.0/bin/ffmpeg.exe",
        std::make_unique<Oran7FakeRoomNetwork>());
    engine.rootContext()->setContextProperty("videoHistoryFixture",&history);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError>& errors) {
        for (const auto& error : errors) { warnings << error.toString(); std::fprintf(stderr, "%s\n", qPrintable(error.toString())); }
    });
    QQmlComponent basicComponent(&engine);
    basicComponent.setData(R"qml(import QtQml
QtObject {
    property var mainWindow: null
    property int globalPlayingFocus: 0
    property int globalPlayer_VideoPlayerIndex: 1
    property int globalPlayer_LivePlayerIndex: 2
    property int max_Slider_Value: 65535
    property int playerVolume: 80
    function clearAllUi_inVIdeoRenderArea(value) {}
    function openBilibiliLoginPopup() {}
})qml", QUrl());
    std::unique_ptr<QObject> basic(basicComponent.create());
    engine.rootContext()->setContextProperty("BasicConfig", basic.get());
    QQmlComponent component(&engine, QUrl::fromLocalFile(dir.filePath("Fixture.qml")));
    std::unique_ptr<QObject> windowObject(component.create());
    if (!windowObject) for (const auto& error : component.errors()) std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
    auto* window = qobject_cast<QQuickWindow*>(windowObject.get());
    check(window, "video workspace loads");
    basic->setProperty("mainWindow", QVariant::fromValue<QObject*>(window));
    QTest::qWait(200);
    auto* page = find(window->contentItem(), "videoPage");
    auto* renderer = find(page, "videoRenderer");
    check(page && renderer && page->property("isActive").toBool(), "initial StackView page activates");
    check(find(page, "videoOpenFileButton")->isVisible(), "library opens first");
    check(find(page,"videoHistoryRemoveButton") && history.localCount()==1 && history.liveCount()==1,
          "previous-run history is visible immediately without autoplay");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/library.png");
    auto* client = engine.singletonInstance<QObject*>(clientId);
    check(client->property("renderScale").toInt()==1, "saved Fill applies without creating video settings panel");
    auto* playback = client->property("playback").value<QObject*>();
    auto playFile = [&](const QString& path) { check(QMetaObject::invokeMethod(page, "playLocal", Q_ARG(QVariant, path)), "open local video"); };
    playFile("C:/missing.mp4");
    check(!page->property("playbackView").toBool(), "missing path stays in library");
    playFile("file:///C:/fixture%20video.mp4");
    check(playback->property("source") == "C:/fixture video.mp4" && page->property("ownsPlayback").toBool(), "file URL decodes and routes to backend");
    check(client->property("playCount").toInt() == 1, "one explicit play command");
    check(history.localCount()==1,"successful local playback records a visit");
    QMetaObject::invokeMethod(page, "showLibrary");
    check(playback->property("playing").toBool() && !page->property("playbackView").toBool(), "return to library keeps playback running");
    check(page->property("windowControlsOpacity").toReal() == 1 && page->property("networkLatencyOpacity").toReal() == 1,
          "library restores ordinary window controls and network display");
    auto* resumeButton = find(page, "videoResumeButton");
    check(resumeButton && resumeButton->width() >= 180 && resumeButton->height() >= 50,
          "current session provides a prominent return-to-playback action");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/session.png");
    QMetaObject::invokeMethod(resumeButton, "clicked");
    check(renderer == find(page, "videoRenderer") && client->property("playCount").toInt() == 1, "return to video reuses render host and session");
    QMetaObject::invokeMethod(find(page, "videoSettingsButton"), "clicked");
    QTest::qWait(50);
    QTest::keyClick(window, Qt::Key_Escape);
    QTest::qWait(50);
    check(page->property("playbackView").toBool(), "Escape does not navigate away when closing video popup");
    QTest::keyClick(window, Qt::Key_Escape);
    check(page->property("playbackView").toBool() && playback->property("playing").toBool(),
          "Escape on focused video canvas keeps playback view and leaves the global shortcut available");
    QMetaObject::invokeMethod(page, "togglePlayback");
    check(!playback->property("playing").toBool() && client->property("toggleCount").toInt() == 1, "pause uses explicit backend command");
    QMetaObject::invokeMethod(page, "togglePlayback");
    check(page->property("networkLatencyOpacity").toReal() == 1, "local video normally displays network latency");
    auto* nativeButtons = find(window->contentItem(), "nativeWindowButtons");
    check(nativeButtons && nativeButtons->isVisible() && nativeButtons->isEnabled(),
          "window buttons remain available while video header is visible");
    auto* toolbar = find(page, "videoPlaybackToolbar");
    QMetaObject::invokeMethod(toolbar, "seekRequested", Q_ARG(int, 20), Q_ARG(QString, "old.mp4"));
    check(client->property("seekCount").toInt() == 0, "stale drag cannot seek a different source");
    QMetaObject::invokeMethod(toolbar, "seekRequested", Q_ARG(int, 20), Q_ARG(QString, "C:/fixture video.mp4"));
    check(playback->property("positionMs").toInt() == 20000, "current source seek reaches backend");
    auto* seekSlider=find(page,"videoSeekSlider");
    const auto seekPoint=seekSlider->mapToScene(QPointF(seekSlider->width()/2,seekSlider->height()/2)).toPoint();
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,seekPoint);
    check(qAbs(playback->property("positionMs").toInt()-150000)<=1000,
          "restored progress control commits its clicked position before bound progress resumes");
    const auto dragEnd=seekSlider->mapToScene(QPointF(seekSlider->width()*0.75,seekSlider->height()/2)).toPoint();
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,seekPoint);
    QTest::mouseMove(window,dragEnd,30);
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,dragEnd);
    check(qAbs(playback->property("positionMs").toInt()-225000)<=1000,"restored progress control preserves drag seeking");
    QTest::mouseMove(window, QPoint(320,220));
    QMetaObject::invokeMethod(page,"wakeControls");
    QTest::qWait(1650);
    check(!page->property("controlsVisible").toBool() && !page->property("mouseHidden").toBool(),
          "static pointer hides controls and header before cursor");
    QTest::qWait(500);
    check(page->property("controlsFullyHidden").toBool() && !page->property("mouseHidden").toBool(),
          "cursor remains visible after controls finish fading out");
    check(page->property("windowControlsOpacity").toReal() == 0 && !nativeButtons->isVisible() && !nativeButtons->isEnabled(),
          "faded window controls are hidden and cannot accept native or QML clicks");
    check(page->property("networkLatencyOpacity").toReal() == 0,
          "stationary pointer on local video hides NET with the video header");
    QTest::qWait(1800);
    check(page->property("mouseHidden").toBool(),"cursor hides in the later idle stage");
    check(window->cursor().shape()==Qt::BlankCursor,"the actual canvas window cursor becomes blank");
    QTest::mouseMove(window,QPoint(321,220));
    QTest::qWait(30);
    check(page->property("mouseHidden").toBool(),"subthreshold pointer jitter does not wake playback chrome");
    QTest::mouseMove(window,QPoint(325,220));
    QTest::qWait(30);
    check(page->property("controlsVisible").toBool() && !page->property("mouseHidden").toBool(),
          "meaningful pointer movement restores both idle stages");
    check(window->cursor().shape()==Qt::ArrowCursor,"canvas movement restores the actual arrow cursor");
    QTest::qWait(320);
    check(nativeButtons->isVisible() && nativeButtons->isEnabled() && nativeButtons->opacity() > 0.99,
          "pointer movement restores window buttons with video header opacity");
    check(page->property("networkLatencyOpacity").toReal() > 0.99,
          "moving pointer restores NET during local video playback");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/player.png");
    auto playRoom = [&](const QString& number) { QMetaObject::invokeMethod(page, "playRoom", Q_ARG(QVariant, number)); };
    playRoom("123");
    auto* oldRequest = page->property("activeRequest").value<QObject*>();
    playRoom("456");
    auto* request = page->property("activeRequest").value<QObject*>();
    check(oldRequest && request && oldRequest != request, "new live request supersedes old resolver");
    QMetaObject::invokeMethod(oldRequest, "urlsReady");
    check(client->property("playCount").toInt() == 1, "old resolver cannot start playback");
    QMetaObject::invokeMethod(request, "urlsReady");
    check(client->property("playCount").toInt() == 2 && page->property("live").toBool(), "latest live result starts once");
    check(history.liveCount()==1 && history.latest().value("target")=="456","live history records the room number instead of the stream URL");
    check(!find(page, "videoSeekSlider")->isVisible(), "live playback has no local timeline");
    check(page->property("networkLatencyOpacity").toReal() == 1, "Bilibili live playback displays network latency");
    QMetaObject::invokeMethod(page, "wakeControls");
    QTest::qWait(2000);
    check(page->property("windowControlsOpacity").toReal() == 0 && page->property("networkLatencyOpacity").toReal() == 1,
          "live NET stays visible even when the idle video header and window buttons are hidden");
    playRoom("789");
    playback->setProperty("source", "new-music.mp3");
    check(!page->property("resolving").toBool(), "another media command cancels unresolved live intent");
    playFile("C:/fixture.mp4");
    QMetaObject::invokeMethod(page, "toggleFullscreen");
    QTest::qWait(100);
    check(page->property("fullscreen").toBool() &&
              find(window->contentItem(), "videoPlaybackSurface")->parentItem() == window->property("contentItem").value<QQuickItem*>(), "fullscreen moves same surface above main UI");
    QTest::keyClick(window, Qt::Key_Escape);
    check(page->property("fullscreen").toBool() && page->property("playbackView").toBool(),
          "fullscreen video does not repurpose the global Escape shortcut");
    QMetaObject::invokeMethod(page, "exitFullscreen");
    check(!page->property("fullscreen").toBool() && renderer == find(page, "videoRenderer"), "fullscreen exit restores stable render host");
    window->resize(560, 400);
    QMetaObject::invokeMethod(page, "showLibrary");
    QTest::qWait(200);
    check(find(page,"videoHistoryRemoveButton"),"restored watching records appear in the library");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/compact.png");
    check(find(page, "videoStartButton")->width() > 0, "compact entry remains usable");
    const auto clip=dir.filePath("preview.mkv");
    QProcess generate;
    generate.start(source + "/ffmpeg6.0/bin/ffmpeg.exe",{"-nostdin","-hide_banner","-loglevel","error",
        "-f","lavfi","-i","color=c=blue:size=160x90:rate=5","-t","1","-c:v","ffv1","-y",clip});
    check(generate.waitForFinished(10000) && generate.exitCode()==0,"generate a local thumbnail UI fixture");
    history.rememberLocal(clip);
    QElapsedTimer thumbnailWait;
    thumbnailWait.start();
    while (history.latest().value("thumbnail").toString().isEmpty() && thumbnailWait.elapsed()<12000) QTest::qWait(20);
    check(!history.latest().value("thumbnail").toString().isEmpty(),"visible history delegate requests its real thumbnail automatically");
    QTest::qWait(200);
    auto* thumbnail=find(page,"videoHistoryThumbnail");
    check(thumbnail && find(thumbnail,"roundedImageContent")->property("status").toInt()==1,"rounded history thumbnail image finishes loading");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/thumbnails.png");
    auto* library=find(page,"videoLibraryPage");
    library->setProperty("selectedTab",1);
    QTest::qWait(100);
    bool liveInfoVisible=false;
    for (int row=0;row<history.rowCount();++row) {
        if (history.data(history.index(row,0),Oran7VideoHistory::MediaKind)=="live")
            liveInfoVisible=history.data(history.index(row,0),Oran7VideoHistory::AnchorName)=="Fixture anchor";
    }
    check(liveInfoVisible,"entering the live tab loads metadata through the existing history model");
    window->resize(760,490);
    QTest::qWait(100);
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/live-rooms.png");
    window->resize(1100, 640);
    QTest::qWait(150);
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/library-wide.png");
    check(warnings.isEmpty(), "workspace emits no QML warnings");
    theme.insert("Oran7VideoPlayer", QVariantMap{{"ScaleMode","Fit"}});
    QCoreApplication::processEvents();
    check(client->property("renderScale").toInt()==0, "renderer follows configuration changed after creation");
    theme.insert("Oran7VideoPlayer", QVariantMap{{"ScaleMode","Fill"}});
    QCoreApplication::processEvents();
    QMetaObject::invokeMethod(renderer, "tryAttachDelayed");
    QTest::qWait(30);
    check(client->property("renderScale").toInt()==1, "reattaching renderer preserves current Fill configuration");
    std::puts("PASS: video workspace layout and routing");
}
