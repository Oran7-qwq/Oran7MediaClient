#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QQuickImageProvider>
#include <QSqlDatabase>
#include <QLoggingCategory>
#include <QQmlContext>
#include <memory>

#include "GlobalEventFilter.h"
#include "AppJsonConfigManager.h"
//#include "Sever.h"
#include "Oran7MediaClient.h"
#include "ApplicationContext.h"
#include "BilibliLiveRoomAddressCatch.h"
#include "BilibiliDanmakuClient.h"
#include "BilibiliAuthManager.h"
#include "Oran7MusicSearchService.h"
#include "Oran7NetworkMonitor.h"
#include "Oran7Async.h"
#include "Oran7AsyncManager.h"
#include "NcmSession.h"
#include "QrLoginController.h"
#include "WebLoginController.h"
#include "NcmPlaylistApi.h"
#include "Oran7Theme.h"
#include "LyricsModel.h"
#include "Oran7SoundEffect.h"
#include "ConsoleLogger.h"


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

// 全局未处理异常处理器：崩溃时自动生成 minidump，便于定位崩溃位置
LONG WINAPI Oran7CrashHandler(EXCEPTION_POINTERS* ep)
{
    static const wchar_t* dumpPath =
        L"Oran7MediaClient_crash.dmp";
    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = TRUE;
        // 保留全部内存与线程栈，便于用 cdb 解析完整调用栈
        const MINIDUMP_TYPE dumpType =
            static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory
                                       | MiniDumpWithHandleData
                                       | MiniDumpWithThreadInfo
                                       | MiniDumpWithProcessThreadData
                                       | MiniDumpWithModuleHeaders);
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          dumpType, &mei, nullptr, nullptr);
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
        PSETPROCESSDPIAWARE pSetProcessDPIAware =
            (PSETPROCESSDPIAWARE)GetProcAddress(hUser32, "SetProcessDPIAware");
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
ConfigRET Config_AppConfigManager_Load(QQmlApplicationEngine &engine);
ConfigRET Config_AppConfigManager_Save(QQmlApplicationEngine &engine);

#undef main
int main(int argc, char *argv[])
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

    // threaded 渲染循环
    qputenv("QSG_RENDER_LOOP", "basic");
    qputenv("QSG_NO_VSYNC", "1");

    // 强制 Qt Multimedia 使用 Windows Media Foundation 后端
    // 避免加载 ffmpegmediaplugin 时与项目自带的 FFmpeg DLL 冲突
    qputenv("QT_MEDIA_BACKEND", "windows");
    // qputenv("QSG_RHI_BACKEND", "opengl");
    // qputenv("QSG_RHI_PROFILE", "false");
    // qputenv("QSG_INFO", "1");
    //  qputenv("QSG_RHI_BACKEND", "opengl");//Discard
    // qputenv("QSG_RHI_VALIDATE_LAYER", "0");// 禁用RHI验证
#endif

    // QT_LOGGING_RULES
    qputenv("QT_LOGGING_RULES",
            "qt.scenegraph.general=true\n"
            "qt.scenegraph.renderloop=true\n"
            "qt.rhi.general=true\n"
            "qt.rhi.warning=true\n");

    QLoggingCategory::setFilterRules("qt.quick.shadereffect.warning=false\n");

    // Config SurfaceFormat
    QSurfaceFormat format;
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);          // 启用VSync
    format.setDepthBufferSize(32);
    format.setStencilBufferSize(16);
    format.setSamples(1);                  // 1x(关闭MSAA,减少帧缓冲/离屏纹理内存;该UI为平面+shader效果,视觉无差别)
    format.setAlphaBufferSize(16);
    format.setVersion(3, 3);                // 使用兼容性配置文件
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(format);

    // 不使用 OpenGL 共享，删除 AA_ShareOpenGLContexts
    // QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // 附加日志控制台窗口
    ATTACH_CONSOLE("Oran7MediaClient Logs");

    // 确保 Qt 窗口 backing store 始终带 alpha 通道（透明窗口必须）
    QQuickWindow::setDefaultAlphaBuffer(true);

    QGuiApplication app(argc, argv);

    // 设置场景图形后端-->Direct3D11
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QQuickWindow::setTextRenderType(QQuickWindow::QtTextRendering);
    QQuickStyle::setStyle("Fusion");

    // Qt 默认 backend-->Windows : D3D11
    INFO_LOG << "Graphics API:" << QQuickWindow::graphicsApi();

    app.setWindowIcon(QIcon(":/image/Oran7.jpg"));
    app.setApplicationName("Oran7MediaClient");
    app.setOrganizationName("Oran7Apps");

    QQmlApplicationEngine engine;

    // 确保全局单例被创建-->
    ApplicationContext::instance();

    qmlRegisterSingletonType(QUrl("qrc:/src/qml/Basic/BasicConfig.qml"), "BasicConfig", 1, 0, "BasicConfig");
    qmlRegisterSingletonType(QUrl("qrc:/src/qml/Settings/GlobalSettings/Oran7MainUiSetting.qml"), "Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting");

    //qmlRegisterType<Oran7FileHelper>("Oran7FileHelper", 1, 0, "Oran7FileHelper");
    qmlRegisterType<BilibiliRoomAddressCatch>("BilibiliRoomAddressCatch", 1, 0, "BilibiliRoomAddressCatch");
    qmlRegisterType<BilibiliDanmakuClient>("BilibiliDanmakuClient", 1, 0, "BilibiliDanmakuClient");
    qmlRegisterType<D3D11VideoItem>("D3D11VideoItem", 1, 0, "D3D11VideoItem");
    qmlRegisterType<LyricsModel>("LyricsModel", 1, 0, "LyricsModel");
    qmlRegisterType<Oran7SoundEffect>("Oran7Sound", 1, 0, "Oran7SoundEffect");
    //qmlRegisterType<FramelessWindow>("FramelessWindow", 1, 0, "FramelessWindow");

    // B站认证管理器单例
    qmlRegisterSingletonType<BilibiliAuthManager>("BilibiliAuth", 1, 0, "BilibiliAuthManager",
        [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return &BilibiliAuthManager::instance();
        });

    // 网易云音乐搜索服务单例
    qmlRegisterSingletonType<Oran7MusicSearchService>("Oran7MusicSearch", 1, 0, "Oran7MusicSearchService",
        [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return &Oran7MusicSearchService::instance();
        });

    // 网易云扫码登录会话单例（isLoggedIn / userName / logout）
    qmlRegisterSingletonType<NcmSession>("Oran7NetEaseAuth", 1, 0, "NcmSession",
        [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return &NcmSession::instance();
        });

    // 网易云扫码登录状态机控制器（弹窗用：start / loginState / qrCodeSource）
    qmlRegisterSingletonType<QrLoginController>("Oran7NetEaseAuth", 1, 0, "NcmQrLogin",
        [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return NcmSession::instance().qrController();
        });

    // 网易云网页登录控制器（start / loginState / statusText）
    qmlRegisterSingletonType<WebLoginController>("Oran7NetEaseAuth", 1, 0, "NcmWebLogin",
        [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return NcmSession::instance().webLoginController();
        });

    // 网易云"我喜欢的音乐"歌单 API（fetchLikedSongs / setLike）
    qmlRegisterSingletonType<NcmPlaylistApi>("Oran7NetEaseAuth", 1, 0, "NcmPlaylistApi",
        [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return NcmSession::instance().playlistApi();
        });

    // 全局网络监测器单例（延迟 / 在线状态 / Cookie 失效自动重登）
    qmlRegisterSingletonType<Oran7NetworkMonitor>("Oran7Network", 1, 0, "Oran7NetworkMonitor",
        [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return &Oran7NetworkMonitor::instance();
        });

    // 全局异步管理器单例（QML 侧：Async.run(operation, args, completed, failed)）
    qmlRegisterSingletonType<Oran7AsyncManager>("Oran7Async", 1, 0, "Oran7AsyncManager",
        [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return new Oran7AsyncManager();
        });

    // 注册全局事件过滤器到 QML
    // 使用 std::unique_ptr 确保自动清理，但保留原始指针用于 installEventFilter
    std::unique_ptr<GlobalEventFilter> globalFilterPtr = std::make_unique<GlobalEventFilter>(&app);
    app.installEventFilter(globalFilterPtr.get());
    engine.rootContext()->setContextProperty("globalEventFilter", globalFilterPtr.get());

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl)
        {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    // Sever sever;
    // sever.startServer(55711);

    Client *client = ApplicationContext::instance().client();
    qmlRegisterSingletonType<Client>("Client", 1, 0, "Client",
         [client](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject *
         {
             Q_UNUSED(engine);
             Q_UNUSED(scriptEngine);
             return client;
         });

    if (!QSqlDatabase::isDriverAvailable("QSQLITE")){
        qCritical() << "SQLite driver not available!";
        return -1;
    }

    // 网易云扫码二维码 image provider（本地生成，image://ncmqr/latest?v=N）
    // 此处同时确保 NcmSession 先于 QML 加载完成实例化
    engine.addImageProvider(QStringLiteral("ncmqr"),
        NcmSession::instance().qrController()->createQrImageProvider());

    engine.load(url);

    Config_AppConfigManager_RET = Config_AppConfigManager_Load(engine);
    if (Config_AppConfigManager_RET == ConfigRET::Config_SUCCESSED)
        CONFIG_LOG << "AppConfigManager:Successfully load user Config,";

    QObject::connect(&app, &QGuiApplication::aboutToQuit, &app, [&engine]() mutable{
        // 先确保杀死Oran7MediaPlayer防止Video_refresh还在触发回调qml渲染对象
        ApplicationContext::instance().client()->stopPlayerRunning();

        // 进入退出流程：禁止 Logger/Oran7Async 再向线程池投递新任务，
        // 避免退出阶段创建线程（拒绝访问）以及线程池随静态析构时仍在等待。
        Oran7Async::beginShutdown();
        Oran7AsyncManager::beginShutdown();

        // 在事件循环还活着时安全销毁BilibiliAuthManager（含QTimer/QNetworkAccessManager）
        // 先写回（含已修复解码的 Cookie），再清理，避免清理后 saveCookies 落到空实例上
        BilibiliAuthManager::instance().saveCookies();
        BilibiliAuthManager::cleanup();

        // 网易云音乐搜索服务（含 QNetworkAccessManager）
        Oran7MusicSearchService::cleanup();

        // 全局网络监测器（停止定时器，并先于 NcmSession 断开其连接）
        Oran7NetworkMonitor::cleanup();

        // 网易云扫码登录会话（含 QNetworkAccessManager/QTimer/ImageProvider）
        NcmSession::cleanup();

        Config_AppConfigManager_RET = Config_AppConfigManager_Save(engine);
        if(Config_AppConfigManager_RET == ConfigRET::Config_SUCCESSED)
            CONFIG_LOG<<"AppConfigManager:Successfully save user Config.";

        // ===== 在事件循环停止前清理所有后台线程和资源 =====
        // 1. 等待 Oran7Async 线程池中所有任务完成（避免直接 delete 运行中的工作；
        //    已在 beginShutdown 后无新任务投递，可安全等待到全部结束）
        Oran7Async::waitForDone();

        // 2. 断开 QTcpSocket，防止连接挂起
        Client* client = ApplicationContext::instance().client();
        if (client) {QTcpSocket* sock = client->findChild<QTcpSocket*>();
            if (sock) { sock->abort(); }
        }
    });

    ///Singleton依赖:析构链路Oran7Theme.Oran7ThemePrivate
    qAddPostRoutine([](){
        // BilibiliAuthManager 已在 aboutToQuit 中提前销毁，此处不再处理
        Oran7Theme::cleanup();
    });

    return app.exec();
}

ConfigRET Config_AppConfigManager_Load(QQmlApplicationEngine &engine)
{
    try
    {
        AppConfigManager::ins().loadConfig(); //<-----一定要首先加载

        ApplicationContext::instance().client()->loadConfig_AppWindowPosition(engine);
        ApplicationContext::instance().client()->loadConfig_AppWindowSize(engine);

        QString SearchLocalMediaFiles_folderPath = ApplicationContext::instance().client()->createAppDirectories();
        if (SearchLocalMediaFiles_folderPath.isEmpty())
            throw std::runtime_error("AppData Temp folder in Oran7MediaClient Could not be Created correctly");
        ApplicationContext::instance().client()->loadConfig_localMusicList_playOrder(); //*加载localMusicList自定义排序Config配置
        ApplicationContext::instance().client()->searchLocalMediaFiles(SearchLocalMediaFiles_folderPath); //*Load localMusic列表（Async）
        ApplicationContext::instance().client()->loadConfig_lastCloseAppFocusedMusic();//*加载last focus music info
        ApplicationContext::instance().client()->loadConfig_AppSetPlayerVolume();//*Load PlayerVolume Config
        BilibiliAuthManager::instance().loadCookies(); //*加载B站认证Cookie
        NcmSession::instance().load();                          //*加载网易云登录Cookie
        Oran7NetworkMonitor::instance().start();                //*启动网络延迟监测（联网且延迟<180ms 时自动校验 Cookie）
    }
    catch (std::exception &e)
    {
        return Config_ERROR;
    }

    return Config_SUCCESSED;
}

ConfigRET Config_AppConfigManager_Save(QQmlApplicationEngine &engine)
{
    try
    {
        ApplicationContext::instance().client()->saveConfig_lastCloseAppFocusedMusic(); //* 保存客户端最近一次播放的文件
        ApplicationContext::instance().client()->saveConfig_localMusicList_playOrder(); //* 保存Custom自定义LocalMusicList_playOrder的Config配置
        ApplicationContext::instance().client()->saveConfig_AppSetPlayerVolume();

        ApplicationContext::instance().client()->saveConfig_AppWindowSize(engine);
        ApplicationContext::instance().client()->saveConfig_AppWindowPosition(engine);

        BilibiliAuthManager::instance().saveCookies();  //*保存B站认证Cookie

        AppConfigManager::ins().saveConfig(); // 确保程序退出前保存配置
    }
    catch (std::exception &e)
    {
        return Config_ERROR;
    }
    return Config_SUCCESSED;
}
