#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSqlDatabase>
#include <QLoggingCategory>
#include <QQmlContext>

#include "appjsonconfigmanager.h"
#include "sever.h"
#include "client.h"
#include "applicationcontext.h"
#include "filehelper.h"
#include "bilibliliveroomaddresscatch.h"

#include <QtGlobal>
#include <QDebug>

#include <QLoggingCategory>


#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>

extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

void disableDPIVirtualization()
{
    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (*PSETPROCESSDPIAWARE)(void);
        PSETPROCESSDPIAWARE pSetProcessDPIAware =
            (PSETPROCESSDPIAWARE)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (pSetProcessDPIAware) {
            pSetProcessDPIAware();
        }
        FreeLibrary(hUser32);
    }
}
#endif

enum ConfigRET{
    Config_NONE = -2,
    Config_ERROR = -1,
    Config_SUCCESSED = 0
};

ConfigRET Config_AppConfigManager_RET = Config_NONE;
ConfigRET Config_AppConfigManager_Load(QQmlApplicationEngine &engine);
ConfigRET Config_AppConfigManager_Save();

#undef main
int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    QLoggingCategory::setFilterRules("qt.gui.imageio=false");
    disableDPIVirtualization();

    //threaded 渲染循环
    //qputenv("QSG_RHI_BACKEND", "opengl");
    qputenv("QSG_RENDER_LOOP", "basic");
    //qputenv("QSG_RHI_PROFILE", "false");
    //qputenv("QSG_INFO", "1");

    // 非强制 opengl，让 Qt 默认走 D3D11
    // qputenv("QSG_RHI_BACKEND", "opengl");//Discard

    // 禁用RHI验证（Qt 6.7+）
    //qputenv("QSG_RHI_VALIDATE_LAYER", "0");
#endif

    // 日志调试
    qputenv("QT_LOGGING_RULES",
            "qt.scenegraph.general=true\n"
            "qt.scenegraph.renderloop=true\n"
            "qt.rhi.general=true\n"
            "qt.rhi.warning=true\n"
            );

    // 配置渲染表面
    QSurfaceFormat format;
    // 针对性能优化
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);  // 启用VSync
    format.setDepthBufferSize(32);
    format.setStencilBufferSize(16);
    format.setSamples(4);  // 4x MSAA
    format.setAlphaBufferSize(16);
    // 使用兼容性配置文件
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(format);

    // 不使用 OpenGL 共享，删除 AA_ShareOpenGLContexts
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QGuiApplication app(argc, argv);

    // 设置场景图形后端
    //QQuickWindow::setSceneGraphBackend("software");  // 使用软件渲染器

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);

    // Qt 默认 backend（Windows 下通常是 D3D11）
    qDebug() << "Graphics API:" << QQuickWindow::graphicsApi();

    app.setWindowIcon(QIcon(":/image/Oran7.jpg"));
    app.setApplicationName("Oran7MediaClient");
    app.setOrganizationName("Oran7Apps");

    QQuickWindow::setTextRenderType(QQuickWindow::QtTextRendering);
    QQuickStyle::setStyle("Fusion");

    QQmlApplicationEngine engine;

    ApplicationContext::instance();

    qmlRegisterSingletonType(QUrl("qrc:/Src/Basic/BasicConfig.qml"),
                             "BasicConfig", 1, 0, "BasicConfig");
    qmlRegisterType<FileHelper>("FileHelper", 1, 0, "FileHelper");
    qmlRegisterType<BilibiliRoomAddressCatch>("BilibiliRoomAddressCatch",1,0,"BilibiliRoomAddressCatch");
    qmlRegisterType<D3D11VideoItem>("D3D11VideoItem", 1, 0, "D3D11VideoItem");

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    Sever sever;
    sever.startServer(55711);

    Client *client = ApplicationContext::instance()->client();
    qmlRegisterSingletonType<Client>("Client", 1, 0, "Client",
         [client](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
             Q_UNUSED(engine);
             Q_UNUSED(scriptEngine);
             return client;
         });

    QLoggingCategory::setFilterRules("qt.quick.shadereffect.warning=false\n");

    if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        qCritical() << "SQLite driver not available!";
        return -1;
    }

    engine.load(url);
    // auto *rootObj = engine.rootObjects().value(0);
    // if (rootObj) rootObj->dumpObjectTree();

    Config_AppConfigManager_RET = Config_AppConfigManager_Load(engine);
    if(Config_AppConfigManager_RET == ConfigRET::Config_SUCCESSED)
        qDebug()<<"[AppConfigManager:]Successfully load user Config,";

    QObject::connect(&app, &QGuiApplication::aboutToQuit, &app,[&engine]() mutable {
        ApplicationContext::instance()->client()->StopPlayerRuning();//先确保杀死Oran7MediaPlayer防止Video_refresh还在触发回调qml渲染对象

        ApplicationContext::instance()->client()->saveConfig_AppWindowSize(engine);
        ApplicationContext::instance()->client()->saveConfig_AppWindowPosition(engine);

        Config_AppConfigManager_RET = Config_AppConfigManager_Save();
        if(Config_AppConfigManager_RET == ConfigRET::Config_SUCCESSED)
            qDebug()<<"[AppConfigManager:]Successfully save user Config.";
    });

    return app.exec();
}


ConfigRET Config_AppConfigManager_Load(QQmlApplicationEngine &engine)
{
    try
    {
        AppConfigManager::instance().loadConfig();//<-----一定要首先加载

        ApplicationContext::instance()->client()->loadConfig_AppWindowPosition(engine);
        ApplicationContext::instance()->client()->loadConfig_AppWindowSize(engine);

        QString SearchLocalMediaFiles_folderPath=ApplicationContext::instance()->client()->createAppDirectories();
        if(SearchLocalMediaFiles_folderPath.isEmpty())
            throw std::runtime_error("AppData Temp folder in Oran7CloudMusic Could not be Created correctly");

        ApplicationContext::instance()->client()->loadConfig_localMusicList_playOrder();//*加载localMusicList自定义排序Config配置

        ApplicationContext::instance()->asyncWorker()->startSearchLocalMediaFiles_Task(SearchLocalMediaFiles_folderPath); //*Load localMusic列表（Async）

        ApplicationContext::instance()->client()->loadConfig_lastCloseAppFocusedMusic();//*加载last focus music info

        ApplicationContext::instance()->client()->loadConfig_AppSetPlayerVolume();//*Load PlayerVolume Config
    }
    catch (std::exception &e)
    {
        return Config_ERROR;
    }

    return Config_SUCCESSED;
}

ConfigRET Config_AppConfigManager_Save()
{
    try
    {
        ApplicationContext::instance()->client()->saveConfig_lastCloseAppFocusedMusic();//* 保存客户端最近一次播放的文件

        ApplicationContext::instance()->client()->saveConfig_localMusicList_playOrder();//* 保存Custom自定义LocalMusicList_playOrder的Config配置

        ApplicationContext::instance()->client()->saveConfig_AppSetPlayerVolume();

        AppConfigManager::instance().saveConfig();// 确保程序退出前保存配置
    }
    catch (std::exception &e)
    {
        return Config_ERROR;
    }
    return Config_SUCCESSED;
}
