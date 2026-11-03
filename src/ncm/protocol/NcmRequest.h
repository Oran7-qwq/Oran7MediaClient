#ifndef NCMREQUEST_H
#define NCMREQUEST_H

#include <QByteArray>
#include <QHash>
#include <QUrl>
#include <QString>
#include <atomic>

/**
 * @brief 一次已准备好的 HTTP 请求（协议层输出，网络层输入）
 *
 * 纯数据：NcmProtocol 负责构造，curl 传输层负责发送。
 * 网络层不需要知道 WEAPI/EAPI 的区别。
 */
struct NcmRequest
{
    static quint64 nextId() {
        static std::atomic<quint64> sequence{0};
        return ++sequence;
    }
    quint64 id = nextId();
    QString tag;
    QUrl url;
    QByteArray body;
    QHash<QByteArray, QByteArray> headers;
    bool isPost = true;
    bool disableHttp2 = true; // 网易云 HTTP/2 空响应规避
};

#endif // NCMREQUEST_H
