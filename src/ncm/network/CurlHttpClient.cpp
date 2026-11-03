#include "CurlHttpClient.h"
#include "Oran7GlobalHelper.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <memory>
#include <utility>

// QObject 空壳在 GUI 建立后移动，所有进程/文件资源只在后台线程创建和释放。
class Oran7CurlHttpWorker final : public QObject {
    Q_OBJECT
public:
    Oran7CurlHttpWorker(QString executable, QString root)
        : m_curl(std::move(executable)), m_temporaryRoot(std::move(root)) {}
    ~Oran7CurlHttpWorker() override {
        for (auto* process : findChildren<QProcess*>(QString(), Qt::FindDirectChildrenOnly)) {
            disconnect(process, nullptr, this, nullptr);
            if (process->state() != QProcess::NotRunning) {
                process->kill();
                process->waitForFinished(1000);
            }
            delete process;
        }
    }
    void send(const NcmRequest& req);
signals:
    void requestFinished(quint64 requestId, const QString& tag, int httpStatus,
                         const QByteArray& body, const QString& setCookie);
    void requestError(quint64 requestId, const QString& tag, const QString& message);
private:
    QString m_curl;
    QString m_temporaryRoot;
};

CurlHttpClient::CurlHttpClient(QObject* parent, const QString& executable, const QString& temporaryRoot)
    : QObject(parent)
{
    m_worker = new Oran7CurlHttpWorker(executable.isEmpty() ? curlPath() : executable,
        temporaryRoot.isEmpty() ? QDir::tempPath() : temporaryRoot);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &Oran7CurlHttpWorker::requestFinished, this, &CurlHttpClient::requestFinished, Qt::QueuedConnection);
    connect(m_worker, &Oran7CurlHttpWorker::requestError, this, &CurlHttpClient::requestError, Qt::QueuedConnection);
    m_thread.start();
}
CurlHttpClient::~CurlHttpClient()
{
    // 仅服务退出时等待：不向 GUI 转发迟到结果，后台销毁进程后才结束线程。
    disconnect(m_worker, nullptr, this, nullptr);
    m_thread.requestInterruption();
    m_thread.quit();
    m_thread.wait();
}
QString CurlHttpClient::curlPath()
{
    for (const auto& path : {QStringLiteral("C:/Windows/System32/curl.exe"), QStringLiteral("C:/Windows/SysWOW64/curl.exe")})
        if (QFile::exists(path)) return path;
    return QStandardPaths::findExecutable(QStringLiteral("curl"));
}
void CurlHttpClient::send(const NcmRequest& req)
{
    // 请求仅携带值快照；操作系统 CreateProcess 即使耗时，也不会堵住调用线程。
    auto* worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, req] {
        if (!QThread::currentThread()->isInterruptionRequested()) worker->send(req);
    }, Qt::QueuedConnection);
}
void Oran7CurlHttpWorker::send(const NcmRequest& req)
{
    if (m_curl.isEmpty()) {
        emit requestError(req.id, req.tag, QStringLiteral("未找到系统 curl.exe"));
        return;
    }
    auto directory = std::make_shared<QTemporaryDir>(QDir(m_temporaryRoot).filePath("oran7-ncm-XXXXXX"));
    if (!directory->isValid()) {
        emit requestError(req.id, req.tag, QStringLiteral("无法创建请求临时目录"));
        return;
    }
    const auto bodyPath = directory->filePath("body");
    const auto outPath = directory->filePath("response");
    const auto hdrPath = directory->filePath("headers");
    if (req.isPost) {
        QFile body(bodyPath);
        if (!body.open(QIODevice::WriteOnly) || body.write(req.body) != req.body.size()) {
            emit requestError(req.id, req.tag, QStringLiteral("无法写入临时请求体"));
            return;
        }
    }
    QStringList args{"--http1.1", "--ssl-no-revoke", "--globoff", "-sS", "--max-time", "10",
        "--output", outPath, "--dump-header", hdrPath, "--write-out", "%{http_code}"};
    if (req.isPost) args << "-X" << "POST" << "--data-binary" << "@" + bodyPath;
    for (auto it = req.headers.cbegin(); it != req.headers.cend(); ++it)
        args << "-H" << QString::fromLatin1(it.key()) + ": " + QString::fromLatin1(it.value());
    args << req.url.toString(QUrl::FullyEncoded);
    NETWORK_LOG << "Ncm HTTP request=" << req.id << "tag=" << req.tag << "path=" << req.url.path();
    auto* process = new QProcess(this); // QObject 独占，退出时先终止进程再释放临时目录。
    auto completed = std::make_shared<bool>(false);
    connect(process, &QObject::destroyed, [directory] {});
    connect(process, &QProcess::errorOccurred, this,
        [this, process, completed, id = req.id, tag = req.tag](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart || *completed) return;
            *completed = true;
            process->deleteLater();
            emit requestError(id, tag, QStringLiteral("curl 进程启动失败"));
        });
    connect(process, &QProcess::finished, this,
        [this, process, directory, completed, id = req.id, tag = req.tag, outPath, hdrPath]
        (int exitCode, QProcess::ExitStatus status) {
            if (*completed) return;
            *completed = true;
            const int httpStatus = QString::fromUtf8(process->readAllStandardOutput()).trimmed().toInt();
            // stderr 可能包含请求 URL/凭据，不进入日志或向上层传播。
            process->readAllStandardError();
            process->deleteLater();
            if (exitCode != 0 || status != QProcess::NormalExit) {
                NETWORK_LOG << "Ncm HTTP request=" << id << "tag=" << tag << "exit=" << exitCode;
                emit requestError(id, tag, QStringLiteral("curl 请求失败（退出码 %1）").arg(exitCode));
                return;
            }
            QFile response(outPath); QByteArray body;
            if (response.open(QIODevice::ReadOnly)) body = response.readAll();
            QFile headers(hdrPath); QStringList cookies;
            if (headers.open(QIODevice::ReadOnly)) {
                for (const auto& line : headers.readAll().split('\n'))
                    if (line.toLower().startsWith("set-cookie:")) cookies.append(QString::fromUtf8(line.mid(11)).trimmed());
            }
            NETWORK_LOG << "Ncm HTTP request=" << id << "tag=" << tag << "status=" << httpStatus << "bytes=" << body.size();
            emit requestFinished(id, tag, httpStatus, body, cookies.join("; "));
        });
    process->start(m_curl, args);
}

#include "CurlHttpClient.moc"
