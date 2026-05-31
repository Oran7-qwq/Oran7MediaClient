#ifndef GLOBALHELPER_H
#define GLOBALHELPER_H
#include <QString>
#include <QStringList>
#include <QSize>
#include <QVariantMap>
#include <QDir>
#include <QThread>

#include <QCoreApplication>

#define DEVELOPER_MODE 0
#define MAX_SLIDER_VALUE 65536

enum ERROR_CODE
{
    NoError = 0,
    ErrorFileInvalid
};

//==================<Self define logInfo Out>=================//
#pragma once
#include <QDebug>
#include <QDateTime>

class Logger
{
public:
    enum class Level
    {
        Client,
        Info,
        Warning,
        Error,
        Network,
        Config
    };

    Logger(Level level,
           const char* file,
           int line,
           bool showDetail = false,
           bool showData = true,
           bool showThreadId = false)
        : m_debug(levelToQtType(level))
    {
        m_debug.noquote().nospace();

        QString header;
        if(showData)
        {
            header += QDateTime::currentDateTime()
            .toString("hh:mm:ss.zzz");
        }
        if (showDetail)
        {
            header += QString("[%1:%2]").arg(file).arg(line);
        }
        header += ("[" + levelToString(level) + "]")
                             .leftJustified(2, ' ');

        if(showThreadId)
        {
            header += QString("[T:%1]")
            .arg((quintptr)QThread::currentThreadId())
                .leftJustified(2, ' ');
        }

        header += " >> ";
        m_debug << header;
    }

    template<typename T>
    Logger& operator<<(const T& value)
    {
        m_debug << value;
        return *this;
    }

    ~Logger() = default;

private:
    static QtMsgType levelToQtType(Level level)
    {
        switch (level)
        {
        case Level::Client:
            return QtInfoMsg;
        case Level::Info:
            return QtInfoMsg;
        case Level::Warning:
            return QtWarningMsg;
        case Level::Error:
            return QtCriticalMsg;
        case Level::Network:
            return QtInfoMsg;
        case Level::Config:
            return QtInfoMsg;
        default:
            return QtDebugMsg;
        }
    }

    static QString levelToString(Level level)
    {
        switch (level)
        {
        case Level::Client:  return "CLET";
        case Level::Info:    return "INFO";
        case Level::Warning: return "WARN";
        case Level:: Error: return "EROR";
        case Level::Network: return "NETW";
        case Level::Config:  return "CFIG";
        }
        return "UNKNOWN";
    }

private:
    QDebug m_debug;
};


#define CLIENT_LOG   Logger(Logger::Level::Client,  __FILE__, __LINE__)
#define INFO_LOG     Logger(Logger::Level::Info,    __FILE__, __LINE__)
#define WARNING_LOG  Logger(Logger::Level::Warning, __FILE__, __LINE__)
#define ERROR_LOG Logger(Logger::Level::Error, __FILE__,__LINE__)
#define NETWORK_LOG  Logger(Logger::Level::Network, __FILE__, __LINE__)
#define CONFIG_LOG   Logger(Logger::Level::Config,  __FILE__, __LINE__)

class GlobalHelper
{
public:
    GlobalHelper();

    static QString getConfigDir();
};


#include <QString>
#ifdef _WIN32
#include <dxgiformat.h>
#endif
//=====================Global Parameters Or DataStruct===============//
struct Oran7AppData
{
public:
    Oran7AppData() {}
    //=========  Global =========//
    QString lastDirectory=QDir().homePath(); //App程序最近一次访问的目录
    QString audioAbsoluteFilePath=QString();//上次播放的audio文件绝对路径
    QString videoAbsoluteFilePath = QString();//上次播放的video文件绝对路径
    QString CurMediaFilePath = QString();//当前正在播放的media文件绝对路径

    //*cache 媒体文件缓存路径*//
    QString appDirPath;              //应用主目录//暂未启用

    QString audioDirPath;           //audio 子目录
    QString audioCoverDirPath; //audioCover存储封面子目录

    //============== Oran7MediaPlayer Parameters ===============
    int playerVolume = 25;

    //============== LocalMusic module ==============//
    QList<QFileInfo> localMusic_fileList=QList<QFileInfo>();     //存储文件列表（顺序只在首次启动时与Config.json同步）
    QSet<QString> localMusic_fileSet=QSet<QString>();          //字典-用于提升查重效率
    QVariantList Custom_LocalMusic_playOrder=QVariantList();//临时存储用户指定顺序(路径)
};

struct Oran7VideoInfo{
public:
    Oran7VideoInfo(){}
    QSize srcSize;
    int srcFormat;
    QString srcFormatName;

    QSize renderSize;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
    QString dxgiFormatName;

    int fps = 0;
    bool isFromHardWare = false;
    QString decodeDevice;
    QString renderDevice;
    QString fillModeName;
};

//============= tools function ==============//

static QString dxgiFormatName(DXGI_FORMAT fmt)
{
#ifdef _WIN32
    switch (static_cast<DXGI_FORMAT>(fmt)) {
    case DXGI_FORMAT_UNKNOWN: return "DXGI_FORMAT_UNKNOWN";
    case DXGI_FORMAT_NV12:    return "DXGI_FORMAT_NV12";
    case DXGI_FORMAT_P010:    return "DXGI_FORMAT_P010";
    case DXGI_FORMAT_P016:    return "DXGI_FORMAT_P016";
    case DXGI_FORMAT_YUY2:    return "DXGI_FORMAT_YUY2";
    case DXGI_FORMAT_AYUV:    return "DXGI_FORMAT_AYUV";
    case DXGI_FORMAT_Y410:    return "DXGI_FORMAT_Y410";
    case DXGI_FORMAT_Y416:    return "DXGI_FORMAT_Y416";
    case DXGI_FORMAT_R8G8B8A8_UNORM: return "DXGI_FORMAT_R8G8B8A8_UNORM";
    case DXGI_FORMAT_B8G8R8A8_UNORM: return "DXGI_FORMAT_B8G8R8A8_UNORM";
    //...
    default:
        return QString("DXGI_FORMAT(%1)").arg(fmt);
    }
#else
    return QString("DXGI_FORMAT(%1)").arg(fmt);
#endif
}

static QVariantMap Oran7VideoInfo_To_QVariantMap(const Oran7VideoInfo &i)
{
    return {
            {"srcWidth", i.srcSize.width()},
            {"srcHeight", i.srcSize.height()},
            {"srcFormat", i.srcFormat},
            {"srcFormatName", i.srcFormatName},

            {"renderWidth", i.renderSize.width()},
            {"renderHeight", i.renderSize.height()},
            {"dxgiFormat", i.dxgiFormat},
            {"dxgiFormatName", i.dxgiFormatName},

            {"fps", i.fps},
            {"isFromHardWare", i.isFromHardWare},
            {"decodeDevice", i.decodeDevice},
            {"renderDevice", i.renderDevice},
            {"fillModeName", i.fillModeName},
    };
}

#endif // GLOBALHELPER_H
