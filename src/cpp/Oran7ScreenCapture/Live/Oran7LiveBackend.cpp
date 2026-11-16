#include "Oran7LiveBackend.h"
#include "Oran7RtmpPublisher.h"
#include "../Oran7AudioCaptureBackend.h"
#include "../Oran7AudioMixPipeline.h"
#include "../Oran7AudioEncoder.h"
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QDebug>
namespace
{
struct Oran7LiveResources
{
    std::shared_ptr<Oran7RecordingFeed> feed;
    std::shared_ptr<Oran7VideoEncoder> video = std::make_shared<Oran7VideoEncoder>();
    uint64_t videoSubscription=0;
    std::shared_ptr<Oran7AudioEncoder> audio;
    Oran7AudioMixPipeline audioMix;
    std::shared_ptr<Oran7AudioCaptureFeed> audioSource;
    uint64_t audioSubscription = 0;
    std::unique_ptr<Oran7RtmpPublisher> publisher;
    void stopAudioPump()
    {
        if (audioSource)
            audioSource->detach(audioSubscription);
        audioMix.stop();
    }
    ~Oran7LiveResources()
    {
        feed->detach(videoSubscription);
        stopAudioPump();
        if (audio)
            audio->stop();
        video->stop();
        if (publisher)
            publisher->close();
    }
};
} // namespace
QString Oran7LiveBackend::run(const Oran7LiveConfig& c, std::shared_ptr<Oran7RecordingFeed> feed,
                              const std::atomic_bool& cancel, const Oran7RecordingCallbacks& cb,
                              std::shared_ptr<Oran7LiveAudioControl> audioControl,
                              std::shared_ptr<Oran7AudioCaptureFeed> audioFeed)
{
    if (auto error = Oran7LiveSession::validate(c, true); !error.isEmpty())
        return error;
    if (!feed)
        return QStringLiteral("采集源不可用");
    const auto source = feed->snapshot();
    if (!source.alive || !source.device || !source.width)
        return QStringLiteral("请先启动并等待抓屏预览就绪");
    if (audioFeed)
    {
        const auto error = Oran7AudioCaptureBackend::waitReady(audioFeed, cancel);
        if (!error.isEmpty() || cancel.load())
            return error;
    }
    Oran7LiveResources r;
    r.audioSource = audioFeed;
    r.feed = feed;
    std::atomic_bool encoderFailed{false};
    const ErrorCallback onError = [&encoderFailed](const std::string&) { encoderFailed.store(true); };
    // 回调捕获的局部状态须覆盖编码器资源的析构，退出前显式停止编码器。
    const auto guard = qScopeGuard(
        [&]
        {
            r.feed->detach(r.videoSubscription);
            r.stopAudioPump();
            if (r.audio)
                r.audio->stop();
            r.video->stop();
        });
    Oran7VideoEncoder::EncoderConfig video;
    video.codecName = "h264_mf";
    video.width = c.width;
    video.height = c.height;
    video.fps = c.fps;
    video.gopSize = c.fps * 2;
    video.bitrateKbps = c.bitrateKbps;
    video.hwDevice = source.device.get();
    video.liveStreaming = true;
    video.hardwareLive = true;
    auto wait = [&](auto opened, auto running)
    {
        QElapsedTimer t;
        t.start();
        while (!cancel.load() && t.elapsed() < 3000)
        {
            if (opened())
                return true;
            if (!running())
                break;
            QThread::msleep(5);
        }
        return false;
    };
    if (!r.video->configure(video, onError) || !r.video->start() ||
        !wait([&] { return r.video->isOpened(); }, [&] { return r.video->isRuning(); }))
        return cancel.load() ? QString() : QStringLiteral("直播视频编码器准备失败");
    // 始终保留一路 AAC：全部静音后仍可在直播中重新开启输入。
    {
        Oran7AudioEncoder::EncoderConfig audio;
        audio.codecName = avcodec_find_encoder_by_name("libfdk_aac") ? "libfdk_aac" : "aac";
        audio.sampleRate = 48000;
        audio.channels = 2;
        audio.bitrateBps = 160000;
        audio.continuousPts = true;
        audio.maxFrameQueue = 64;
        audio.maxPacketQueue = 100; // 约 2 秒 AAC，吸收连接初期短暂网络写入抖动。
        r.audio = std::make_shared<Oran7AudioEncoder>();
        if (!r.audio->configure(audio, onError) || !r.audio->start() ||
            !wait([&] { return r.audio->isOpened(); }, [&] { return r.audio->isRunning(); }))
            return cancel.load() ? QString() : QStringLiteral("直播音频编码器准备失败");
    }
    if (cancel.load())
        return {};
    // Publisher 仅同步运行于本次任务内，借用任务取消标记的生命周期。
    auto cancellation = std::shared_ptr<std::atomic_bool>(const_cast<std::atomic_bool*>(&cancel), [](auto*) {});
    r.publisher = std::make_unique<Oran7RtmpPublisher>(cancellation);
    auto params = [](const AVCodecContext* ctx)
    {
        auto* p = avcodec_parameters_alloc();
        if (p && (!ctx || avcodec_parameters_from_context(p, ctx) < 0))
            avcodec_parameters_free(&p);
        return std::unique_ptr<AVCodecParameters, void (*)(AVCodecParameters*)>(p, [](auto* x)
                                                                                { avcodec_parameters_free(&x); });
    };
    auto vp = params(r.video->codecContext());
    auto ap = params(r.audio ? r.audio->codecContext() : nullptr);
    if (!vp || (r.audio && !ap))
        return QStringLiteral("直播编码参数不可用");
    if (!r.publisher->open(c.server, c.streamKey, vp.get(), r.video->encoderTimeBase(), ap.get(),
                           r.audio ? r.audio->encoderTimeBase() : AVRational{1, 48000}))
        return cancel.load() ? QString() : r.publisher->error();
    if (cancel.load())
        return {};
    if (!audioControl)
    {
        audioControl = std::make_shared<Oran7LiveAudioControl>();
        audioControl->set({c.audioEnabled, c.microphoneEnabled, c.systemVolume, c.microphoneVolume});
    }
    if (audioFeed)
    {
        r.audioSubscription = audioFeed->attach(r.audio);
        if (!r.audioSubscription)
            return QStringLiteral("音频输出接入失败");
    }
    else if (!r.audioMix.start(r.audio, audioControl))
        return QStringLiteral("直播系统声音采集或混音失败");
    r.videoSubscription=feed->attach(r.video,c.width,c.height,c.fps);
    if (!r.videoSubscription)
        return QStringLiteral("采集源已停止");
    const auto epoch = feed->snapshot().epoch;
    QElapsedTimer elapsed, period;
    elapsed.start();
    period.start();
    quint64 lastBytes = 0, lastFrames = 0;
    qint64 lastAudioDiagnostic = -1000, lastInputDropDiagnostic = 0, lastPacketDropDiagnostic = 0;
    quint64 lastCaptureDrops = 0, lastDiscontinuities = 0;
    bool started = false, videoSent = false, audioSent = false;
    qint64 firstVideoDts = AV_NOPTS_VALUE, firstVideoWall = 0;
    AVPacket* packet = av_packet_alloc();
    if (!packet)
        return QStringLiteral("直播数据包分配失败");
    const auto packetGuard = qScopeGuard([&] { av_packet_free(&packet); });
    while (!cancel.load())
    {
        const auto current = feed->snapshot();
        if (!current.alive || current.epoch != epoch || current.width != source.width ||
            current.height != source.height)
            return QStringLiteral("采集画面发生变化，直播已停止，请重新开始推流");
        if (audioFeed && !audioFeed->snapshot().alive)
            return Oran7AudioCaptureBackend::errorMessage(audioFeed->snapshot().error);
        if (encoderFailed.load())
            return QStringLiteral("直播编码失败");
        if (r.audioMix.error() == Oran7AudioMixPipeline::Error::Microphone)
            return QStringLiteral("麦克风采集失败，请检查默认输入设备和麦克风权限");
        if (r.audioMix.error() != Oran7AudioMixPipeline::Error::None)
            return QStringLiteral("直播系统声音采集或混音失败");
        if (!r.video->isRuning() || (r.audio && !r.audio->isRunning()))
            return QStringLiteral("直播编码或音频设备已停止");
        bool consumed = false;
        for (int lane = 0; lane < 2; ++lane)
        {
            for (int batch = 0; batch < 8 && !cancel.load(); ++batch)
            {
                const bool got = lane == 0 ? r.video->getPacket(packet, 0) : (r.audio && r.audio->getPacket(packet, 0));
                if (!got)
                    break;
                consumed = true;
                // 第一个视频包必须为关键帧，不能从依赖旧 GOP 的帧开始。
                if (lane == 0 && !videoSent && !(packet->flags & AV_PKT_FLAG_KEY))
                {
                    av_packet_unref(packet);
                    r.video->requestKeyframe();
                    continue;
                }
                const auto tb = lane == 0 ? r.video->encoderTimeBase() : r.audio->encoderTimeBase();
                if (lane == 0 && packet->dts != AV_NOPTS_VALUE)
                {
                    const auto milliseconds = av_rescale_q(packet->dts, tb, AVRational{1, 1000});
                    if (firstVideoDts == AV_NOPTS_VALUE)
                    {
                        firstVideoDts = milliseconds;
                        firstVideoWall = elapsed.elapsed();
                    }
                    if (elapsed.elapsed() - firstVideoWall - (milliseconds - firstVideoDts) > 1500)
                    {
                        av_packet_unref(packet);
                        return QStringLiteral("上传积压超过安全范围，推流已停止，请降低码率后重试");
                    }
                }
                if (packet->size > 8 * 1024 * 1024)
                {
                    av_packet_unref(packet);
                    return QStringLiteral("直播编码包超过安全上限");
                }
                const bool ok = r.publisher->write(packet, lane == 0, tb);
                av_packet_unref(packet);
                if (!ok)
                    return cancel.load() ? QString() : r.publisher->error();
                if (lane == 0)
                    videoSent = true;
                else
                    audioSent = true;
            }
        }
        if (!started && videoSent && audioSent)
        {
            started = true;
            cb.started({});
        }
        if (!started && elapsed.elapsed() > 8000)
            return QStringLiteral("直播等待首批音视频发送超时");
        if (period.elapsed() >= 500)
        {
            const auto bytes = r.publisher->payloadBytes(), frames = r.publisher->videoPackets();
            const qint64 inputDrops = r.audio ? r.audio->droppedInputFrames() : 0;
            const qint64 packetDrops = r.audio ? r.audio->droppedOutputPackets() : 0;
            const auto sourceStats = audioFeed ? audioFeed->snapshot() : Oran7AudioCaptureFeed::Snapshot{};
            const auto captureDrops = audioFeed ? sourceStats.captureDrops + sourceStats.microphoneDrops
                                                : r.audioMix.captureDrops() + r.audioMix.microphoneDrops();
            const auto discontinuities = audioFeed ? sourceStats.discontinuities : r.audioMix.discontinuities();
            if (r.audio && elapsed.elapsed() - lastAudioDiagnostic >= 1000 &&
                (elapsed.elapsed() < 10000 || inputDrops != lastInputDropDiagnostic ||
                 packetDrops != lastPacketDropDiagnostic || captureDrops != lastCaptureDrops ||
                 discontinuities != lastDiscontinuities))
            {
                qInfo() << "[RTMPAudio] elapsedMs=" << elapsed.elapsed()
                        << "queuedPackets=" << r.audio->queuedPktCount() << "inputDrops=" << inputDrops
                        << "packetDrops=" << packetDrops << "captureDrops=" << captureDrops
                        << "deviceDiscontinuities=" << discontinuities << "microphoneDrops="
                        << (audioFeed ? sourceStats.microphoneDrops : r.audioMix.microphoneDrops());
                lastAudioDiagnostic = elapsed.elapsed();
                lastInputDropDiagnostic = inputDrops;
                lastPacketDropDiagnostic = packetDrops;
                lastCaptureDrops = captureDrops;
                lastDiscontinuities = discontinuities;
            }
            cb.statistics({{"audioInputDrops", inputDrops},
                           {"audioPacketDrops", packetDrops},
                           {"elapsedMs", elapsed.elapsed()},
                           {"bitrateKbps", double(bytes - lastBytes) * 8 / period.elapsed()},
                           {"fps", double(frames - lastFrames) * 1000 / period.elapsed()},
                           {"framesSent", qulonglong(frames)},
                           {"queuedPackets", r.video->queuedPktCount() + (r.audio ? r.audio->queuedPktCount() : 0)},
                           {"droppedFrames", qint64(r.video->droppedFrameCount())}});
            lastBytes = bytes;
            lastFrames = frames;
            period.restart();
        }
        if (!consumed)
            QThread::msleep(2);
    }
    cb.stopping();
    return {};
}
