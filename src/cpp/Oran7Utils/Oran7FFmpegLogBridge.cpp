#include "Oran7FFmpegLogBridge.h"

#include <QDebug>
#include <cstdarg>
extern "C"
{
#include <libavutil/log.h>
}

namespace
{
void ffmpegMessage(void* context, int level, const char* format, va_list args)
{
    if (level > av_log_get_level())
        return;
    // 每次回调独立格式化，不共享 FFmpeg 默认回调的行缓冲或 stderr。
    char buffer[8192]{};
    int prefix = 1;
    av_log_format_line2(context, level, format, args, buffer, sizeof(buffer), &prefix);
    QtMsgType type = QtDebugMsg;
    if (level <= AV_LOG_ERROR)
        type = QtCriticalMsg; // FFmpeg 的 fatal 不等于 Qt 的进程终止。
    else if (level <= AV_LOG_WARNING)
        type = QtWarningMsg;
    else if (level <= AV_LOG_INFO)
        type = QtInfoMsg;
    QDebug message(type);
    message.noquote().nospace() << "[FFmpeg] " << QString::fromUtf8(buffer).trimmed();
}
} // namespace

void Oran7FFmpegLogBridge::install()
{
    av_log_set_level(AV_LOG_WARNING);
    av_log_set_callback(ffmpegMessage);
}
