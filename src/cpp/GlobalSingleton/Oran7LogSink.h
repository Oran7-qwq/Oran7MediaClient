#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QtGlobal>
#include <chrono>
#include <functional>
#include <memory>

// Oran7Core 内唯一的日志出口；生产者只脱敏并入队，不执行文件或控制台 I/O。
class Oran7LogSink
{
  public:
    struct Options
    {
        QString directory; // 空目录表示仅输出控制台，独立检查不会写用户目录。
        int queueCapacity = 2048;
        int maxMessageChars = 8192;
        qint64 maxFileBytes = 8 * 1024 * 1024;
        int maxFiles = 10;
        bool consoleEnabled = true;
        // 可替换输出与时钟，供嵌入宿主和离线检查使用；均在写线程调用。
        std::function<void(QtMsgType, const QByteArray&)> consoleWriter;
        std::function<QDateTime()> clock;
    };

    struct Statistics
    {
        quint64 accepted = 0;
        quint64 completed = 0;
        quint64 dropped = 0;
        quint64 rejectedAfterShutdown = 0;
        quint64 fileErrors = 0;
        quint64 consoleErrors = 0;
        int queued = 0;
        int peakQueued = 0;
    };

    explicit Oran7LogSink(Options options);
    ~Oran7LogSink();
    Oran7LogSink(const Oran7LogSink&) = delete;
    Oran7LogSink& operator=(const Oran7LogSink&) = delete;

    static Oran7LogSink& global();
    static QString redact(QString message);
    bool installQtHandler();
    bool submit(QtMsgType type, const QString& message, const QString& category = {});
    bool flush(std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));
    void shutdown(); // 拒绝新消息，排空已接收队列，join；可重复调用。
    Statistics statistics() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
