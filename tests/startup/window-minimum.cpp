#define NOMINMAX
#include "Oran7WindowAgent.h"
#include "Oran7StartupCoordinator.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QFile>
#include <QTest>
#include <windows.h>
#include <cstdio>
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
static void write(const QString& path, const QByteArray& data)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "write temporary geometry fixture");
}
int main(int argc, char** argv)
{
    // 只建立隔离的原生透明窗口，用软件场景图；不加载主 GUI、配置或媒体，不采集桌面。
    qputenv("QT_QPA_PLATFORM", "windows");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RENDER_LOOP", "basic");
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QQuickWindow::setDefaultAlphaBuffer(true);
    qmlRegisterType<Oran7WindowAgent>("Oran7GeometryCheck", 1, 0, "Oran7WindowAgent");
    QTemporaryDir temporary;
    write(temporary.filePath("Splash.qml"), "import QtQuick\nWindow { objectName: \"__Oran7StartupWindow__\"; "
                                            "width:100; height:60; opacity:0; visible:true }");
    write(temporary.filePath("Main.qml"),
          "import QtQuick\nimport Oran7GeometryCheck 1.0\n"
          "Window { objectName: \"__Oran7Window__\"; width:990; height:564; minimumWidth:990; minimumHeight:564;"
          "visible:false; color:\"transparent\"; property bool startupPageReady:false; Oran7WindowAgent {} }");
    for (const int height : {564, 600, 640})
    {
        std::unique_ptr<Oran7StartupCoordinator> startup;
        QQmlApplicationEngine engine;
        startup = std::make_unique<Oran7StartupCoordinator>(engine);
        engine.rootContext()->setContextProperty("Oran7Startup", startup.get());
        bool restored = false;
        Oran7StartupCoordinator::Oran7Options options;
        options.splashUrl = QUrl::fromLocalFile(temporary.filePath("Splash.qml"));
        options.mainUrl = QUrl::fromLocalFile(temporary.filePath("Main.qml"));
        options.restore = [&](QQuickWindow* window)
        {
            window->resize(990, height);
            restored = true;
            return QString();
        };
        startup->start(options);
        check(startup->splashWindow(), "isolated zero-opacity splash created");
        QMetaObject::invokeMethod(startup->splashWindow(), "frameSwapped");
        QElapsedTimer elapsed;
        elapsed.start();
        while (elapsed.elapsed() < 4000 && startup->stage() != Oran7StartupCoordinator::Oran7Stage::PreparingDisplay)
        {
            if (engine.incubationController())
                engine.incubationController()->incubateFor(2);
            QTest::qWait(5);
        }
        auto* window = startup->mainWindow();
        check(restored && window, "production startup coordinator applied restoration");
        QTest::qWait(25);
        std::fprintf(stderr, "restored client height=%d actual=%d minimum=%d dpr=%.3f\n", height, window->height(),
                     window->minimumHeight(), window->devicePixelRatio());
        check(window->opacity() == 0.0, "native geometry check never exposes visible GUI content");
        check(window->frameMargins().top() == 0, "production window agent removed the native title bar");
        check(window->size() == QSize(990, height),
              "first native show must preserve restored client size including minimum height");
        MINMAXINFO info{};
        const auto hwnd = reinterpret_cast<HWND>(window->winId());
        SendMessageW(hwnd, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&info));
        SetWindowPos(hwnd, nullptr, 0, 0, info.ptMinTrackSize.x, info.ptMinTrackSize.y,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        check(window->height() == 564, "native minimum tracking agrees with QML minimum height");
        startup->cancel();
    }
    std::printf("PASS: %d native minimum-height checks; software rendering, zero-opacity fixtures, no main app.\n",
                checks);
}
