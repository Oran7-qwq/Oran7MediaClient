#ifndef NCMREQUEST_H
#define NCMREQUEST_H

#include <QByteArray>
#include <QHash>
#include <QUrl>

/**
 * @brief 一次已准备好的 HTTP 请求（协议层输出，网络层输入）
 *
 * 纯数据：NcmProtocol 负责构造，HttpClient 负责发送。
 * 网络层不需要知道 WEAPI/EAPI 的区别。
 */
struct NcmRequest
{
    QUrl url;
    QByteArray body;
    QHash<QByteArray, QByteArray> headers;
    bool isPost = true;
    bool disableHttp2 = true; // 网易云 HTTP/2 空响应规避
};

#endif // NCMREQUEST_H
