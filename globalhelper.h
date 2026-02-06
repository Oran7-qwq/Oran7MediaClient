#ifndef GLOBALHELPER_H
#define GLOBALHELPER_H
#include <QString>
#include <QStringList>
#include <QSize>
#include <QVariantMap>

#define MAX_SLIDER_VALUE 65536

enum ERROR_CODE
{
    NoError = 0,
    ErrorFileInvalid
};

class GlobalHelper
{
public:
    GlobalHelper();
};

//==================<Self define logInfo Out>=================//
#include <iostream>
#include <sstream>
class INFO_Logger
{
public:
    INFO_Logger(const char *file,const int line,const std::string& prefix = "[INFO]")
        : enable(true)
        , specific(false)
    {
        if(specific==true)
            stream << prefix << "["<<file<<":"<<line<<"]------->";
        else
            stream << prefix<<"--->";
    }
    template<typename T>
    INFO_Logger& operator<<(const T& value)
    {
        if(enable)
            stream << value;
        return *this;
    }
    ~INFO_Logger()
    {
        if(enable)
            std::cout<<stream.str()<<std::endl;
    }

    INFO_Logger(const INFO_Logger&)=delete;
    INFO_Logger& operator=(const INFO_Logger&)=delete;

private:
    bool enable;
    bool specific;
    std::ostringstream stream;
};
#define INFO_LOG INFO_Logger(__FILE__,__LINE__)


class WARNNING_Logger
{
public:
    WARNNING_Logger(const char *file,const int line,const std::string& prefix = "[WARNNING]")
        : enable(true)
        , specific(false)
    {
        if(specific==true)
            stream << prefix << "["<<file<<":"<<line<<"]------->";
        else
            stream << prefix<<"--->";
    }
    template<typename T>
    WARNNING_Logger& operator<<(const T& value)
    {
        if(enable)
            stream << value;
        return *this;
    }
    ~WARNNING_Logger()
    {
        if(enable)
            std::cout<<stream.str()<<std::endl;
    }

    WARNNING_Logger(const WARNNING_Logger&)=delete;
    WARNNING_Logger& operator=(const WARNNING_Logger&)=delete;

private:
    bool enable;
    bool specific;
    std::ostringstream stream;
};
#define WARNNING_LOG WARNNING_Logger(__FILE__,__LINE__)

#include <iostream>
#include <sstream>
#include <string>
#include <locale>
#include <codecvt>

class NECTWORK_Logger
{
public:
    NECTWORK_Logger(const char *file, const int line, const std::string& prefix = "[NECTWORK]")
        : enable(true), specific(false)
    {
        if (specific == true)
            stream << prefix << "[" << file << ":" << line << "]---->";
        else
            stream << prefix << "--->";
    }

    template<typename T>
    NECTWORK_Logger& operator<<(const T& value)
    {
        if (enable)
            stream << value;
        return *this;
    }

    // 专门处理输出宽字符
    NECTWORK_Logger& operator<<(const std::wstring& wvalue)
    {
        if (enable)
        {
            // 转换宽字符为UTF-8，然后输出
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::string str = converter.to_bytes(wvalue);
            stream << str;
        }
        return *this;
    }

    ~NECTWORK_Logger()
    {
        if (enable)
            std::cout << stream.str() << std::endl;
    }

    NECTWORK_Logger(const NECTWORK_Logger&) = delete;
    NECTWORK_Logger& operator=(const NECTWORK_Logger&) = delete;

private:
    bool enable;
    bool specific;
    std::ostringstream stream;
};

#define NECTWORK_LOG NECTWORK_Logger(__FILE__, __LINE__)


#include <QString>
#ifdef _WIN32
#include <dxgiformat.h>
#endif

//=====================Global Parameters Or DataStruct===============//
struct Oran7AppData
{
public:
    Oran7AppData() {}
    int playerVolume = 25;
};

struct Oran7VideoInfo{
public:
    Oran7VideoInfo(){}
    QSize srcSize;
    int srcFormat;
    QString srcFormatName;

    QSize renderSize;
    int dxgiFormat = 0;
    QString dxgiFormatName;

    int fps = 0;
    bool isFromHardWare = false;
    QString decodeDevice;
    QString renderDevice;
    QString fillModeName;
};

//============= tools function ==============//

static QString dxgiFormatName(int fmt)
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
