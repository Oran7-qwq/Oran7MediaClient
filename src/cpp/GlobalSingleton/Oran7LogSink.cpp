#include "Oran7LogSink.h"

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
std::mutex handlerMutex;
Oran7LogSink* handlerSink = nullptr;
QtMessageHandler previousHandler = nullptr;
thread_local bool inSink = false;

const char* levelName(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "INFO";
}

void consoleOutput(QtMsgType type, const QByteArray& bytes)
{
#ifdef Q_OS_WIN
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO oldInfo{};
    const bool colored = console && console != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(console, &oldInfo);
    if (colored)
    {
        WORD color = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        if (type == QtWarningMsg)
            color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        if (type == QtCriticalMsg || type == QtFatalMsg)
            color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        SetConsoleTextAttribute(console, color);
    }
#else
    Q_UNUSED(type);
#endif
    const bool written = std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stdout) == size_t(bytes.size());
    const bool flushed = std::fflush(stdout) == 0;
#ifdef Q_OS_WIN
    if (colored)
        SetConsoleTextAttribute(console, oldInfo.wAttributes);
#endif
    if (!written || !flushed)
        throw std::runtime_error("console write failed");
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    // 写入失败不再发 Qt 消息；Qt 内部若在写线程报告错误也直接终止递归。
    if (inSink)
        return;
    inSink = true;
    try
    {
        std::lock_guard<std::mutex> lock(handlerMutex);
        if (handlerSink)
        {
            handlerSink->submit(type, message, QString::fromUtf8(context.category ? context.category : "default"));
            if (type == QtFatalMsg)
                handlerSink->flush(std::chrono::milliseconds(200));
        }
    }
    catch (...)
    {
        // 禁止将原始消息交回默认处理器，否则异常路径可能泄漏凭据。
    }
    inSink = false;
}
} // namespace

QString Oran7LogSink::redact(QString message)
{
    // 输入也设上限，避免异常响应正文令格式化开销无限增长；截断后仍先脱敏。
    message = message.left(65536);
    message.replace(QStringLiteral("\\/"), QStringLiteral("/"));
    message.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    static const QRegularExpression urlPattern(QStringLiteral(R"((?:https?|wss?|rtmps?|rtsp|ftp)://[^\s"'<>\)]+)"),
                                               QRegularExpression::CaseInsensitiveOption);
    // URL 路径本身也可能是一次性令牌，所以只保留协议、主机、端口。
    auto matches = urlPattern.globalMatch(message);
    QList<QPair<qsizetype, QPair<qsizetype, QString>>> replacements;
    while (matches.hasNext())
    {
        const auto match = matches.next();
        const QUrl url(match.captured());
        const QString host = url.host();
        QString replacement = host.isEmpty()
                                  ? QStringLiteral("[URL已脱敏]")
                                  : url.scheme() + QStringLiteral("://") + host +
                                        (url.port() >= 0 ? QStringLiteral(":%1").arg(url.port()) : QString()) +
                                        QStringLiteral("/[URL已脱敏]");
        replacements.append({match.capturedStart(), {match.capturedLength(), replacement}});
    }
    for (auto it = replacements.crbegin(); it != replacements.crend(); ++it)
        message.replace(it->first, it->second.first, it->second.second);

    // 对敏感字段保守地隐藏该行剩余内容，覆盖 JSON、Header、QDebug 容器和普通 key=value。
    static const QRegularExpression fields(QStringLiteral(
        R"((?im)(?<![\w])(?:["']?)(?:cookie(?:string)?|set-cookie|authorization|proxy-authorization|headers?|password|passwd|pwd|username|user_name|account|credentials?|secret|client_secret|(?:api|auth|access)[_-]?key|key|qrcode_key|unikey|params|body|payload|raw[_-]?response|response[_-]?body|session(?:id|[_-]?token)?|access[_-]?token|refresh[_-]?token|bearer[_-]?token|token|MUSIC_[UA]|__csrf|csrf(?:[_-]?token)?|SESSDATA|bili_jct|DedeUserID|buvid[34]|encSecKey|sign(?:ature)?|wsSecret|w_rid|ice-pwd|ice-ufrag|用户名|密码|口令|凭据)(?:["']?)\s*[:：=,]\s*[^\r\n]*)"));
    message.replace(fields, QStringLiteral("[敏感字段已脱敏]"));
    static const QRegularExpression authorization(QStringLiteral(R"((?i)\b(?:Bearer|Basic)\s+[^\s"',;]+)"));
    message.replace(authorization, QStringLiteral("[认证信息已脱敏]"));
    static const QRegularExpression sdp(QStringLiteral(R"((?is)(?:\bv=0\r?\n|a=ice-(?:pwd|ufrag):).*)"));
    message.replace(sdp, QStringLiteral("[SDP已脱敏]"));
    static const QRegularExpression jwt(QStringLiteral(R"(\beyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+)"));
    message.replace(jwt, QStringLiteral("[JWT已脱敏]"));
    // 单条物理行，防止正文伪造后续日志；保留换行位置便于定位。
    message.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    message.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    message.replace(QChar(0x1b), QLatin1Char('?'));
    return message;
}

struct Oran7LogSink::Impl
{
    struct Record
    {
        QtMsgType type;
        QString text;
        quint64 sequence;
    };
    Options options;
    mutable std::mutex mutex;
    std::mutex shutdownMutex;
    std::condition_variable ready;
    std::condition_variable drained;
    std::deque<Record> queue;
    Statistics stats;
    bool stopping = false;
    quint64 pendingDrops = 0;
    std::thread worker;
    std::unique_ptr<QFile> file;
    std::unique_ptr<QLockFile> directoryLock;
    QString date;
    int segment = 0;
    bool fileDisabled = false;

    explicit Impl(Options value) : options(std::move(value))
    {
        options.queueCapacity = std::clamp(options.queueCapacity, 1, 65536);
        options.maxMessageChars = std::clamp(options.maxMessageChars, 128, 65536);
        options.maxFileBytes = std::max<qint64>(options.maxFileBytes, 256);
        options.maxFiles = std::clamp(options.maxFiles, 1, 1000);
        if (!options.clock)
            options.clock = [] { return QDateTime::currentDateTime(); };
        if (!options.consoleWriter)
            options.consoleWriter = consoleOutput;
        worker = std::thread([this] { run(); });
    }

    void fileError()
    {
        file->close();
        directoryLock.reset();
        fileDisabled = true; // 本次会话不反复重试、递归记错或拖慢播放；控制台继续输出。
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++stats.fileErrors;
        }
        if (options.consoleEnabled)
        {
            try
            {
                options.consoleWriter(QtWarningMsg,
                                      QStringLiteral("[日志] 文件写入失败，本次会话降级为控制台输出\n").toUtf8());
            }
            catch (...)
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++stats.consoleErrors;
            }
        }
    }

    void prune()
    {
        QDir dir(options.directory);
        auto files = dir.entryList({QStringLiteral("oran7-*.log")}, QDir::Files, QDir::Name);
        static const QRegularExpression ownedName(QStringLiteral(R"(^oran7-\d{4}-\d{2}-\d{2}-\d{6}\.log$)"));
        files.removeIf([](const QString& name) { return !ownedName.match(name).hasMatch(); });
        int excess = files.size() - options.maxFiles;
        for (const auto& name : files)
        {
            if (excess <= 0)
                break;
            if (dir.filePath(name) == file->fileName())
                continue;
            if (!dir.remove(name))
            {
                fileError();
                return;
            }
            --excess;
        }
    }

    void writeFile(const QByteArray& bytes, const QDateTime& now)
    {
        if (fileDisabled || options.directory.isEmpty())
            return;
        const QString nextDate = now.date().toString(QStringLiteral("yyyy-MM-dd"));
        if (date != nextDate)
        {
            file->close();
            date = nextDate;
            segment = 0;
            const auto existing =
                QDir(options.directory)
                    .entryList({QStringLiteral("oran7-%1-??????.log").arg(date)}, QDir::Files, QDir::Name);
            if (!existing.isEmpty())
                segment = std::max(0, existing.last().mid(17, 6).toInt() - 1);
        }
        if (file->isOpen() && file->size() + bytes.size() > options.maxFileBytes)
            file->close();
        if (!file->isOpen())
        {
            if (!QDir().mkpath(options.directory))
                return fileError();
            if (!directoryLock)
            {
                directoryLock =
                    std::make_unique<QLockFile>(QDir(options.directory).filePath(QStringLiteral("writer.lock")));
                if (!directoryLock->tryLock(0))
                    return fileError();
            }
            // 启动/滚动时只向未满的末段追加，绝不覆盖已有日志。
            do
            {
                if (segment >= 999999)
                    return fileError();
                const QString name =
                    QStringLiteral("oran7-%1-%2.log").arg(date).arg(++segment, 6, 10, QLatin1Char('0'));
                file->setFileName(QDir(options.directory).filePath(name));
                if (!file->open(QIODevice::WriteOnly | QIODevice::Append))
                    return fileError();
                if (file->size() == 0 || file->size() + bytes.size() <= options.maxFileBytes)
                    break;
                file->close();
            } while (true);
            prune();
            if (fileDisabled)
                return;
        }
        if (file->write(bytes) != bytes.size() || !file->flush())
            fileError();
    }

    void output(QtMsgType type, const QString& text)
    {
        const QDateTime now = options.clock();
        const auto fileChars = std::min<qint64>(options.maxMessageChars, (options.maxFileBytes - 128) / 4);
        const QString boundedText = options.directory.isEmpty() ? text : text.left(fileChars);
        const QByteArray bytes =
            (now.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")) +
             QStringLiteral(" [%1] ").arg(QLatin1String(levelName(type))) + boundedText + QLatin1Char('\n'))
                .toUtf8();
        if (options.consoleEnabled)
        {
            try
            {
                options.consoleWriter(type, bytes);
            }
            catch (...)
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++stats.consoleErrors;
            }
        }
        writeFile(bytes, now);
    }

    void run()
    {
        inSink = true;
        file = std::make_unique<QFile>(); // 在写线程创建/销毁，不跨线程操作 QObject。
        for (;;)
        {
            Record record;
            quint64 drops;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [this] { return stopping || !queue.empty(); });
                if (queue.empty())
                    break;
                record = std::move(queue.front());
                queue.pop_front();
                drops = std::exchange(pendingDrops, 0);
            }
            try
            {
                if (drops)
                    output(QtWarningMsg, QStringLiteral("[日志] 队列已满，丢弃 %1 条新消息").arg(drops));
                output(record.type, record.text);
            }
            catch (...)
            {
                // I/O/宿主回调异常不逃离线程，继续消费队列并允许退出。
                std::lock_guard<std::mutex> lock(mutex);
                ++stats.fileErrors;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                stats.completed = record.sequence;
            }
            drained.notify_all();
        }
        file->close();
        file.reset();
        directoryLock.reset();
    }
};

Oran7LogSink::Oran7LogSink(Options options) : m_impl(std::make_unique<Impl>(std::move(options))) {}

Oran7LogSink::~Oran7LogSink()
{
    shutdown();
    std::lock_guard<std::mutex> lock(handlerMutex);
    if (handlerSink == this)
    {
        handlerSink = nullptr;
        qInstallMessageHandler(previousHandler);
    }
}

Oran7LogSink& Oran7LogSink::global()
{
    static Oran7LogSink sink(
        []
        {
            Options options;
            options.directory =
                QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/Logs");
            return options;
        }());
    return sink;
}

bool Oran7LogSink::installQtHandler()
{
    std::lock_guard<std::mutex> lock(handlerMutex);
    if (handlerSink && handlerSink != this)
        return false;
    if (handlerSink == this)
        return true;
    previousHandler = qInstallMessageHandler(qtMessageHandler);
    handlerSink = this;
    return true;
}

bool Oran7LogSink::submit(QtMsgType type, const QString& message, const QString& category)
{
    QString line = redact(message);
    if (!category.isEmpty() && category != QLatin1String("default"))
        line.prepend(QStringLiteral("[%1] ").arg(redact(category).left(128)));
    if (line.size() > m_impl->options.maxMessageChars)
        line = line.left(m_impl->options.maxMessageChars) + QStringLiteral(" [日志已截断]");
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->stopping)
    {
        ++m_impl->stats.rejectedAfterShutdown;
        return false;
    }
    if (m_impl->queue.size() >= size_t(m_impl->options.queueCapacity))
    {
        ++m_impl->stats.dropped;
        ++m_impl->pendingDrops;
        return false;
    }
    const auto sequence = ++m_impl->stats.accepted;
    m_impl->queue.push_back({type, std::move(line), sequence});
    m_impl->stats.peakQueued = std::max(m_impl->stats.peakQueued, int(m_impl->queue.size()));
    m_impl->ready.notify_one();
    return true;
}

bool Oran7LogSink::flush(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_impl->mutex);
    const auto target = m_impl->stats.accepted;
    return m_impl->drained.wait_for(lock, timeout, [&] { return m_impl->stats.completed >= target; });
}

void Oran7LogSink::shutdown()
{
    std::lock_guard<std::mutex> lifecycle(m_impl->shutdownMutex);
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->stopping = true;
    }
    m_impl->ready.notify_all();
    if (m_impl->worker.joinable())
        m_impl->worker.join();
}

Oran7LogSink::Statistics Oran7LogSink::statistics() const
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto stats = m_impl->stats;
    stats.queued = int(m_impl->queue.size());
    return stats;
}
