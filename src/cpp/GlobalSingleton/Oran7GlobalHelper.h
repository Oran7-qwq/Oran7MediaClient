#ifndef ORAN7GLOBALHELPER_H
#define ORAN7GLOBALHELPER_H
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

using ErrorCallback = std::function<void(const std::string& msg)>;

//==================<Self define logInfo Out>=================//
#pragma once
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>

#include "Oran7Async.h"

class Oran7Logger
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

    Oran7Logger(Level level,
           const char* file,
           int line,
           bool showDetail = false,
           bool showData = true,
           bool showThreadId = false)
        : m_level(level)
        , m_debug(&m_buf)   // 先格式化进 QString，析构时交给统一 Qt 日志入口
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
        header += QString("[%1]").arg(QFileInfo(QString::fromUtf8(file)).completeBaseName());

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
    Oran7Logger& operator<<(const T& value)
    {
        m_debug << value;
        return *this;
    }

    ~Oran7Logger()
    {
        // 统一 Qt 消息处理器只负责脱敏入队；不再占用业务线程池。
        if (!m_buf.trimmed().isEmpty())
        {
            QDebug dbg(levelToQtType(m_level));
            dbg.noquote().nospace() << m_buf;
        }
    }

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
    Level m_level = Level::Info;
    QString m_buf;
    QDebug m_debug;
};


#define CLIENT_LOG   Oran7Logger(Oran7Logger::Level::Client,  __FILE__, __LINE__)
#define INFO_LOG     Oran7Logger(Oran7Logger::Level::Info,    __FILE__, __LINE__)
#define WARNING_LOG  Oran7Logger(Oran7Logger::Level::Warning, __FILE__, __LINE__)
#define ERROR_LOG Oran7Logger(Oran7Logger::Level::Error, __FILE__,__LINE__)
#define NETWORK_LOG  Oran7Logger(Oran7Logger::Level::Network, __FILE__, __LINE__)
#define CONFIG_LOG   Oran7Logger(Oran7Logger::Level::Config,  __FILE__, __LINE__)

class Oran7GlobalHelper
{
public:
    Oran7GlobalHelper();

    static QString getConfigDir();
};


#include <QString>
#ifdef _WIN32
#include <dxgiformat.h>
#endif
//=====================Global Parameters Or DataStruct===============//
struct Oran7VideoInfo{
public:
    Oran7VideoInfo(){}
    QSize srcSize;
    int srcFormat = -1;
    QString srcFormatName;

    QSize renderSize;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
    QString dxgiFormatName;

    int fps = 0;
    bool isFromHardWare = false;
    QString decodeDevice;
    QString renderDevice;
    QString fillModeName;
    QString sourceTag;      // "player" / "screen"，区分来源
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
            {"sourceTag",    i.sourceTag},
    };
}

#endif // ORAN7GLOBALHELPER_H
