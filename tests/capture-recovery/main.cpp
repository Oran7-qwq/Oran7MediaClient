#include "Oran7ScreenCapture.h"
#include "Oran7DesktopCaptureWorker.h"
#include "Oran7RecordingBackend.h"
#include "Oran7AudioCaptureBackend.h"
#include <QScopeGuard>
#include "Live/Oran7LiveBackend.h"
#include "Oran7AppConfigManager.h"
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include <QElapsedTimer>
#include <cstdio>
#include <cstdlib>
#include <QTimer>
#include <algorithm>

static std::mutex liveAudioProbeMutex;
static std::shared_ptr<Oran7LiveAudioControl> liveAudioProbe;
static std::atomic_bool forceLiveFailure{false};
// 退出审计：后台不依赖 GUI 消息，输出尾段结束前采集源必须存活。
static std::atomic_bool shutdownAudit{false}, shutdownPreparing{false};
static std::atomic_bool shutdownRecordEntered{false}, shutdownLiveEntered{false};
static std::atomic_bool shutdownRecordCancelled{false}, shutdownRecordDone{false}, shutdownLiveDone{false};
static std::atomic_bool shutdownOrderOk{true};
QString Oran7LiveBackend::run(const Oran7LiveConfig&,std::shared_ptr<Oran7RecordingFeed>,
                            const std::atomic_bool& cancelled,const Oran7RecordingCallbacks& cb, std::shared_ptr<Oran7LiveAudioControl> control, std::shared_ptr<Oran7AudioCaptureFeed> audioFeed) {
    if(audioFeed){const auto error=Oran7AudioCaptureBackend::waitReady(audioFeed,cancelled);if(!error.isEmpty() || cancelled.load())return error;}
    {std::lock_guard<std::mutex> lock(liveAudioProbeMutex);liveAudioProbe=std::move(control);}
    shutdownLiveEntered=true;
    if (!shutdownPreparing.load()) cb.started({});
    while(!cancelled.load()){if(forceLiveFailure.load())return QStringLiteral("RTMP 发送失败、超时或已取消");QThread::msleep(1);}
    if (shutdownAudit.load()) {
        for(int i=0;i<300 && !shutdownRecordCancelled.load();++i) QThread::msleep(1);
        if(!shutdownRecordCancelled.load()) shutdownOrderOk=false;
        shutdownLiveDone=true;
        cb.started({});cb.statistics({{"late",true}}); // 退出后的迟到消息不能复活会话。
    }
    return {};
}
static std::atomic_int audioCaptureStarts{0}, audioCaptureActive{0}, audioCaptureMax{0};
static std::atomic_int audioCaptureFailure{0};
static std::mutex audioSourceProbeMutex;
static std::shared_ptr<Oran7AudioCaptureFeed> audioSourceProbe;
void Oran7AudioEncoder::pushFrame(AVFrame* frame) { av_frame_free(&frame); }
QString Oran7AudioCaptureBackend::run(std::shared_ptr<Oran7AudioCaptureFeed> feed,
    std::shared_ptr<Oran7LiveAudioControl> control,const std::atomic_bool& cancel,const Oran7RecordingCallbacks& cb)
{
    ++audioCaptureStarts;const int count=++audioCaptureActive;audioCaptureMax.store(std::max(count,audioCaptureMax.load()));
    int error=0;
    const auto guard=qScopeGuard([&]{
        if(shutdownAudit.load() && (!shutdownRecordDone.load() || !shutdownLiveDone.load())) shutdownOrderOk=false;
        QThread::msleep(30);feed->close(error);--audioCaptureActive;
    });
    {std::lock_guard<std::mutex> lock(audioSourceProbeMutex);audioSourceProbe=feed;}
    if(cancel.load())return {};
    if(audioCaptureFailure.load()) {error=audioCaptureFailure.load();return errorMessage(error);}
    feed->setOrigin(Oran7WasapiAudioWorker::clockNow100ns());
    auto* f=av_frame_alloc();f->format=AV_SAMPLE_FMT_S16;f->sample_rate=48000;f->nb_samples=480;f->pts=0;
    av_channel_layout_default(&f->ch_layout,2);av_frame_get_buffer(f,0);memset(f->data[0],0,1920);feed->submit(f);
    cb.started({});
    while(!cancel.load())QThread::msleep(2);
    return {};
}
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
    if(shutdownAudit.load() && (!shutdownRecordDone.load() || !shutdownLiveDone.load())) shutdownOrderOk=false;
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
static std::mutex recordingAudioProbeMutex;
static std::shared_ptr<Oran7LiveAudioControl> recordingAudioProbe;
QString Oran7RecordingBackend::run(const Oran7RecordingConfig&, std::shared_ptr<Oran7RecordingFeed> feed,
                                   const std::atomic_bool& cancelled, const Oran7RecordingCallbacks& cb, std::shared_ptr<Oran7LiveAudioControl> control, std::shared_ptr<Oran7AudioCaptureFeed> audioFeed)
{
    if(audioFeed){const auto error=Oran7AudioCaptureBackend::waitReady(audioFeed,cancelled);if(!error.isEmpty() || cancelled.load())return error;}
    { std::lock_guard<std::mutex> lock(recordingAudioProbeMutex); recordingAudioProbe=std::move(control); }
    shutdownRecordEntered=true;
    if (!shutdownPreparing.load()) cb.started({});
    while (!cancelled.load())
        QThread::msleep(1);
    if(shutdownAudit.load()) {
        shutdownRecordCancelled=true;
        QThread::msleep(60); // 模拟无需 GUI 的混音尾段/文件收尾。
        if(!feed->snapshot().alive || !audioFeed || !audioFeed->snapshot().alive) shutdownOrderOk=false;
        shutdownRecordDone=true;
        cb.started({});cb.stopping();
    }
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
        check(capture.applyAudioSettings({{"audioEnabled",true},{"microphoneEnabled",true},{"systemVolume",35},{"microphoneVolume",65}}).value("ok").toBool(),"save audio preset before creating recording generation");
        check(capture.startRecording(), "recording accepts start with recovered worker");
        until([&] { return capture.isRecording(); });
        {
            std::lock_guard<std::mutex> lock(recordingAudioProbeMutex);
            check(recordingAudioProbe && recordingAudioProbe->get().microphoneEnabled && recordingAudioProbe->get().microphoneVolume==65 && recordingAudioProbe->get().systemVolume==35,
                  "initial recording control loads saved microphone and gain before worker starts");
        }

        check(capture.previewAudioSettings({{"audioEnabled",false},{"microphoneEnabled",true},{"systemVolume",30},{"microphoneVolume",72}}).value("ok").toBool(),"local recording allows realtime shared audio changes");
        check(capture.applyAudioSettings({{"audioEnabled",false},{"microphoneEnabled",true},{"systemVolume",30},{"microphoneVolume",72}}).value("ok").toBool() && !config.snapshot().recording.audioEnabled && config.snapshot().live.microphoneVolume==72,
              "local audio settings persist together with legacy system switch");
        const auto oldAudio=config.snapshot();config.saveFails=true;
        check(!capture.applyAudioSettings({{"microphoneVolume",10}}).value("ok").toBool() && config.snapshot().live.microphoneVolume==oldAudio.live.microphoneVolume,"failed local audio save restores snapshot");
        config.saveFails=false;
        check(!capture.applyAudioSettings({{"microphoneVolume",101}}).value("ok").toBool() && !capture.applyAudioSettings({{"fps",30}}).value("ok").toBool(),"audio endpoint rejects invalid volume and video fields");
        check(capture.isRecording(),"audio edits preserve recording state");
        std::shared_ptr<Oran7LiveAudioControl> activeAudio;
        {std::lock_guard<std::mutex> lock(recordingAudioProbeMutex);activeAudio=recordingAudioProbe;}
        check(activeAudio && activeAudio->get().microphoneVolume==72 && activeAudio->get().microphoneEnabled,
              "recording backend retains the same control snapshot after started callback and gain edits");
        check(capture.previewAudioSettings({{"audioEnabled",true},{"microphoneEnabled",true},{"systemVolume",40},{"microphoneVolume",60}}).value("ok").toBool(),"both source levels can be previewed in recording");
        check(activeAudio->get().microphoneVolume==60 && activeAudio->get().systemVolume==40,"hot gain reaches actual recording control");
        activeAudio->publishLevels({Oran7AudioSourceLevel{.5f,false},Oran7AudioSourceLevel{.95f,true}});
        until([&]{return capture.audioLevels().value("microphoneFullScale").toBool();});
        check(capture.audioLevels().value("active").toBool() && capture.audioLevels().value("systemPeak").toDouble()>.49,"recording meters read real control snapshot independently of statistics");

        invalidate(latest.load(), "display changed during recording");
        until([&] { return !capture.recordingBusy(); });
        check(!capture.audioLevels().value("active").toBool() && capture.audioLevels().value("systemPeak").toDouble()==0,"recording end clears peak display");
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
    {
        mode=0;
        auto settings=config.snapshot();settings.live={"custom","rtmp://example.test/live","FICTIONAL",1920,1080,60,6000,true};
        settings.recording.fps=30;config.updateConfig(settings);
        Oran7ScreenCaptureController capture;capture.setVideoItem(&item);
        check(capture.start(),"live preview starts");
        until([&]{return capture.previewStateName()=="Running";});
        check(capture.startLive(),"independent live session starts from running preview");
        until([&]{return capture.liveBusy();});
        check(capture.startRecording() && !capture.restartPreview(),"live permits local recording but blocks destructive preview restart");
        until([&]{return capture.isRecording();});
        check(capture.liveBusy(),"starting recording keeps live active");
        capture.stopRecording();until([&]{return !capture.recordingBusy();});
        check(capture.liveBusy(),"stopping recording keeps live active");
        check(!capture.applyLiveSettings({{"fps",30}}).value("ok").toBool(),"live parameters locked while streaming");
        check(!capture.applyCaptureSettings(2,60,true,false).value("ok").toBool(),"capture parameters locked while streaming");
        until([&]{return capture.liveSession()->property("state").toString()=="Streaming";});
        check(capture.previewLiveAudioSettings({{"microphoneEnabled",true},{"microphoneVolume",65},{"systemVolume",25}}).value("ok").toBool(), "audio gains update during live without config writes");
        {
            std::lock_guard<std::mutex> lock(liveAudioProbeMutex);
            check(bool(liveAudioProbe),"live backend holds active audio control");
            liveAudioProbe->publishLevels({Oran7AudioSourceLevel{.2f,false},Oran7AudioSourceLevel{.9f,true}});
        }
        until([&]{return capture.audioLevels().value("microphoneFullScale").toBool();});
        check(capture.audioLevels().value("active").toBool() && capture.audioLevels().value("microphonePeak").toDouble()>.89,"streaming meter reads live session mailbox");
        check(capture.applyLiveSettings({{"microphoneEnabled",true},{"microphoneVolume",65},{"systemVolume",25}}).value("ok").toBool() && config.snapshot().live.microphoneEnabled && config.snapshot().live.microphoneVolume==65,
              "audio-only settings save while streaming");
        check(!capture.previewLiveAudioSettings({{"microphoneVolume",101}}).value("ok").toBool() && !capture.previewLiveAudioSettings({{"microphoneEnabled",1}}).value("ok").toBool(), "invalid live audio level and bool types rejected");
        capture.stopLive();until([&]{return !capture.liveBusy();});
        const auto before=config.snapshot();config.saveFails=true;
        check(!capture.applyLiveSettings({{"bitrateKbps",4000}}).value("ok").toBool() && config.snapshot().live.bitrateKbps==before.live.bitrateKbps,
              "failed live save rolls back settings");config.saveFails=false;
        check(capture.applyLiveSettings({{"fps",30},{"bitrateKbps",4000}}).value("ok").toBool() && config.snapshot().recording.fps==30,
              "live settings save without rewriting recording options");
        check(capture.startLive(),"live can retry after ordinary stop");capture.stop();
        until([&]{return !capture.liveBusy();});
    }
    {
        auto options=config.snapshot();options.recording.mode=0;options.recording.fps=60;options.live.fps=60;
        options.capture.fps=60;config.updateConfig(options);
        mode=0;
        Oran7ScreenCaptureController capture;capture.setVideoItem(&item);
        check(capture.start(),"video preview for persistent audio source fixture");
        until([&]{return capture.previewStateName()=="Running";});
        check(capture.applyAudioSettings({{"audioEnabled",true},{"microphoneEnabled",true},{"systemVolume",40},{"microphoneVolume",60}}).value("ok").toBool(),"set idle monitor source settings");
        capture.setAudioMonitoringEnabled(true);
        until([&]{return capture.audioLevels().value("active").toBool();});
        const int opened=audioCaptureStarts.load();
        std::shared_ptr<Oran7AudioCaptureFeed> original;
        {std::lock_guard<std::mutex> lock(audioSourceProbeMutex);original=audioSourceProbe;}
        check(!capture.recordingBusy() && !capture.liveBusy() && audioCaptureActive.load()==1,"idle monitoring never starts output sessions");
        check(capture.startRecording(),"recording taps persistent audio source");until([&]{return capture.isRecording();});
        check(audioCaptureStarts.load()==opened && original->snapshot().alive,"recording does not reopen the audio source");
        check(capture.startLive(),"RTMP may start while recording is already active");
        until([&]{return capture.liveSession()->property("state").toString()=="Streaming";});
        check(capture.isRecording() && audioCaptureStarts.load()==opened,"two output sessions share one audio source");
        forceLiveFailure=true;until([&]{return !capture.liveBusy();});
        check(capture.isRecording() && audioCaptureStarts.load()==opened,"RTMP failure does not stop recording or shared capture");
        forceLiveFailure=false;
        check(capture.startLive(),"RTMP may retry while recording continues");
        until([&]{return capture.liveSession()->property("state").toString()=="Streaming";});

        capture.stopLive();until([&]{return !capture.liveBusy();});
        check(capture.isRecording(),"stopping RTMP keeps local recording active");
        capture.stopRecording();until([&]{return !capture.recordingBusy();});
        check(capture.audioLevels().value("active").toBool() && original->snapshot().alive,"idle meters continue after recording stops");
        check(capture.startLive(),"streaming taps the same persistent audio source");
        until([&]{return capture.liveSession()->property("state").toString()=="Streaming";});
        check(audioCaptureStarts.load()==opened,"RTMP start reuses capture devices");
        capture.setAudioMonitoringEnabled(false);
        check(original->snapshot().alive,"leaving workspace keeps active output audio source alive");
        capture.stopLive();until([&]{return !capture.liveBusy() && audioCaptureActive.load()==0;});
        check(!capture.audioLevels().value("active").toBool(),"hidden idle workspace releases capture and clears meters");
        capture.setAudioMonitoringEnabled(true);until([&]{return capture.audioLevels().value("active").toBool();});
        capture.setAudioMonitoringEnabled(false);QCoreApplication::processEvents();
        check(capture.startRecording(),"output can start while old idle source is retiring");
        until([&]{return capture.isRecording();});
        check(audioCaptureMax.load()==1,"rapid source transitions never overlap capture generations");
        capture.stopRecording();until([&]{return !capture.recordingBusy() && audioCaptureActive.load()==0;});
        audioCaptureFailure=int(Oran7AudioMixPipeline::Error::Microphone);
        capture.setAudioMonitoringEnabled(true);
        until([&]{return !capture.audioLevels().value("error").toString().isEmpty();});
        const int failedStarts=audioCaptureStarts.load();QTest::qWait(80);
        check(audioCaptureStarts.load()==failedStarts,"failed detection does not retry in a loop");
        capture.setAudioMonitoringEnabled(false);audioCaptureFailure=0;
        capture.setAudioMonitoringEnabled(true);until([&]{return capture.audioLevels().value("active").toBool();});
        capture.shutdown();check(audioCaptureActive.load()==0,"shutdown releases persistent source");
    }
    for(int phase=0;phase<3;++phase) {
        auto options=config.snapshot();options.recording.mode=0;options.recording.fps=60;
        options.live.fps=60;options.live.server="rtmp://example.test/live";options.live.streamKey="FICTIONAL";
        config.updateConfig(options);mode=0;
        shutdownRecordEntered=false;shutdownLiveEntered=false;shutdownRecordCancelled=false;
        shutdownRecordDone=false;shutdownLiveDone=false;shutdownOrderOk=true;shutdownPreparing=phase==1;
        Oran7ScreenCaptureController capture;capture.setVideoItem(&item);capture.start();
        until([&]{return capture.previewStateName()=="Running";});
        check(capture.startRecording() && capture.startLive(),"dual shutdown fixture starts both outputs");
        until([]{return shutdownRecordEntered.load() && shutdownLiveEntered.load();});
        if(phase==0) until([&]{return capture.isRecording() && capture.liveSession()->property("state")=="Streaming";});
        shutdownAudit=true;
        if(phase==2){capture.stopRecording();capture.stopLive();}
        int flushes=0;
        QObject::connect(&capture,&Oran7ScreenCaptureController::settingsFlushRequested,&capture,[&]{
            ++flushes;
            check(capture.applyAudioSettings({{"systemVolume",33}}).value("ok").toBool(),"shutdown flush signal precedes freezing audio settings API");
            capture.shutdown(); // 补交期间重入不能重复发信号或提前关闭源。
        });
        QElapsedTimer elapsed;elapsed.start();capture.shutdown();
        check(flushes==1 && config.snapshot().live.systemVolume==33,"shutdown flush is synchronous and reentrant-safe");
        check(elapsed.elapsed()<1500 && shutdownOrderOk.load(),"shutdown cancels both before waiting and retains sources through file tail");
        check(shutdownRecordDone.load() && shutdownLiveDone.load() && audioCaptureActive.load()==0 && liveWorkers.load()==0,
              "shutdown joins both outputs and releases audio/video workers without GUI event pumping");
        shutdownAudit=false;shutdownPreparing=false;
        QSignalSpy changed(&capture,&Oran7ScreenCaptureController::recordingStateChanged);
        capture.shutdown();QTest::qWait(30);
        check(changed.isEmpty() && !capture.startLive() && !capture.startRecording(),"repeated shutdown and late callbacks cannot restart outputs");
    }
    {
        Oran7ScreenCaptureController capture;capture.setVideoItem(&item);capture.start();
        until([&]{return capture.previewStateName()=="Running";});
        check(capture.startRecording() && capture.startLive(),"immediate quit fixture accepts two output requests");
        // 不处理排队的音频启动：直接在准备请求提交后关闭。
        QElapsedTimer elapsed;elapsed.start();capture.shutdown();
        check(elapsed.elapsed()<1500 && audioCaptureActive.load()==0 && liveWorkers.load()==0,
              "quit before queued audio startup cancels both readiness waits and releases video");
        const auto opened=audioCaptureStarts.load();QTest::qWait(30);
        check(audioCaptureStarts.load()==opened,"queued audio startup cannot run after shutdown");
    }
    asyncChecks(item, config);
    std::printf("PASS: %d controller recovery checks\n", checks);
}
