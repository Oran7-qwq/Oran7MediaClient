#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "D3d11Videoitem.h"
#include "Oran7HdrWindowOutput.h"
#include <QQuickWindow>
#include <QQuickStyle>
#include <QQuickImageProvider>
#include <QSqlDatabase>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QStandardPaths>
#include "Oran7CoverNetworkFactory.h"
#include <memory>

#include "Oran7GlobalEventFilter.h"
#include "Oran7AppConfigManager.h"
#include "Oran7MediaClient.h"
#include "Oran7AudioEffects.h"
#include "Oran7EqualizerModel.h"
#include "Oran7ApplicationContext.h"
#include "BilibliLiveRoomAddressCatch.h"
#include "BilibiliDanmakuClient.h"
#include "Oran7BilibiliAuthManager.h"
#include "Oran7NetworkMonitor.h"
#include "Oran7Async.h"
#include "Oran7AsyncManager.h"
#include "NcmSession.h"
#include "WebLoginController.h"
#include "NcmPlaylistApi.h"
#include "Oran7Theme.h"
#include "Oran7UiPreferences.h"
#include "src/cpp/Oran7Startup/Oran7StartupCoordinator.h"
#include "src/cpp/Oran7Startup/Oran7BackgroundFrameMonitor.h"
#include "src/cpp/Oran7Startup/Oran7StartupActivityItem.h"
#include "Oran7LyricsModel.h"
#include "Oran7SoundEffect.h"
#include "Oran7ConsoleLogger.h"
#include "Oran7LogSink.h"
#include "Oran7FFmpegLogBridge.h"
#include <QScopeGuard>
#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
#include <rtc/global.hpp>
#endif

#include <QtGlobal>
#include <QDebug>
#include <QLoggingCategory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <dbghelp.h>

extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

/*!  全局未处理异常处理器 */
LONG WINAPI Oran7CrashHandler(EXCEPTION_POINTERS* ep)
{
    static const wchar_t* dumpPath = L"Oran7MediaClient_crash.dmp";
    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = TRUE;
        const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithProcessThreadData |
            MiniDumpWithModuleHeaders); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, dumpType, &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void disableDPIVirtualization()
{
    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    if (hUser32)
    {
        typedef BOOL (*PSETPROCESSDPIAWARE)(void);
        PSETPROCESSDPIAWARE pSetProcessDPIAware = (PSETPROCESSDPIAWARE)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (pSetProcessDPIAware)
        {
            pSetProcessDPIAware();
        }
        FreeLibrary(hUser32);
    }
}
#endif

enum ConfigRET
{
    Config_NONE = -2,
    Config_ERROR = -1,
    Config_SUCCESSED = 0
};
ConfigRET Config_AppConfigManager_RET = Config_NONE;
ConfigRET Config_AppConfigManager_Load(QQmlApplicationEngine& engine);
ConfigRET Config_AppConfigManager_Save(QQmlApplicationEngine& engine);

#undef main
int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
#ifdef Q_OS_WIN
    QLoggingCategory::setFilterRules("qt.gui.imageio=false");
    QLoggingCategory::setFilterRules("qt.core.qobject.warning=true");
    QLoggingCategory::setFilterRules("qt.quick.warning=true");
    disableDPIVirtualization();
    // 崩溃时自动生成 minidump（Oran7MediaClient_crash.dmp），用于定位崩溃
    SetUnhandledExceptionFilter(Oran7CrashHandler);
    qputenv("QSG_RENDER_LOOP", "basic");
    qputenv("QSG_NO_VSYNC", "1");
    qputenv("QT_MEDIA_BACKEND", "windows");
#endif

    // QT_LOGGING_RULES
    qputenv("QT_LOGGING_RULES", "qt.scenegraph.general=true\n"
                                "qt.scenegraph.renderloop=true\n"
                                "qt.rhi.general=true\n"
                                "qt.rhi.warning=true\n");

    QLoggingCategory::setFilterRules("qt.quick.shadereffect.warning=false\n");

    // Config SurfaceFormat
    QSurfaceFormat format;
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);
    format.setDepthBufferSize(32);
    format.setStencilBufferSize(16);
    format.setSamples(1);
    format.setAlphaBufferSize(16);
    format.setVersion(3, 3); // 使用兼容性配置文件
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(format);

    // 不使用 OpenGL 共享，删除 AA_ShareOpenGLContexts
    // QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // 附加日志控制台窗口
    ATTACH_CONSOLE("Oran7MediaClient Logs");

    // 后声明的引擎、应用先销毁，最后排空日志；也覆盖初始化失败的提前返回。
    Oran7LogSink* logSink = nullptr;
    const auto stopLogging = qScopeGuard(
        [&]
        {
            if (logSink)
                logSink->shutdown();
        });
    QGuiApplication app(argc, argv);
    app.setApplicationName("Oran7MediaClient");
    app.setOrganizationName("Oran7Apps");
    logSink = &Oran7LogSink::global();
    logSink->installQtHandler();
    Oran7FFmpegLogBridge::install();
#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
    // 仅接管日志，不启动推流或信令连接。
    rtc::InitLogger(rtc::LogLevel::Warning,
                    [](rtc::LogLevel level, std::string message)
                    {
                        QDebug log(level <= rtc::LogLevel::Error ? QtCriticalMsg : QtWarningMsg);
                        log.noquote().nospace() << "[WebRTC] " << QString::fromStdString(message);
                    });
#endif
    // 确保 Qt 窗口 backing store 始终带 alpha 通道（透明窗口必须）
    QQuickWindow::setDefaultAlphaBuffer(true);
    // 设置场景图形后端-->Direct3D11
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QQuickWindow::setTextRenderType(QQuickWindow::QtTextRendering);
    QQuickStyle::setStyle("Fusion");

    // Qt 默认 backend-->Windows : D3D11
    INFO_LOG << "Graphics API:" << QQuickWindow::graphicsApi();

    app.setWindowIcon(QIcon(":/image/Oran7.jpg"));

    Oran7UiPreferences uiPreferences(Oran7AppConfigManager::ins());
    // 上下文服务晚于引擎析构，保证 QML 销毁时借用对象仍存活。
    std::unique_ptr<Oran7GlobalEventFilter> globalFilterPtr;
    std::unique_ptr<Oran7StartupCoordinator> startup;
    Oran7MediaClient* client = nullptr;
    bool configurationRestored = false;
    Oran7CoverNetworkFactory coverNetworkFactory(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/qml-covers-v1");
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&coverNetworkFactory);
    uiPreferences.attachEngine(&engine);
    QQmlEngine::setObjectOwnership(&uiPreferences, QQmlEngine::CppOwnership);
    engine.rootContext()->setContextProperty("Oran7UiPreferences", &uiPreferences);
    startup = std::make_unique<Oran7StartupCoordinator>(engine);
    qmlRegisterType<Oran7StartupActivityItem>("Oran7Startup", 1, 0, "Oran7StartupActivityItem");
    QQmlEngine::setObjectOwnership(startup.get(), QQmlEngine::CppOwnership);
    engine.rootContext()->setContextProperty("Oran7Startup", startup.get());
    QObject::connect(startup.get(), &Oran7StartupCoordinator::quitRequested, &app, &QGuiApplication::quit,
                     Qt::QueuedConnection);

    Oran7StartupCoordinator::Oran7Options options;
    options.splashUrl = QUrl(QStringLiteral("qrc:/src/qml/Components/Oran7GlobalComponents/Oran7StartupWindow.qml"));
    options.mainUrl = QUrl(QStringLiteral("qrc:/main.qml"));
    options.maximized = Oran7AppConfigManager::ins().snapshot().window.maximized;
    options.initialize = [&]() -> QString
    {
        // 确保全局单例被创建-->
        client = Oran7ApplicationContext::instance().client();
        qmlRegisterType<Oran7BackgroundFrameMonitor>("Oran7Startup", 1, 0, "Oran7BackgroundFrameMonitor");
        qmlRegisterSingletonType(QUrl("qrc:/src/qml/Basic/BasicConfig.qml"), "BasicConfig", 1, 0, "BasicConfig");
        qmlRegisterSingletonType(QUrl("qrc:/src/qml/Settings/GlobalSettings/Oran7MainUiSetting.qml"),
                                 "Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting");

        // qmlRegisterType<Oran7FileHelper>("Oran7FileHelper", 1, 0, "Oran7FileHelper");
        qmlRegisterType<BilibiliRoomAddressCatch>("BilibiliRoomAddressCatch", 1, 0, "BilibiliRoomAddressCatch");
        qmlRegisterType<BilibiliDanmakuClient>("BilibiliDanmakuClient", 1, 0, "BilibiliDanmakuClient");
        qmlRegisterType<D3D11VideoItem>("D3D11VideoItem", 1, 0, "D3D11VideoItem");
        qmlRegisterType<Oran7HdrWindowOutput>("D3D11VideoItem", 1, 0, "Oran7HdrWindowOutput");
        qmlRegisterType<Oran7LyricsModel>("LyricsModel", 1, 0, "LyricsModel");
        qmlRegisterType<Oran7SoundEffect>("Oran7Sound", 1, 0, "Oran7SoundEffect");

        // B站认证管理器单例
        qmlRegisterSingletonType<Oran7BilibiliAuthManager>("BilibiliAuth", 1, 0, "BilibiliAuthManager",
                                                           [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
                                                           {
                                                               Q_UNUSED(engine);
                                                               Q_UNUSED(scriptEngine);
                                                               return &Oran7BilibiliAuthManager::instance();
                                                           });

        // 网易云扫码登录会话单例（isLoggedIn / userName / logout）
        qmlRegisterSingletonType<NcmSession>("Oran7NetEaseAuth", 1, 0, "NcmSession",
                                             [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
                                             {
                                                 Q_UNUSED(engine);
                                                 Q_UNUSED(scriptEngine);
                                                 return &NcmSession::instance();
                                             });

        // 网易云网页登录控制器（start / loginState / statusText）
        qmlRegisterSingletonType<WebLoginController>("Oran7NetEaseAuth", 1, 0, "NcmWebLogin",
                                                     [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
                                                     {
                                                         Q_UNUSED(engine);
                                                         Q_UNUSED(scriptEngine);
                                                         return NcmSession::instance().webLoginController();
                                                     });

        // 网易云喜欢/排序写操作 API；读取由共享异步 handler 负责
        qmlRegisterSingletonType<NcmPlaylistApi>("Oran7NetEaseAuth", 1, 0, "NcmPlaylistApi",
                                                 [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
                                                 {
                                                     Q_UNUSED(engine);
                                                     Q_UNUSED(scriptEngine);
                                                     return NcmSession::instance().playlistApi();
                                                 });

        // 全局网络监测器单例（延迟 / 在线状态 / Cookie 失效自动重登）
        qmlRegisterSingletonType<Oran7NetworkMonitor>("Oran7Network", 1, 0, "Oran7NetworkMonitor",
                                                      [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
                                                      {
                                                          Q_UNUSED(engine);
                                                          Q_UNUSED(scriptEngine);
                                                          return &Oran7NetworkMonitor::instance();
                                                      });

        // 全局异步管理器单例（QML 侧：Async.run(operation, args, completed, failed)）
        qmlRegisterSingletonType<Oran7AsyncManager>("Oran7Async", 1, 0, "Oran7AsyncManager",
                                                    [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
                                                    {
                                                        Q_UNUSED(engine);
                                                        Q_UNUSED(scriptEngine);
                                                        return new Oran7AsyncManager();
                                                    });

        // 注册全局事件过滤器到 QML
        // 使用 std::unique_ptr 确保自动清理，但保留原始指针用于 installEventFilter
        globalFilterPtr = std::make_unique<Oran7GlobalEventFilter>();
        app.installEventFilter(globalFilterPtr.get());
        engine.rootContext()->setContextProperty("globalEventFilter", globalFilterPtr.get());

        qmlRegisterUncreatableType<Oran7TrackListModel>("Client", 1, 0, "Oran7TrackListModel",
                                                        QStringLiteral("曲目列表由 Client 持有"));
        qmlRegisterUncreatableType<Oran7CloudMusicService>("Client", 1, 0, "Oran7CloudMusicService", "由 Client 持有");
        qmlRegisterUncreatableType<Oran7VideoHistory>("Client", 1, 0, "Oran7VideoHistory", "由 Client 持有");
        qmlRegisterUncreatableType<Oran7PlaybackViewModel>("Client", 1, 0, "Oran7PlaybackViewModel", "由 Client 持有");
        qmlRegisterUncreatableType<Oran7TrackLibrary>("Client", 1, 0, "Oran7TrackLibrary",
                                                      QStringLiteral("曲目集合由 Client 持有"));
        qmlRegisterUncreatableType<Oran7PlaybackState>("Client", 1, 0, "Oran7PlaybackState",
                                                       QStringLiteral("播放状态由 Oran7MediaClient 持有"));
        qmlRegisterUncreatableType<Oran7EqualizerModel>("Client", 1, 0, "Oran7EqualizerModel",
                                                        QStringLiteral("均衡器模型由 Client.audioEffects 持有"));
        qmlRegisterUncreatableType<Oran7AudioEffects>("Client", 1, 0, "Oran7AudioEffects",
                                                      QStringLiteral("音效由 Client 持有"));

        qmlRegisterSingletonType<Oran7MediaClient>("Client", 1, 0, "Client",
                                                   [client](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
                                                   {
                                                       Q_UNUSED(engine);
                                                       Q_UNUSED(scriptEngine);
                                                       return client;
                                                   });
        if (!QSqlDatabase::isDriverAvailable("QSQLITE"))
            return QStringLiteral("SQLite 驱动不可用");
        return {};
    };
    options.restore = [&](QQuickWindow* window) -> QString
    {
        Config_AppConfigManager_RET = Config_AppConfigManager_Load(engine);
        if (Config_AppConfigManager_RET != Config_SUCCESSED)
            return QStringLiteral("恢复配置失败");
        // 主窗仍隐藏，先保存正常窗口几何，再按配置显示/最大化。
        if (!QMetaObject::invokeMethod(window, "prepareStartupGeometry"))
            return QStringLiteral("主界面缺少几何初始化接口");
        configurationRestored = true;
        return {};
    };
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &app,
                     [&]
                     {
                         if (!startup->finished())
                             startup->cancel();
                         if (client)
                             client->shutdown();
                         Oran7Async::beginShutdown();
                         Oran7AsyncManager::beginShutdown();
                         // 未完成交接时不把默认/部分窗口几何和未初始化账号写回配置。
                         // 必须在认证管理器清理前保存，不能清理后再 instance() 重建它。
                         if (configurationRestored && startup->finished())
                         {
                             Config_AppConfigManager_RET = Config_AppConfigManager_Save(engine);
                             if (Config_AppConfigManager_RET != Config_SUCCESSED)
                                 WARNING_LOG << "配置或凭据保存失败：" << Oran7AppConfigManager::ins().lastError();
                         }
                         Oran7NetworkMonitor::cleanup();
                         Oran7BilibiliAuthManager::cleanup();
                         NcmSession::cleanup();
                         Oran7Async::waitForDone();
                     });
    qAddPostRoutine([] { Oran7Theme::cleanup(); });
    startup->start(std::move(options));
    return app.exec();
}

ConfigRET Config_AppConfigManager_Load(QQmlApplicationEngine& engine)
{
    try{
        /*! @brief must be first loaded*/
        Oran7AppConfigManager::ins().loadConfig();

        if (!Oran7ApplicationContext::instance().client()->restoreConfiguration(engine))
            throw std::runtime_error("无法创建媒体客户端缓存目录");
        Oran7BilibiliAuthManager::instance().loadCookies(); //*加载B站认证Cookie
        NcmSession::instance().load();                                    //*加载网易云登录Cookie
        Oran7NetworkMonitor::instance().start();                  //*启动网络延迟监测（联网且延迟<180ms 时自动校验 Cookie）
    }
    catch (std::exception& e){
        return Config_ERROR;
    }
    return Config_SUCCESSED;
}

ConfigRET Config_AppConfigManager_Save(QQmlApplicationEngine& engine)
{
    try{
        Oran7ApplicationContext::instance().client()->saveConfiguration(engine);
        Oran7BilibiliAuthManager::instance().saveCookies(); //*保存B站认证Cookie
        if (!Oran7AppConfigManager::ins().saveConfig())
            return Config_ERROR;
    }
    catch (std::exception& e){
        return Config_ERROR;
    }
    return Config_SUCCESSED;
}
