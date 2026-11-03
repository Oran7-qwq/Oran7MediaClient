#include "Oran7FFmpegLogBridge.h"
#include "Oran7GlobalHelper.h"
#include "Oran7LogSink.h"

#include <QCoreApplication>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
extern "C"
{
#include <libavutil/log.h>
}

namespace
{
int checks = 0;
void check(bool value, const char* reason)
{
    if (!value)
    {
        std::fprintf(stderr, "FAIL: %s\n", reason);
        std::exit(1);
    }
    ++checks;
}

QByteArray read(const QString& path)
{
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "read artifact");
    return file.readAll();
}

void redaction()
{
    const QStringList samples = {"Cookie: MUSIC_U=SENTINEL_CREDENTIAL; __csrf=SECOND_SECRET",
                                 "Set-Cookie: SESSDATA=SENTINEL_CREDENTIAL; Path=/",
                                 "Authorization: Bearer SENTINEL_CREDENTIAL",
                                 "authorization=Basic SENTINEL_CREDENTIAL",
                                 "{\"result\":{\"password\":\"SENTINEL_CREDENTIAL\",\"nested\":true}}",
                                 "{\\\"cookie\\\":\\\"SENTINEL_CREDENTIAL\\\"}",
                                 "QMap((\"Cookie\", \"SENTINEL_CREDENTIAL\"))",
                                 "user_name=SENTINEL_CREDENTIAL; other=ok",
                                 "bili_jct=SENTINEL_CREDENTIAL",
                                 "csrfToken: SENTINEL_CREDENTIAL",
                                 "bearerToken=SENTINEL_CREDENTIAL",
                                 "credential=SENTINEL_CREDENTIAL",
                                 "body={unrecognized: SENTINEL_CREDENTIAL}",
                                 "params=SENTINEL_CREDENTIAL",
                                 "密码：SENTINEL_CREDENTIAL", // 中文全角分隔符也必须覆盖。
                                 "https://user:SENTINEL_CREDENTIAL@example.test/path-token?x=SECOND_SECRET#private",
                                 "QUrl(\"wss://example.test/SENTINEL_CREDENTIAL?token=SECOND_SECRET\")",
                                 "https:\\/\\/example.test/private?x=SENTINEL_CREDENTIAL",
                                 "v=0\r\na=ice-pwd:SENTINEL_CREDENTIAL\r\na=ice-ufrag:SECOND_SECRET",
                                 "line\nCookie: SENTINEL_CREDENTIAL\nmessage"};
    for (const auto& sample : samples)
    {
        const QString safe = Oran7LogSink::redact(sample);
        check(!safe.contains("SENTINEL_CREDENTIAL") && !safe.contains("SECOND_SECRET"), "secret removed");
        check(!safe.contains('\n') && !safe.contains('\r'), "single physical line");
    }
    check(Oran7LogSink::redact("[CaptureDiag] fps=120 status=200 bytes=512 colorToken=blue") ==
              "[CaptureDiag] fps=120 status=200 bytes=512 colorToken=blue",
          "diagnostics preserved");
    check(!Oran7LogSink::redact("eyJhbGciOiJIUzI1NiJ9.c2VjcmV0.c2ln").contains("c2VjcmV0"), "JWT removed");
}

void entryPoints()
{
    QTemporaryDir dir;
    check(dir.isValid(), "temporary log directory");
    QByteArray console;
    std::thread::id writer;
    Oran7LogSink::Options options;
    options.directory = dir.path();
    options.consoleWriter = [&](QtMsgType, const QByteArray& bytes)
    {
        writer = std::this_thread::get_id();
        console += bytes;
        qWarning("Cookie: RECURSION_SENTINEL"); // 写线程递归必须被截断。
    };
    Oran7LogSink sink(options);
    check(sink.installQtHandler() && sink.installQtHandler(), "handler installation idempotent");
    Oran7LogSink other(Oran7LogSink::Options{});
    check(!other.installQtHandler(), "second handler rejected");
    INFO_LOG << "[Test] Cookie: MACRO_SENTINEL";
    qWarning("Authorization: Bearer QT_SENTINEL");
    qInfo("https://user:URL_SENTINEL@example.test/signed?auth=QUERY_SENTINEL");
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQml\nQtObject { Component.onCompleted: console.log('password=QML_SENTINEL') }",
                          QUrl());
        std::unique_ptr<QObject> object(component.create());
        check(bool(object), "real QML console entry");
    }
    Oran7FFmpegLogBridge::install();
    av_log(nullptr, AV_LOG_WARNING, "https://example.test/private?token=FFMPEG_SENTINEL\n");
    av_log(nullptr, AV_LOG_FATAL, "ffmpeg-recoverable-fatal\n");
    check(sink.flush(), "all entry points flushed");
    sink.shutdown();
    check(writer != std::this_thread::get_id(), "I/O stays on dedicated writer");
    QByteArray files;
    for (const auto& name : QDir(dir.path()).entryList({"*.log"}, QDir::Files))
        files += read(dir.filePath(name));
    for (const auto& marker : {"MACRO_SENTINEL", "QT_SENTINEL", "URL_SENTINEL", "QUERY_SENTINEL", "QML_SENTINEL",
                               "FFMPEG_SENTINEL", "RECURSION_SENTINEL"})
    {
        check(!console.contains(marker) && !files.contains(marker), "both sinks redacted");
    }
    check(console.contains("ffmpeg-recoverable-fatal") && files.contains("ffmpeg-recoverable-fatal"),
          "FFmpeg fatal does not abort");
    check(files == console, "console and file share identical records");
    check(sink.statistics().completed >= 6, "Qt QML macro FFmpeg all delivered");
}

void overflow()
{
    std::mutex mutex;
    std::condition_variable signal;
    bool entered = false, release = false;
    QByteArray console;
    Oran7LogSink::Options options;
    options.queueCapacity = 4;
    options.consoleWriter = [&](QtMsgType, const QByteArray& bytes)
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        signal.notify_all();
        signal.wait(lock, [&] { return release; });
        console += bytes;
    };
    Oran7LogSink sink(options);
    check(sink.submit(QtInfoMsg, "first"), "first accepted");
    {
        std::unique_lock<std::mutex> lock(mutex);
        check(signal.wait_for(lock, std::chrono::seconds(3), [&] { return entered; }),
              "writer blocked deterministically");
    }
    for (int i = 0; i < 100; ++i)
        sink.submit(QtWarningMsg, QStringLiteral("queued-%1").arg(i));
    const auto before = sink.statistics();
    check(before.accepted == 5 && before.dropped == 96 && before.queued == 4 && before.peakQueued == 4,
          "bounded queue drops newest without blocking producer");
    check(!sink.flush(std::chrono::milliseconds(5)), "flush timeout on blocked sink");
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
    }
    signal.notify_all();
    sink.shutdown();
    sink.shutdown();
    check(sink.statistics().completed == 5 && sink.statistics().queued == 0, "shutdown drains accepted records");
    check(!sink.submit(QtCriticalMsg, "late"), "late writes rejected");
    check(sink.statistics().rejectedAfterShutdown == 1, "late write counted");
    check(console.contains("96") && console.contains("queued-3") && !console.contains("queued-4"),
          "drop summary and FIFO policy");
}

void concurrent()
{
    std::set<std::thread::id> writers;
    int received = 0;
    Oran7LogSink::Options options;
    options.queueCapacity = 16384;
    options.consoleWriter = [&](QtMsgType, const QByteArray&)
    {
        writers.insert(std::this_thread::get_id());
        ++received;
    };
    Oran7LogSink sink(options);
    std::vector<std::thread> producers;
    for (int t = 0; t < 8; ++t)
        producers.emplace_back(
            [&sink, t]
            {
                for (int i = 0; i < 1000; ++i)
                    sink.submit(QtInfoMsg, QStringLiteral("[Worker%1] item=%2").arg(t).arg(i));
            });
    for (auto& producer : producers)
        producer.join();
    sink.shutdown();
    const auto stats = sink.statistics();
    check(stats.accepted == 8000 && stats.completed == 8000 && stats.dropped == 0, "concurrent records accounted");
    check(received == 8000 && writers.size() == 1, "exactly one writer thread");

    Oran7LogSink racing(options);
    producers.clear();
    for (int t = 0; t < 4; ++t)
        producers.emplace_back(
            [&]
            {
                for (int i = 0; i < 1000; ++i)
                    racing.submit(QtInfoMsg, "shutdown race");
            });
    racing.shutdown();
    for (auto& producer : producers)
        producer.join();
    const auto race = racing.statistics();
    check(race.accepted + race.dropped + race.rejectedAfterShutdown == 4000 && race.completed == race.accepted,
          "shutdown concurrent with producers is accounted and drained");
}

void rotationAndFailure()
{
    QTemporaryDir dir;
    check(dir.isValid(), "rotation temp directory");
    QFile unrelated(dir.filePath("keep.txt"));
    check(unrelated.open(QIODevice::WriteOnly), "create unrelated file");
    unrelated.write("keep");
    unrelated.close();
    std::atomic<int> day{25};
    Oran7LogSink::Options options;
    options.directory = dir.path();
    options.consoleEnabled = false;
    options.maxFileBytes = 512;
    options.maxFiles = 3;
    options.clock = [&] { return QDateTime(QDate(2026, 10, day.load()), QTime(12, 0)); };
    {
        Oran7LogSink sink(options);
        for (int i = 0; i < 100; ++i)
            sink.submit(QtInfoMsg, QStringLiteral("record-%1 ").arg(i) + QString(120, 'x'));
        check(sink.flush(), "size rotation flush");
        day = 26;
        sink.submit(QtInfoMsg, "next-day-final");
        sink.shutdown();
        check(sink.statistics().fileErrors == 0, "rotation no errors");
    }
    {
        Oran7LogSink sink(options);
        sink.submit(QtInfoMsg, "restart-final");
        sink.shutdown();
    }
    const auto files = QDir(dir.path()).entryList({"oran7-*.log"}, QDir::Files, QDir::Name);
    check(files.size() == 3, "file retention bounded");
    QByteArray content;
    for (const auto& name : files)
    {
        const auto bytes = read(dir.filePath(name));
        check(bytes.size() <= 512, "segment size bounded");
        content += bytes;
    }
    check(content.contains("next-day-final") && content.contains("restart-final"),
          "daily roll and restart preserve tail");
    check(QFile::exists(unrelated.fileName()), "unrelated files preserved");

    {
        options.consoleEnabled = false;
        Oran7LogSink owner(options);
        owner.submit(QtInfoMsg, "directory-lock-owner");
        check(owner.flush(), "first writer owns directory");
        Oran7LogSink competitor(options);
        competitor.submit(QtInfoMsg, "directory-lock-competitor");
        competitor.shutdown();
        check(competitor.statistics().fileErrors == 1, "second process-style writer cannot rotate owner's files");
        owner.shutdown();
    }

    QFile blocker(dir.filePath("blocker"));
    check(blocker.open(QIODevice::WriteOnly), "unwritable path fixture");
    blocker.close();
    options.directory = blocker.fileName() + "/Logs";
    options.consoleEnabled = true;
    int delivered = 0;
    options.consoleWriter = [&](QtMsgType, const QByteArray&) { ++delivered; };
    Oran7LogSink failure(options);
    for (int i = 0; i < 20; ++i)
        failure.submit(QtInfoMsg, "fallback");
    failure.shutdown();
    check(failure.statistics().fileErrors == 1 && failure.statistics().completed == 20,
          "file failure disables retries and drains");
    check(delivered == 21, "one failure notice plus continued console output");

    options.directory = dir.filePath("console-failure");
    options.consoleWriter = [](QtMsgType, const QByteArray&) { throw 1; };
    Oran7LogSink badConsole(options);
    badConsole.submit(QtInfoMsg, "console-exception-final");
    badConsole.shutdown();
    check(badConsole.statistics().consoleErrors == 1 && badConsole.statistics().fileErrors == 0 &&
              badConsole.statistics().completed == 1,
          "console exception keeps file sink and shutdown alive");
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    redaction();
    entryPoints();
    overflow();
    concurrent();
    rotationAndFailure();
    std::printf("PASS: %d logging assertions\n", checks);
}
