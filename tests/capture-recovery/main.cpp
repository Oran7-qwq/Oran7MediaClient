#include "Oran7ScreenCapture.h"
#include "Oran7DesktopCaptureWorker.h"
#include "Oran7RecordingBackend.h"
#include "Oran7AppConfigManager.h"
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include <QElapsedTimer>
#include <cstdio>
#include <cstdlib>
#include <QTimer>
#include <algorithm>

static std::atomic_int starts{0}, mode{0}, destructorDelay{0}, liveWorkers{0}, maxWorkers{0};
static std::atomic<Oran7DesktopCaptureWorker*> latest{nullptr};
static QVariantList outputs{QVariantMap{{"outputIndex", 0}, {"attached", true}, {"deviceName", "DISPLAY-A"}}};
QVariantList oran7FixtureMonitors()
{
    return outputs;
}
Oran7DesktopCaptureWorker::Oran7DesktopCaptureWorker(std::function<void(AVFrame*)> submitFrame, int outputIndex,
                                                     bool drawMouse, int fps)
    : m_submitFrame(std::move(submitFrame)), m_outputIndex(outputIndex), m_drawMouse(drawMouse), m_fps(fps)
{
    const int live = ++liveWorkers;
    maxWorkers.store(qMax(maxWorkers.load(), live));
}
Oran7DesktopCaptureWorker::~Oran7DesktopCaptureWorker()
{
    Q_ASSERT(thread() == QThread::currentThread());
    QThread::msleep(destructorDelay.load());
    stop();
    cleanup();
    --liveWorkers;
}
void Oran7DesktopCaptureWorker::stop()
{
    m_control->stopRequested.store(true);
}
void Oran7DesktopCaptureWorker::cleanup()
{
    m_recordingFeed->close();
}
void Oran7DesktopCaptureWorker::start()
{
    latest.store(this);
    const int number = ++starts;
    const int behavior = mode.load();
    if (m_control->stopRequested.load())
        return;
    if (behavior == 4)
    {
        // 模拟不可即时取消的驱动调用，并故意发送已退役的通知。
        QThread::msleep(180);
        emit firstFrameReady();
        emit errorOccurred("stale driver error");
        return;
    }
    if (behavior == 2 || behavior == 7 || (behavior == 6 && m_fps == 120) || (behavior == 1 && number == 2))
    {
        emit errorOccurred("temporary DDA initialization failure");
        return;
    }
    emit started();
    if (behavior == 3 || (behavior == 1 && number == 1))
    {
        emit captureInvalidated("output changed");
        emit stopped();
    }
    else if (behavior != 5)
        emit firstFrameReady();
}
QString Oran7RecordingBackend::run(const Oran7RecordingConfig&, std::shared_ptr<Oran7RecordingFeed>,
                                   const std::atomic_bool& cancelled, const Oran7RecordingCallbacks& cb)
{
    cb.started({});
    while (!cancelled.load())
        QThread::msleep(1);
    return {};
}
QVariantMap Oran7RecordingBackend::settings(const Oran7RecordingConfig&)
{
    return {};
}
QString Oran7RecordingBackend::apply(const QVariantMap& values, Oran7RecordingConfig* config)
{
    if (values.contains("fps"))
        config->fps = values.value("fps").toInt();
    return {};
}

static int checks = 0;
static void check(bool good, const char* label)
{
    if (!good)
    {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
    ++checks;
}
template <class F> void until(F ready)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!ready() && elapsed.elapsed() < 5000)
        QTest::qWait(2);
    check(ready(), "expected event completed");
}
static void invalidate(Oran7DesktopCaptureWorker* worker, const QString& reason)
{
    QMetaObject::invokeMethod(
        worker,
        [worker, reason]
        {
            emit worker->captureInvalidated(reason);
            emit worker->stopped();
        },
        Qt::QueuedConnection);
}
#include "async-cases.h"

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    D3D11VideoItem item;
    auto& config = Oran7AppConfigManager::ins();
    auto saved = config.snapshot();
    saved.recording.fps = 60;
    saved.capture.fps = 60;
    config.updateConfig(saved);
    {
        Oran7ScreenCaptureController capture;
        check(!capture.applyRecordingSettings({{"fps", 120}}).value("ok").toBool(),
              "backend rejects output FPS above applied capture");
        check(config.snapshot().recording.fps == 60, "rejected FPS does not persist");
        check(capture.applyRecordingSettings({{"fps", 30}}).value("ok").toBool(), "backend accepts lower output FPS");
        check(!capture.applyCaptureSettings(0, 20, true, false).value("ok").toBool(),
              "backend rejects capture FPS below saved output before device enumeration");
        saved = config.snapshot();
        saved.recording.fps = 120;
        config.updateConfig(saved);
        QSignalSpy errors(&capture, &Oran7ScreenCaptureController::errorOccurred);
        check(!capture.startRecording() && errors.size() == 1, "legacy inconsistent FPS cannot start recording");
        saved.recording.fps = 60;
        config.updateConfig(saved);
    }
    {
        mode = 1;
        starts = 0;
        Oran7ScreenCaptureController capture;
        capture.setVideoItem(&item);
        QSignalSpy errors(&capture, &Oran7ScreenCaptureController::errorOccurred);
        QSignalSpy stopped(&capture, &Oran7ScreenCaptureController::stopped);
        check(capture.start(), "preview accepts start");
        until([] { return starts == 3; });
        QTest::qWait(30);
        check(errors.isEmpty(), "recoverable loss and temporary initialization failure do not become fatal");
        check(stopped.isEmpty(), "old queued stopped cannot stop recovery");
        // 首帧后再次切换，预算应重新开始，预览仍可恢复。
        mode = 0;
        invalidate(latest.load(), "second display change");
        until([] { return starts == 4; });
        QTest::qWait(30);
        check(stopped.isEmpty(), "second display change does not latch stopped state");
        check(capture.startRecording(), "recording accepts start with recovered worker");
        until([&] { return capture.isRecording(); });
        invalidate(latest.load(), "display changed during recording");
        until([&] { return !capture.recordingBusy(); });
        until([] { return starts == 5; });
        check(errors.size() == 1 && errors.first().first().toString().contains(QString::fromUtf8("已停止录制")),
              "recording is closed with explanation instead of rebinding its feed");
        capture.stop();
        const int count = starts;
        QTest::qWait(350);
        check(starts == count, "explicit stop cancels recovery");
    }
    {
        mode = 3;
        starts = 0;
        Oran7ScreenCaptureController capture;
        capture.setVideoItem(&item);
        QSignalSpy errors(&capture, &Oran7ScreenCaptureController::errorOccurred);
        QSignalSpy stopped(&capture, &Oran7ScreenCaptureController::stopped);
        capture.start();
        until([&] { return !errors.isEmpty(); });
        check(starts == 9, "no-frame invalidation has finite eight-rebuild budget");
        check(stopped.size() == 1, "retry exhaustion stops exactly once");
        const int count = starts;
        QTest::qWait(350);
        check(starts == count, "retry exhaustion leaves no pending restart");
        mode = 0;
        check(capture.start(), "manual start after exhaustion");
        until([&] { return starts == count + 1; });
        capture.stop();
    }
    {
        mode = 2;
        starts = 0;
        Oran7ScreenCaptureController capture;
        capture.setVideoItem(&item);
        QSignalSpy errors(&capture, &Oran7ScreenCaptureController::errorOccurred);
        capture.start();
        until([&] { return errors.size() == 1; });
        QTest::qWait(350);
        check(starts == 1, "ordinary initial failure does not enter display recovery");
    }
    {
        mode = 0;
        starts = 0;
        Oran7ScreenCaptureController capture;
        capture.setVideoItem(&item);
        capture.start();
        until([] { return starts == 1; });
        QTest::qWait(20);
        QPointer<Oran7DesktopCaptureWorker> worker = latest.load();
        invalidate(worker, "cancel pending recovery");
        until([&] { return worker.isNull(); });
        capture.stop();
        QTest::qWait(350);
        check(starts == 1, "stop during delay cannot resurrect preview");
    }
    {
        mode = 0;
        starts = 0;
        outputs = {QVariantMap{{"outputIndex", 1}, {"attached", true}, {"deviceName", "DISPLAY-B"}}};
        saved = config.snapshot();
        saved.capture.outputIndex = 1;
        saved.capture.fps = 60;
        config.updateConfig(saved);
        Oran7ScreenCaptureController capture;
        capture.setVideoItem(&item);
        capture.loadCaptureSettings();
        capture.start();
        until([] { return starts == 1; });
        QTest::qWait(20);
        // 同一物理输出从序号 1 变为 0，重建必须刷新，不能重复使用旧序号。
        outputs = {QVariantMap{{"outputIndex", 0}, {"attached", true}, {"deviceName", "DISPLAY-B"}}};
        invalidate(latest.load(), "output index changed");
        until([] { return starts == 2; });
        QTest::qWait(20);
        check(capture.captureSettings().value("outputIndex").toInt() == 0,
              "topology change remaps surviving display to raw index zero");
        check(config.snapshot().capture.outputIndex == 0, "remapped source persists");
        outputs = {QVariantMap{{"outputIndex", 0}, {"attached", true}, {"deviceName", "DISPLAY-A"}},
                   QVariantMap{{"outputIndex", 4}, {"attached", true}, {"deviceName", "DISPLAY-B"}}};
        invalidate(latest.load(), "source index reused");
        until([] { return starts == 3; });
        QTest::qWait(20);
        check(capture.captureSettings().value("outputIndex").toInt() == 4, "device identity wins over reused index");
        check(!capture.applyCaptureSettings(1, 60, true, false).value("ok").toBool(),
              "missing raw index rejected despite being below list size");
        outputs.push_back(QVariantMap{{"outputIndex", 7}, {"attached", false}, {"deviceName", "DISPLAY-C"}});
        check(!capture.applyCaptureSettings(7, 60, true, false).value("ok").toBool(), "detached source rejected");
        capture.stop();
        QSignalSpy applied(&capture, &Oran7ScreenCaptureController::captureApplyFinished);
        const auto result = capture.applyCaptureSettings(4, 60, true, false);
        check(result.value("ok").toBool() && result.value("pending").toBool(),
              "apply unchanged settings restarts failed preview with sparse raw index");
        until([&] { return applied.size() == 1; });
        check(applied.first().first().toBool(), "restart apply confirms asynchronously");
        const int count = starts;
        check(capture.restartPreview(), "explicit reconnect accepted");
        until([&] { return starts == count + 1; });
        QTest::qWait(20);
        check(capture.startRecording(), "recording starts after reconnect");
        until([&] { return capture.recordingBusy(); });
        check(!capture.restartPreview(), "reconnect cannot interrupt active recording");
        capture.stop();
        until([&] { return !capture.recordingBusy(); });
        outputs.clear();
        check(!capture.applyCaptureSettings(4, 60, true, false).value("ok").toBool(),
              "empty topology rejects stale setting");
        check(capture.restartPreview(), "reconnect waits for temporary empty topology");
        outputs = {QVariantMap{{"outputIndex", 2}, {"attached", true}, {"deviceName", "DISPLAY-D"}}};
        until([&] { return starts == count + 2; });
        QTest::qWait(20);
        check(capture.captureSettings().value("outputIndex").toInt() == 2,
              "missing display falls back to attached raw output after topology returns");
        capture.stop();
    }
    asyncChecks(item, config);
    std::printf("PASS: %d controller recovery checks\n", checks);
}
