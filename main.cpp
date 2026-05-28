#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSqlDatabase>
#include <QLoggingCategory>
#include <QQmlContext>
#include <memory>

#include "GlobalEventFilter.h"
#include "AppJsonConfigManager.h"
//#include "Sever.h"
#include "Client.h"
#include "ApplicationContext.h"
#include "BilibliLiveRoomAddressCatch.h"
#include "Oran7Theme.h"
#include "ConsoleLogger.h"


#include <QtGlobal>
#include <QDebug>
#include <QLoggingCategory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>

extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
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

    // threaded 渲染循环
    qputenv("QSG_RENDER_LOOP", "basic");
    qputenv("QSG_NO_VSYNC", "1");
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
    format.setSwapInterval(1); // 启用VSync
    format.setDepthBufferSize(32);
    format.setStencilBufferSize(16);
    format.setSamples(4); // 4x MSAA
    format.setAlphaBufferSize(16);
    format.setVersion(3, 3); // 使用兼容性配置文件
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(format);

    // 不使用 OpenGL 共享，删除 AA_ShareOpenGLContexts
    // QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // 附加日志控制台窗口
    ATTACH_CONSOLE("Oran7MediaClient Logs");

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
    qmlRegisterType<D3D11VideoItem>("D3D11VideoItem", 1, 0, "D3D11VideoItem");
    //qmlRegisterType<FramelessWindow>("FramelessWindow", 1, 0, "FramelessWindow");

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

    engine.load(url);

    Config_AppConfigManager_RET = Config_AppConfigManager_Load(engine);
    if (Config_AppConfigManager_RET == ConfigRET::Config_SUCCESSED)
        CONFIG_LOG << "AppConfigManager:Successfully load user Config,";

    QObject::connect(&app, &QGuiApplication::aboutToQuit, &app, [&engine]() mutable{
        ApplicationContext::instance().client()->StopPlayerRuning();//先确保杀死Oran7MediaPlayer防止Video_refresh还在触发回调qml渲染对象

        Config_AppConfigManager_RET = Config_AppConfigManager_Save(engine);
        if(Config_AppConfigManager_RET == ConfigRET::Config_SUCCESSED)
            CONFIG_LOG<<"AppConfigManager:Successfully save user Config."; });

    ///Singleton依赖:析构链路AppConfigManager.Oran7ThemeProfileManager.Oran7Theme.Oran7ThemePrivate
    qAddPostRoutine([](){
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
        ApplicationContext::instance().client()->loadConfig_localMusicList_playOrder();                                  //*加载localMusicList自定义排序Config配置
        ApplicationContext::instance().asyncWorker()->startSearchLocalMediaFiles_Task(SearchLocalMediaFiles_folderPath); //*Load localMusic列表（Async）
        ApplicationContext::instance().client()->loadConfig_lastCloseAppFocusedMusic();                                  //*加载last focus music info
        ApplicationContext::instance().client()->loadConfig_AppSetPlayerVolume();                                        //*Load PlayerVolume Config
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

        AppConfigManager::ins().saveConfig(); // 确保程序退出前保存配置
    }
    catch (std::exception &e)
    {
        return Config_ERROR;
    }
    return Config_SUCCESSED;
}
