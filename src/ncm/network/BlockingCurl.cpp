#include "BlockingCurl.h"

#include "CurlHttpClient.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>

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

    // 请求体写入临时文件，避免命令行长度/转义问题（GET 无 body，跳过）
    QString bodyPath;
    if (req.isPost) {
        bodyPath = QDir::temp().filePath(
            QStringLiteral("ncm_curl_body_%1").arg(QRandomGenerator::global()->generate()));
        QFile f(bodyPath);
        if (!f.open(QIODevice::WriteOnly)) {
            if (outError)
                *outError = QStringLiteral("无法写入临时请求体");
            return false;
        }
        f.write(req.body);
        f.close();
    }
    const QString outPath = QDir::temp().filePath(
        QStringLiteral("ncm_curl_out_%1").arg(QRandomGenerator::global()->generate()));
    const QString hdrPath = QDir::temp().filePath(
        QStringLiteral("ncm_curl_hdr_%1").arg(QRandomGenerator::global()->generate()));

    // 与 CurlHttpClient::send 完全一致的参数：--http1.1 防 WAF 空响应、--dump-header
    // 保证 stdout 只输出 %{http_code}、--max-time 限制超时
    QStringList args;
    args << QStringLiteral("--http1.1") << QStringLiteral("--ssl-no-revoke")
         << QStringLiteral("--globoff") << QStringLiteral("-sS")
         << QStringLiteral("--max-time") << QString::number(timeoutMs)
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
    proc.waitForFinished(timeoutMs + 5000);   // 多给 5s 供进程启动/退出

    const int httpStatus = QString::fromUtf8(proc.readAllStandardOutput()).trimmed().toInt();
    const QByteArray stderrText = proc.readAllStandardError().trimmed();

    QByteArray body;
    {
        QFile f(outPath);
        if (f.open(QIODevice::ReadOnly))
            body = f.readAll();
    }

    QFile::remove(outPath);
    if (!bodyPath.isEmpty())   // GET 无 body，bodyPath 为空
        QFile::remove(bodyPath);
    QFile::remove(hdrPath);

    if (proc.exitCode() != 0 || proc.exitStatus() != QProcess::NormalExit) {
        if (outError)
            *outError = QStringLiteral("curl 请求失败: %1").arg(QString::fromUtf8(stderrText));
        return false;
    }

    if (outHttpStatus)
        *outHttpStatus = httpStatus;
    if (outBody)
        *outBody = body;
    return true;
}

} // namespace ncm
