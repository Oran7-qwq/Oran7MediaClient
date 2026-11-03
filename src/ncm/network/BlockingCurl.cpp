#include "BlockingCurl.h"

#include "CurlHttpClient.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

namespace ncm {

bool blockingCurl(const NcmRequest& req, QByteArray* outBody, int* outHttpStatus,
                  QString* outError, int timeoutMs)
{
    if (outBody)
        outBody->clear();
    if (outHttpStatus)
        *outHttpStatus = 0;
    if (outError)
        outError->clear();

    const QString curl = CurlHttpClient::curlPath();
    if (curl.isEmpty()) {
        if (outError)
            *outError = QStringLiteral("未找到系统 curl.exe，无法发送网易云请求");
        return false;
    }

    QTemporaryDir directory(QDir::tempPath() + "/oran7-ncm-blocking-XXXXXX");
    if (!directory.isValid()) {
        if (outError) *outError = QStringLiteral("无法创建请求临时目录");
        return false;
    }
    const auto bodyPath = directory.filePath("body");
    const auto outPath = directory.filePath("response");
    const auto hdrPath = directory.filePath("headers");
    if (req.isPost) {
        QFile file(bodyPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(req.body) != req.body.size()) {
            if (outError) *outError = QStringLiteral("无法写入临时请求体");
            return false;
        }
    }

    // 与 CurlHttpClient::send 完全一致的参数：--http1.1 防 WAF 空响应、--dump-header
    // 保证 stdout 只输出 %{http_code}、--max-time 限制超时
    QStringList args;
    args << QStringLiteral("--http1.1") << QStringLiteral("--ssl-no-revoke")
         << QStringLiteral("--globoff") << QStringLiteral("-sS")
         << QStringLiteral("--max-time") << QString::number(qMax(1, timeoutMs) / 1000.0, 'f', 3)
         << QStringLiteral("--output") << outPath
         << QStringLiteral("--dump-header") << hdrPath
         << QStringLiteral("--write-out") << QStringLiteral("%{http_code}");
    if (req.isPost) {
        args << QStringLiteral("-X") << QStringLiteral("POST")
             << QStringLiteral("--data-binary") << (QStringLiteral("@") + bodyPath);
    }
    for (auto it = req.headers.constBegin(); it != req.headers.constEnd(); ++it) {
        args << QStringLiteral("-H")
             << QStringLiteral("%1: %2").arg(QString::fromLatin1(it.key()),
                                             QString::fromLatin1(it.value()));
    }
    // 必须用 FullyEncoded：QUrl::toString() 默认 PrettyDecoded 会把查询里的中文解码成原始字符，
    // curl.exe 会报 "URL rejected: Malformed input"（如搜索关键词）
    args << req.url.toString(QUrl::FullyEncoded);

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(curl, args);
    if (!proc.waitForFinished(qMax(1, timeoutMs) + 5000)) {
        proc.kill();
        proc.waitForFinished(1000);
        if (outError) *outError = QStringLiteral("curl 启动失败或请求超时");
        return false;
    }

    const int httpStatus = QString::fromUtf8(proc.readAllStandardOutput()).trimmed().toInt();
    proc.readAllStandardError(); // 不传播可能包含敏感 URL/凭据的 stderr。

    QByteArray body;
    {
        QFile f(outPath);
        if (f.open(QIODevice::ReadOnly))
            body = f.readAll();
    }

    if (proc.exitCode() != 0 || proc.exitStatus() != QProcess::NormalExit) {
        if (outError)
            *outError = QStringLiteral("curl 请求失败（退出码 %1）").arg(proc.exitCode());
        return false;
    }

    if (outHttpStatus)
        *outHttpStatus = httpStatus;
    if (outBody)
        *outBody = body;
    return true;
}

} // namespace ncm
