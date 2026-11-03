#include "Oran7StartupCoordinator.h"
#include "Oran7StartupActivityItem.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QFontDatabase>
#include <QQuickItem>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cmath>

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
template <class F> static void until(F ready)
{
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 5000)
        QTest::qWait(5);
    check(ready(), "expected startup transition completed");
}
static Oran7StartupCoordinator::Oran7Options options()
{
    Oran7StartupCoordinator::Oran7Options value;
    value.splashUrl = QUrl::fromLocalFile(QStringLiteral(APP_ROOT) +
                                          "/src/qml/Components/Oran7GlobalComponents/Oran7StartupWindow.qml");
    value.mainUrl = QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/startup/MainWindow.qml");
    value.timeoutMs = 5000;
    return value;
}
int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RENDER_LOOP", "basic");
    QGuiApplication app(argc, argv);
    qmlRegisterType<Oran7StartupActivityItem>("Oran7Startup", 1, 0, "Oran7StartupActivityItem");
    app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc");
    app.setFont(QFont("Microsoft YaHei"));
    using Stage = Oran7StartupCoordinator::Oran7Stage;
    {
        // 协调器覆盖引擎生命周期，模拟 main 的上下文借用关系。
        std::unique_ptr<Oran7StartupCoordinator> startup;
        QQmlApplicationEngine engine;
        startup = std::make_unique<Oran7StartupCoordinator>(engine);
        engine.rootContext()->setContextProperty("Oran7Startup", startup.get());
        int initialized = 0, restored = 0;
        auto request = options();
        request.timeoutMs = 10000; // 留出完整动画循环采样时间，生产超时不变。
        request.initialize = [&]
        {
            ++initialized;
            return QString();
        };
        request.restore = [&](QQuickWindow* window)
        {
            ++restored;
            check(!window->isVisible(), "configuration applies while main window is hidden");
            window->resize(820, 480);
            window->setPosition(-120, 40);
            window->setProperty("restored", true);
            return QString();
        };
        QSignalSpy ready(startup.get(), &Oran7StartupCoordinator::ready);
        startup->start(request);
        check(initialized == 0 && startup->splashWindow(), "service initialization waits for splash presentation");
        auto* surface = startup->splashWindow()->findChild<QQuickItem*>("oran7StartupSurface");
        check(surface && surface->width() == 460 && surface->height() == 300,
              "startup surface has full geometry before the first native frame");
        const auto shot = startup->splashWindow()->grabWindow();
        check(!shot.isNull(), "production startup surface renders offscreen");
        check(shot.pixelColor(200, 150) == QColor("#1c1f29"), "first splash frame is opaque and fully painted");
        shot.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/startup.png");
        until([&] { return startup->mainWindow() && restored == 1; });
        QTest::qWait(40);
        check(!startup->finished() && !startup->revealMain(),
              "configuration and component completion alone cannot reveal page");
        check(startup->mainWindow()->opacity() == 0.0,
              "native main stays fully transparent while its buffers are warming");
        check(!startup->mainWindow()->findChild<QQuickItem*>("oran7StartupSurface"),
              "only the independent splash contains the startup UI");
        auto* track = surface->findChild<QQuickItem*>("startupActivityTrack");
        auto* pulse = surface->findChild<QQuickItem*>("startupActivityPulse");
        check(track && pulse, "production startup activity indicator is available");
        auto* native = surface->findChild<QObject*>("startupNativeActivity");
        check(native && !native->property("nativeActive").toBool(), "offscreen fixture uses explicit QML fallback without native windows");
        auto* wrap = surface->findChild<QQuickItem*>("startupActivityWrap");
        check(wrap, "rightward indicator has a seamless wrap copy");
        bool bounded = true, continuous = true, wrapped = false, movedRight = false;
        qreal previous = pulse->x();
        QElapsedTimer sample;
        sample.start();
        while (sample.elapsed() < 2200)
        {
            QTest::qWait(20);
            const qreal current = pulse->x();
            bounded &= current >= -0.01 && current <= track->width() + 0.01;
            qreal distance = current - previous;
            if (distance < -0.01) { distance += track->width(); wrapped = true; }
            continuous &= distance >= -0.01 && distance < track->width() * 0.3;
            movedRight |= distance > 0.1;
            const qreal visibleWidth = qMin(pulse->width(), track->width() - current)
                + qMax(0.0, wrap->x() + wrap->width());
            bounded &= std::abs(visibleWidth - pulse->width()) < 0.1;
            previous = current;
        }
        check(bounded && continuous && wrapped && movedRight,
              "activity indicator moves right through a seamless wrap without a blank track");
        check(engine.rootObjects().size() == 1 && engine.rootObjects().first() == startup->splashWindow() &&
                  static_cast<QObject*>(startup->mainWindow())->parent() == &engine,
              "same engine owns the loaded splash and asynchronously incubated main");
        check(startup->splashWindow()->width() == 460 && startup->mainWindow()->width() == 820,
              "main geometry never changes splash dimensions");
        check(startup->mainWindow()->grabWindow().pixelColor(5, 5) == QColor("#171a22"),
              "covered main frame cannot expose incomplete page");
        bool invalidated = false;
        const auto revoke = QObject::connect(startup.get(), &Oran7StartupCoordinator::changed,
                                             [&]
                                             {
                                                 if (startup->revealMain() && !invalidated)
                                                 {
                                                     invalidated = true;
                                                     startup->mainWindow()->setProperty("startupPageReady", false);
                                                 }
                                             });
        startup->mainWindow()->setProperty("startupPageReady", true);
        until([&] { return invalidated; });
        check(!startup->finished() && !startup->revealMain(), "late background change restores cover before handoff");
        QObject::disconnect(revoke);
        bool nativeRevoked = false;
        const auto revokeNative =
            QObject::connect(startup->mainWindow(), &QWindow::opacityChanged,
                             [&]
                             {
                                 if (startup->mainWindow()->opacity() > 0.0 && !nativeRevoked)
                                 {
                                     nativeRevoked = true;
                                     check(!startup->finished() && startup->splashWindow()->isVisible(),
                                           "splash remains visible when the primed native main first becomes opaque");
                                     startup->mainWindow()->setProperty("startupPageReady", false);
                                 }
                             });
        startup->mainWindow()->setProperty("startupPageReady", true);
        until([&] { return nativeRevoked; });
        check(!startup->finished() && !startup->revealMain() && startup->mainWindow()->opacity() == 0.0,
              "readiness loss during native presentation hides main again before handoff");
        QObject::disconnect(revokeNative);
        bool partialMain = false, partialSplash = false, fadeGateHeld = true;
        const auto sampleFade =
            QObject::connect(startup->mainWindow(), &QWindow::opacityChanged,
                             [&]
                             {
                                 const qreal opacity = startup->mainWindow()->opacity();
                                 if (opacity > 0 && opacity < 1)
                                 {
                                     partialMain = true;
                    fadeGateHeld &= !startup->finished() && startup->splashWindow()->isVisible();
                                 }
                             });
        const auto sampleSplash = QObject::connect(startup->splashWindow(), &QWindow::opacityChanged,
                                                   [&]
                                                   {
                                                       const qreal opacity = startup->splashWindow()->opacity();
                                                       partialSplash |= opacity > 0 && opacity < 1;
                                                   });
        startup->mainWindow()->setProperty("startupPageReady", true);
        until([&] { return startup->finished(); });
        QObject::disconnect(sampleFade);
        QObject::disconnect(sampleSplash);
        check(partialMain && partialSplash, "both native windows have intermediate crossfade opacity");
        check(fadeGateHeld, "crossfade keeps splash until main opacity animation completes");
        check(ready.size() == 1 && !startup->splashWindow()->isVisible() && startup->mainWindow()->isVisible(),
              "ready handoff hides splash after revealed main frame");
        check(startup->mainWindow()->opacity() == 1.0, "successful handoff restores the intended native opacity");
        check(startup->mainWindow()->grabWindow().pixelColor(5, 5) == QColor("#dce8e0"),
              "handoff presents actual page after removing cover");
        QMetaObject::invokeMethod(startup->mainWindow(), "frameSwapped");
        startup->start(request);
        check(initialized == 1 && restored == 1 && ready.size() == 1,
              "duplicate notifications/start cannot repeat handoff");
    }
    {
        std::unique_ptr<Oran7StartupCoordinator> startup;
        QQmlApplicationEngine engine;
        startup = std::make_unique<Oran7StartupCoordinator>(engine);
        engine.rootContext()->setContextProperty("Oran7Startup", startup.get());
        int initialized = 0;
        auto request = options();
        request.initialize = [&]
        {
            ++initialized;
            return QString();
        };
        QSignalSpy quit(startup.get(), &Oran7StartupCoordinator::quitRequested);
        startup->start(request);
        startup->cancel();
        startup->cancel();
        QTest::qWait(50);
        check(initialized == 0 && quit.size() == 1 && startup->stage() == Stage::Cancelled,
              "early close cancels queued initialization exactly once");
        check(!startup->mainWindow(), "cancelled startup cannot create late main root");
    }
    for (int failure = 0; failure < 7; ++failure)
    {
        std::unique_ptr<Oran7StartupCoordinator> startup;
        QQmlApplicationEngine engine;
        startup = std::make_unique<Oran7StartupCoordinator>(engine);
        engine.rootContext()->setContextProperty("Oran7Startup", startup.get());
        auto request = options();
        QTemporaryDir temp;
        if (failure == 0)
            request.initialize = [] { return QStringLiteral("fixture initialization failure"); };
        if (failure == 1)
        {
            QFile file(temp.filePath("bad.qml"));
            check(file.open(QIODevice::WriteOnly), "create invalid QML fixture");
            file.write("import QtQuick\nWindow { broken syntax }");
            file.close();
            request.mainUrl = QUrl::fromLocalFile(file.fileName());
        }
        if (failure == 2)
            request.restore = [](QQuickWindow*) { return QStringLiteral("fixture restore failure"); };
        if (failure == 3)
            request.timeoutMs = 500;
        if (failure == 5)
            request.splashUrl = QUrl::fromLocalFile(temp.filePath("missing-startup.qml"));
        if (failure == 6)
        {
            QFile file(temp.filePath("required.qml"));
            check(file.open(QIODevice::WriteOnly), "create incubation failure fixture");
            file.write("import QtQuick\nWindow { visible: false; required property int missingInput }");
            file.close();
            request.mainUrl = QUrl::fromLocalFile(file.fileName());
        }
        QSignalSpy quit(startup.get(), &Oran7StartupCoordinator::quitRequested);
        startup->start(request);
        if (failure == 4)
        {
            until([&] { return startup->mainWindow() && startup->stage() == Stage::PreparingDisplay; });
            emit startup->mainWindow()->sceneGraphError(QQuickWindow::ContextNotAvailable,
                                                        QStringLiteral("fixture graphics failure"));
        }
        until([&] { return startup->failed(); });
        check(!startup->error().isEmpty() && !startup->finished() &&
                  (startup->splashWindow() ? startup->splashWindow()->isVisible() : quit.size() == 1),
              "failure stays visible or requests exit when startup window is unavailable");
        if (startup->mainWindow())
        {
            check(!startup->mainWindow()->isVisible(), "failed main remains hidden");
            startup->mainWindow()->setProperty("startupPageReady", true);
            QMetaObject::invokeMethod(startup->mainWindow(), "frameSwapped");
            check(!startup->finished(), "late readiness cannot resurrect failed startup");
        }
        startup->cancel();
        check(startup->stage() == Stage::Cancelled, "error window can exit cleanly");
    }
    for (bool cancel : {false, true})
    {
        std::unique_ptr<Oran7StartupCoordinator> startup;
        QQmlApplicationEngine engine;
        startup = std::make_unique<Oran7StartupCoordinator>(engine);
        engine.rootContext()->setContextProperty("Oran7Startup", startup.get());
        startup->start(options());
        until([&] { return startup->mainWindow(); });
        startup->mainWindow()->setProperty("startupPageReady", true);
        until([&] { return startup->splashWindow()->opacity() < 0.95; });
        if (cancel) startup->cancel();
        else emit startup->mainWindow()->sceneGraphError(QQuickWindow::ContextNotAvailable, "fixture fade failure");
        check(!startup->mainWindow()->isVisible() && startup->splashWindow()->opacity() == 1.0,
              "interrupted fade restores readable splash and hides main");
        QTest::qWait(400);
        check(!startup->finished() && (cancel ? startup->stage() == Stage::Cancelled : startup->failed()),
              "stopped fade cannot finish a failed or cancelled startup later");
    }
    std::printf("PASS: %d startup checks; isolated software windows, no main application or user configuration.\n",
                checks);
}
