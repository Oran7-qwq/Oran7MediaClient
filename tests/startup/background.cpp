#include "Oran7BackgroundFrameMonitor.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFile>
#include <QQuickWindow>
#include <QImage>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QTemporaryDir>
#include <QTest>
#include <QElapsedTimer>
#include <thread>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

class Oran7BackgroundThemeFixture : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap Oran7MainGUI READ values NOTIFY Oran7MainGUIChanged)
public:
    QVariantMap values() const { return m_values; }
    void setEnabled(bool enabled) { m_values["DWMBlurEffectEnabled"]=enabled; emit Oran7MainGUIChanged(); }
signals:
    void Oran7MainGUIChanged();
private:
    QVariantMap m_values{{"DWMBlurEffectEnabled",false}};
};
class Oran7BackgroundAgentFixture : public QObject {
    Q_OBJECT
public:
    int calls=0; bool enabled=false;
    Q_INVOKABLE bool setWindowAttribute(const QString&, bool value) { ++calls; enabled=value; return true; }
};

static int checks = 0;
static void check(bool ok, const char* label)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
    ++checks;
}
template<class F> static void until(F ready)
{
    QElapsedTimer timer; timer.start();
    while (!ready() && timer.elapsed() < 3000) QTest::qWait(5);
    check(ready(), "expected background event completed");
}
static QVideoFrame frame(QRgb color)
{
    QVideoFrame value(QVideoFrameFormat(QSize(16, 16), QVideoFrameFormat::Format_BGRA8888));
    check(value.map(QVideoFrame::WriteOnly), "map synthetic CPU frame");
    for (int y = 0; y < 16; ++y)
        std::fill_n(reinterpret_cast<QRgb*>(value.bits(0) + y * value.bytesPerLine(0)), 16, color);
    value.unmap();
    return value;
}
int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen"); qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_MEDIA_BACKEND", "windows"); qputenv("QSG_RENDER_LOOP", "basic");
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    {
        auto sink = std::make_unique<QVideoSink>();
        Oran7BackgroundFrameMonitor monitor;
        monitor.setSink(sink.get());
        sink->setVideoFrame({});
        check(!monitor.frameReady(), "empty frame cannot release startup");
        sink->setVideoFrame(frame(qRgb(200, 20, 20)));
        check(monitor.frameReady(), "real valid frame releases frame gate");
        monitor.reset();
        auto late = frame(qRgb(20, 20, 200));
        std::thread source([&] { sink->setVideoFrame(late); }); source.join();
        monitor.reset();
        QTest::qWait(20);
        check(!monitor.frameReady(), "queued previous-generation frame is discarded after source reset");
        sink->setVideoFrame(frame(qRgb(20, 200, 20)));
        check(monitor.frameReady(), "new source frame can become ready");
        sink->setVideoFrame({});
        check(monitor.frameReady(), "temporary empty frame cannot alternate video and image layers");
        sink.reset();
        check(!monitor.frameReady() && !monitor.sink(), "destroyed sink clears readiness safely");
    }
    qmlRegisterType<Oran7BackgroundFrameMonitor>("Oran7Startup", 1, 0, "Oran7BackgroundFrameMonitor");
    QTemporaryDir temp;
    QImage image(16, 16, QImage::Format_RGB32); image.fill(QColor("#22bb44"));
    check(image.save(temp.filePath("still.png")), "temporary synthetic fallback image");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("fixtureImage", QUrl::fromLocalFile(temp.filePath("still.png")));
    engine.load(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/startup/BackgroundWindow.qml"));
    check(engine.rootObjects().size() == 1, "production background loads in isolated software window");
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto* background = window->findChild<QObject*>("background");
    auto* still = background->findChild<QObject*>("mainBackgroundImage");
    auto* output = background->findChild<QObject*>("mainBackgroundVideoOutput");
    auto* sink = output->property("videoSink").value<QVideoSink*>();
    check(sink, "actual VideoOutput exposes its sink");
    until([&] { return still->property("status").toInt() == 1; });
    background->setProperty("prepareActive", true);
    QTest::qWait(20);
    check(!background->property("ready").toBool() && !still->property("visible").toBool(),
          "preloaded fallback image is not exposed while selected video is preparing");
    check(window->grabWindow().pixelColor(3, 3) == QColor("#171a22"), "pre-frame background is solid rather than white or transparent");
    sink->setVideoFrame(frame(qRgb(204, 34, 51)));
    until([&] { return background->property("ready").toBool(); });
    // QtMultimedia 的 VideoOutput 不支持此软件场景图；在这里检查真实 sink 门控，
    // 视频纹理的 D3D11 呈现留运行验收，软件像素仅验证底色/图片/透明路由。
    check(output->property("visible").toBool(), "selected video output stays visible while its first valid frame is prepared");
    check(background->property("selectedLayer").toString() == "Video" && !still->property("visible").toBool(),
          "valid video never exposes the static-image intermediate stage");
    background->setProperty("videoTimeoutMs", 30);
    check(QMetaObject::invokeMethod(background, "reloadVideo"), "restart selected video for timeout case");
    until([&] { return background->property("videoFailed").toBool(); });
    check(background->property("selectedLayer").toString() == "Image" && background->property("ready").toBool(),
          "video timeout selects ready image fallback with bounded wait");
    check(window->grabWindow().pixelColor(3, 3) == QColor("#22bb44"), "fallback image is rendered");
    sink->setVideoFrame(frame(qRgb(20, 20, 220)));
    QTest::qWait(20);
    check(background->property("selectedLayer").toString() == "Image" && window->grabWindow().pixelColor(3, 3) == QColor("#22bb44"),
          "late valid video cannot steal the chosen fallback after startup");
    background->setProperty("mode", "Image");
    background->setProperty("nativeReady", false);
    check(!background->property("ready").toBool(), "native style application is part of readiness");
    background->setProperty("nativeReady", true);
    check(background->property("ready").toBool(), "static image becomes ready without video decoding");
    background->setProperty("dwmEnabled", true);
    check(background->property("ready").toBool() && background->property("selectedLayer").toString() == "Dwm",
          "explicit DWM route does not pass through image/video");
    check(window->grabWindow().pixelColor(3, 3) == QColor("#663399"), "DWM route preserves intentional transparency");
    {
        // 使用 main.qml 的实际原生样式控制代码，只替换主题和原生 API 边界。
        QFile mainFile(QStringLiteral(APP_ROOT)+"/main.qml");
        check(mainFile.open(QIODevice::ReadOnly),"read production main background style wiring");
        const auto mainSource=mainFile.readAll();
        const int first=mainSource.indexOf("    property bool backgroundStyleInitialized:");
        const int last=mainSource.indexOf("    property alias __Oran7WindowBackGround__:",first);
        check(first>=0 && last>first,"locate production native background style block");
        Oran7BackgroundThemeFixture theme;
        Oran7BackgroundAgentFixture agent;
        QQmlApplicationEngine styleEngine;
        styleEngine.rootContext()->setContextProperty("Oran7Theme",&theme);
        styleEngine.rootContext()->setContextProperty("__windowAgent",&agent);
        const auto folder=QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/src/qml/Components/Oran7GlobalComponents").toEncoded();
        const auto source=QByteArray("import QtQuick\nimport QtQuick.Window\nimport \"")+folder+"\"\nWindow { id: mainWindow; width:320; height:200; color:\"transparent\"\n"
            +mainSource.mid(first,last-first)+"\nOran7MainWindowBackground { objectName:\"priorityBackground\"; nativeReady:mainWindow.backgroundNativeReady; prepareActive:true; dwmEnabled:mainWindow.backgroundDwmApplied } }";
        styleEngine.loadData(source,QUrl("qrc:/BackgroundPriorityFixture.qml"));
        check(styleEngine.rootObjects().size()==1,"production style wiring loads in isolation");
        auto* style=styleEngine.rootObjects().first();
        auto* layers=style->findChild<QObject*>("priorityBackground");
        style->setProperty("backgroundStyleInitialized",true);
        check(QMetaObject::invokeMethod(style,"applyBackgroundWindowStyle"),"initialize production native style");
        check(!agent.enabled && agent.calls==1,"initial native blur disabled");
        theme.setEnabled(true);
        check(agent.enabled && style->property("backgroundDwmApplied").toBool(),"settings toggle immediately reaches native window style");
        check(layers->property("selectedLayer")=="Dwm","DWM overrides image background mode");
        const int appliedCalls=agent.calls;
        theme.setEnabled(true);
        check(agent.calls==appliedCalls,"unchanged blur setting does not reapply native style on unrelated group notification");
        layers->setProperty("mode","Video");
        check(layers->property("selectedLayer")=="Dwm" && !layers->property("wantsVideo").toBool(),"DWM overrides video background mode");
        theme.setEnabled(false);
        check(!agent.enabled && !style->property("backgroundDwmApplied").toBool(),"disabling DWM immediately restores chosen media mode");
        check(layers->property("selectedLayer")=="Video","turning blur off restores selected video mode");
        layers->setProperty("mode","Image");
        check(layers->property("selectedLayer")=="Image","image mode remains selectable when blur is disabled");
    }
    std::printf("PASS: %d background frame/readiness checks; synthetic CPU frames, no real media or hardware capture.\n", checks);
}
#include "background.moc"
