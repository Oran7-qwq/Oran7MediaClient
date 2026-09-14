#include "CurlHttpClient.h"

#include "GlobalHelper.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QStandardPaths>

CurlHttpClient::CurlHttpClient(QObject* parent)
    : QObject(parent)
    , m_curl(curlPath())
{
}

QString CurlHttpClient::curlPath()
{
    const QStringList candidates = {
        QStringLiteral("C:/Windows/System32/curl.exe"), // 64 位 Windows
        QStringLiteral("C:/Windows/SysWOW64/curl.exe")  // 32 位
    };
    for (const QString& c : candidates) {
        if (QFile::exists(c))
            return c;
    }
    return QStandardPaths::findExecutable(QStringLiteral("curl"));
}

void CurlHttpClient::send(const NcmRequest& req, const QString& tag)
{
    if (m_curl.isEmpty()) {
        emit requestError(QStringLiteral("未找到系统 curl.exe，无法发送网易云请求"), tag);
        return;
    }

    // 请求体写入临时文件，避免命令行长度/转义问题（GET 无 body，跳过）
    QString bodyPath;
    if (req.isPost) {
        bodyPath = QDir::temp().filePath(
            QStringLiteral("ncm_curl_body_%1").arg(QRandomGenerator::global()->generate()));
        QFile f(bodyPath);
        if (!f.open(QIODevice::WriteOnly)) {
            emit requestError(QStringLiteral("无法写入临时请求体"), tag);
            return;
        }
        f.write(req.body);
        f.close();
    }
    const QString outPath = QDir::temp().filePath(
        QStringLiteral("ncm_curl_out_%1").arg(QRandomGenerator::global()->generate()));
    const QString hdrPath = QDir::temp().filePath(
        QStringLiteral("ncm_curl_hdr_%1").arg(QRandomGenerator::global()->generate()));

    QStringList args;
    args << QStringLiteral("--http1.1") << QStringLiteral("--ssl-no-revoke")
         << QStringLiteral("--globoff") << QStringLiteral("-sS") << QStringLiteral("--max-time") << QStringLiteral("10")
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

    NETWORK_LOG << "CurlHttpClient:" << (req.isPost ? "POST" : "GET") << req.url.path() << "tag=" << tag;

    QProcess* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    connect(proc, &QProcess::finished, this, [this, proc, tag, bodyPath, outPath, hdrPath](
            int exitCode, QProcess::ExitStatus status) {
        const int httpStatus = QString::fromUtf8(proc->readAllStandardOutput()).trimmed().toInt();
        const QByteArray stderrText = proc->readAllStandardError().trimmed();

        QByteArray body;
        {
            QFile f(outPath);
            if (f.open(QIODevice::ReadOnly))
                body = f.readAll();
        }
        // 从响应头里提取 Set-Cookie（803 成功兜底）
        QString setCookie;
        {
            QFile f(hdrPath);
            if (f.open(QIODevice::ReadOnly)) {
                const QList<QByteArray> lines = f.readAll().split('\n');
                for (const QByteArray& line : lines) {
                    if (line.startsWith("Set-Cookie:")) {
                        if (!setCookie.isEmpty())
                            setCookie += QStringLiteral("; ");
                        setCookie += QString::fromUtf8(line.mid(11)).trimmed();
                    }
                }
            }
        }
        QFile::remove(outPath);
        if (!bodyPath.isEmpty())   // GET 无 body，bodyPath 为空
            QFile::remove(bodyPath);
        QFile::remove(hdrPath);
        proc->deleteLater();

        if (exitCode != 0 || status != QProcess::NormalExit) {
            NETWORK_LOG << "CurlHttpClient: curl failed exit=" << exitCode
                        << " stderr=" << QString::fromUtf8(stderrText);
            emit requestError(QStringLiteral("curl 请求失败: %1").arg(QString::fromUtf8(stderrText)),
                              tag);
            return;
        }
        NETWORK_LOG << "CurlHttpClient: reply http=" << httpStatus
                    << " len=" << body.size();
        emit requestFinished(httpStatus, body, setCookie, tag);
    });

    proc->start(m_curl, args);
}
