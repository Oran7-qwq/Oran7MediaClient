#include "Oran7StartupCoordinator.h"
#include "Oran7StartupActivityItem.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTest>
#include <cstdio>
#include <memory>

// 实际进入 app.exec()，验证窗口 close 接受策略与 Quit 事件；不能只检查 cancel 信号。
int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RENDER_LOOP", "basic");
    QGuiApplication app(argc, argv);
    qmlRegisterType<Oran7StartupActivityItem>("Oran7Startup", 1, 0, "Oran7StartupActivityItem");
    app.setQuitOnLastWindowClosed(false);
    using Stage = Oran7StartupCoordinator::Oran7Stage;
    int cases = 0;
    for (const auto phase : {Stage::ShowingSplash, Stage::LoadingUi, Stage::PreparingDisplay, Stage::Failed})
    for (bool nativeClose : {false, true})
    {
        std::unique_ptr<Oran7StartupCoordinator> startup;
        QQmlApplicationEngine engine;
        startup = std::make_unique<Oran7StartupCoordinator>(engine);
        engine.rootContext()->setContextProperty("Oran7Startup", startup.get());
        bool requested = false, acted = false, timedOut = false, quit = false;
        QObject::connect(startup.get(), &Oran7StartupCoordinator::quitRequested, &app, &QGuiApplication::quit,
                         Qt::QueuedConnection);
        const auto onQuit = QObject::connect(&app, &QGuiApplication::aboutToQuit, [&] { quit = true; });
        QObject::connect(startup.get(), &Oran7StartupCoordinator::changed, &engine, [&] {
            if (requested || startup->stage() != phase) return;
            requested = true;
            QTimer::singleShot(0, &engine, [&] {
                auto* window = startup->splashWindow();
                if (!window) return;
                acted = true;
                if (nativeClose) window->close();
                else {
                    auto* button = window->findChild<QQuickItem*>("startupClose");
                    if (!button) { acted = false; return; }
                    const auto point = button->mapToScene(QPointF(button->width()/2, button->height()/2));
                    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point.toPoint());
                }
            });
        });
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, &app, [&] {
            timedOut = true;
            app.exit(91); // 仅测试 watchdog；生产不得绕过正常退出清理。
        });
        Oran7StartupCoordinator::Oran7Options options;
        options.splashUrl = QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/src/qml/Components/Oran7GlobalComponents/Oran7StartupWindow.qml");
        options.mainUrl = QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/startup/MainWindow.qml");
        options.timeoutMs = 1000;
        if (phase == Stage::Failed) options.initialize = [] { return QStringLiteral("fixture initialization failure"); };
        startup->start(options);
        deadline.start(2000);
        const auto result = app.exec();
        deadline.stop();
        QObject::disconnect(onQuit);
        if (result || timedOut || !acted || !quit || startup->stage() != Stage::Cancelled) {
            std::fprintf(stderr, "FAIL: startup close phase=%d native=%d result=%d acted=%d timeout=%d\n",
                         int(phase), nativeClose, result, acted, timedOut);
            return 1;
        }
        ++cases;
    }
    std::printf("PASS: %d real event-loop startup close cases; isolated windows, no main app or user configuration.\n", cases);
}
