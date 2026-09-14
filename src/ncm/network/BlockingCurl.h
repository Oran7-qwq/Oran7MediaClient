#ifndef ORAN7_BLOCKINGCURL_H
#define ORAN7_BLOCKINGCURL_H

#include "NcmRequest.h"

#include <QByteArray>
#include <QString>

namespace ncm {

/**
 * @brief 同步阻塞执行一次 curl.exe 请求（在调用线程 waitForFinished）
 *
 * 与 CurlHttpClient::send() 复用同一套 curl 参数（--http1.1 等），但为同步、
 * 无 QObject/信号依赖、线程安全（无任何共享状态）。供 Oran7AsyncManager 的
 * 线程池 handler 直接调用：req 的 url/body/headers/isPost 原样使用，
 * Cookie 由调用方放进 headers。
 *
 * 返回 false 仅表示 curl 进程级/网络错误（找不到 curl、非零退出），*outError 填原因；
 * HTTP 4xx/5xx 仍返回 true，由调用方根据 *outHttpStatus 判断。
 */
bool blockingCurl(const NcmRequest& req, QByteArray* outBody, int* outHttpStatus,
                  QString* outError, int timeoutMs = 10000);

} // namespace ncm

#endif // ORAN7_BLOCKINGCURL_H
