#include "Oran7CaptureOutputRouter.h"
#include "Oran7AudioCaptureFeed.h"
#include "Oran7Mp4Muxer.h"
#include "Live/Oran7RtmpPublisher.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QProcess>
#include <QThread>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <future>

static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
template <class F> static void until(F ready)
{
    QElapsedTimer t;
    t.start();
    while (!ready() && t.elapsed() < 3000)
        QThread::msleep(2);
    check(ready(), "codec ready");
}
static int download(AVFrame* dst, const AVFrame* source, int)
{
    return av_frame_ref(dst, static_cast<AVFrame*>(source->opaque));
}
struct Output
{
    std::shared_ptr<Oran7VideoEncoder> video = std::make_shared<Oran7VideoEncoder>();
    std::shared_ptr<Oran7AudioEncoder> audio = std::make_shared<Oran7AudioEncoder>();
    uint64_t videoId = 0, audioId = 0;
    int width, height, fps;
    std::atomic_int errors{0};
    Output(int w, int h, int f) : width(w), height(h), fps(f)
    {
        Oran7VideoEncoder::EncoderConfig v;
        v.codecName = "libx264";
        v.width = w;
        v.height = h;
        v.fps = f;
        v.gopSize = f;
        v.bitrateKbps = 300;
        v.preset = "ultrafast";
        v.softwareInputFormat = AV_PIX_FMT_NV12;
        check(video->configure(v, [this](const auto&) { ++errors; }) && video->start(),
              "open NV12 software video boundary");
        Oran7AudioEncoder::EncoderConfig a;
        a.codecName = "aac";
        a.continuousPts = true;
        a.maxFrameQueue = 64;
        a.maxPacketQueue = 100;
        a.drainInputOnStop = true;
        check(audio->configure(a, [this](const auto&) { ++errors; }) && audio->start(), "open independent AAC encoder");
        until([&] { return video->isOpened() && audio->isOpened(); });
    }
    void attach(const std::shared_ptr<Oran7RecordingFeed>& v, const std::shared_ptr<Oran7AudioCaptureFeed>& a)
    {
        videoId = v->attach(video, width, height, fps);
        audioId = a->attach(audio);
        check(videoId && audioId, "both media sources permit independent subscription");
    }
};
static void readback(const QString& path, int width, int height, int fps, double minDuration)
{
    AVFormatContext* f = nullptr;
    check(avformat_open_input(&f, path.toUtf8().constData(), nullptr, nullptr) >= 0 &&
              avformat_find_stream_info(f, nullptr) >= 0,
          "read output after independent stop");
    const int vi = av_find_best_stream(f, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0),
              ai = av_find_best_stream(f, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    check(vi >= 0 && ai >= 0, "both output files contain H264 and AAC");
    check(f->streams[vi]->codecpar->width == width && f->streams[vi]->codecpar->height == height,
          "independent scaling survives readback");
    int frames = 0, audioPackets = 0;
    int64_t last = AV_NOPTS_VALUE;
    double first = 0, end = 0;
    AVPacket p{};
    while (av_read_frame(f, &p) >= 0)
    {
        if (p.stream_index == vi)
        {
            check(last == AV_NOPTS_VALUE || p.dts >= last, "independent DTS stays monotonic");
            const double t = p.dts * av_q2d(f->streams[vi]->time_base);
            if (!frames)
                first = t;
            end = t;
            last = p.dts;
            ++frames;
        }
        else if (p.stream_index == ai)
            ++audioPackets;
        av_packet_unref(&p);
    }
    check(end - first >= minDuration && frames >= int(minDuration * fps * .8) && audioPackets > 10,
          "stopping one output does not truncate surviving output");
    avformat_close_input(&f);
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    check(dir.isValid(), "isolated output directory");
    for (bool stopRecordingFirst : {true, false})
    {
        auto videoFeed = std::make_shared<Oran7RecordingFeed>();
        auto audioFeed = std::make_shared<Oran7AudioCaptureFeed>();
        videoFeed->publish(96, 64, nullptr);
        const auto origin = Oran7WasapiAudioWorker::clockNow100ns();
        audioFeed->setOrigin(origin);
        std::atomic_bool capture{true};
        std::thread audioSource(
            [&]
            {
                int64_t next = 0;
                while (capture.load())
                {
                    if (av_rescale(Oran7WasapiAudioWorker::clockNow100ns() - origin, 48000, 10000000) < next + 480)
                    {
                        QThread::msleep(1);
                        continue;
                    }
                    auto* f = av_frame_alloc();
                    f->format = AV_SAMPLE_FMT_S16;
                    f->sample_rate = 48000;
                    f->nb_samples = 480;
                    f->pts = next;
                    av_channel_layout_default(&f->ch_layout, 2);
                    check(av_frame_get_buffer(f, 0) >= 0, "allocate shared PCM block");
                    auto* pcm = reinterpret_cast<int16_t*>(f->data[0]);
                    for (int i = 0; i < 480; ++i)
                        pcm[i * 2] = pcm[i * 2 + 1] =
                            int16_t(6000 * std::sin(6.283185307179586 * 440 * (next + i) / 48000.));
                    next += 480;
                    audioFeed->submit(f);
                }
            });
        until([&] { return audioFeed->snapshot().ready; });
        Output local(96, 64, 60), live(64, 48, 30);
        QTcpServer reserve;
        check(reserve.listen(QHostAddress::LocalHost), "reserve local RTMP endpoint");
        const auto address = QString("rtmp://127.0.0.1:%1/live").arg(reserve.serverPort());
        reserve.close();
        const auto remotePath = dir.filePath(stopRecordingFirst ? "remote-a.flv" : "remote-b.flv");
        QProcess receiver;
        receiver.start(QStringLiteral(APP_ROOT) + "/ffmpeg6.0/bin/ffmpeg.exe", {"-nostdin",
                                                                                "-hide_banner",
                                                                                "-loglevel",
                                                                                "error",
                                                                                "-listen",
                                                                                "1",
                                                                                "-timeout",
                                                                                "5",
                                                                                "-rtmp_app",
                                                                                "live",
                                                                                "-rtmp_playpath",
                                                                                "FICTIONAL",
                                                                                "-i",
                                                                                address + "/FICTIONAL",
                                                                                "-c",
                                                                                "copy",
                                                                                "-f",
                                                                                "flv",
                                                                                "-y",
                                                                                remotePath});
        check(receiver.waitForStarted(3000), "start local RTMP receiver");
        QThread::msleep(250);
        auto cancel = std::make_shared<std::atomic_bool>(false);
        Oran7RtmpPublisher publisher(cancel);
        auto* vp = avcodec_parameters_alloc();
        auto* ap = avcodec_parameters_alloc();
        avcodec_parameters_from_context(vp, live.video->codecContext());
        avcodec_parameters_from_context(ap, live.audio->codecContext());
        check(
            publisher.open(address, "FICTIONAL", vp, live.video->encoderTimeBase(), ap, live.audio->encoderTimeBase()),
            "real RTMP output connects independently");
        avcodec_parameters_free(&vp);
        avcodec_parameters_free(&ap);
        const auto localPath = dir.filePath(stopRecordingFirst ? "local-a.mp4" : "local-b.mp4");
        Oran7Mp4Muxer mux;
        check(mux.open(localPath.toUtf8().constData(), 96, 64, {1, 60}, local.video, local.audio),
              "open local file alongside RTMP");
        std::atomic_bool send{true};
        std::atomic_int sendErrors{0};
        std::thread sender(
            [&]
            {
                while (send.load())
                {
                    bool got = false;
                    for (int lane = 0; lane < 2; ++lane)
                        for (int n = 0; n < 8; ++n)
                        {
                            AVPacket p{};
                            if (!(lane == 0 ? live.video->getPacket(&p, 0) : live.audio->getPacket(&p, 0)))
                                break;
                            got = true;
                            if (!publisher.write(&p, lane == 0,
                                                 lane == 0 ? live.video->encoderTimeBase()
                                                           : live.audio->encoderTimeBase()))
                                ++sendErrors;
                            av_packet_unref(&p);
                        }
                    if (!got)
                        QThread::msleep(1);
                }
            });
        auto* bgra = av_frame_alloc();
        bgra->format = AV_PIX_FMT_BGRA;
        bgra->width = 96;
        bgra->height = 64;
        check(av_frame_get_buffer(bgra, 32) >= 0, "synthetic desktop pixels");
        for (int y = 0; y < 64; ++y)
            memset(bgra->data[0] + y * bgra->linesize[0], 255, 96 * 4);
        AVFrame source{};
        source.format = AV_PIX_FMT_D3D11;
        source.width = 96;
        source.height = 64;
        source.opaque = bgra;
        Oran7CaptureOutputRouter router(nullptr, videoFeed, &download);
        local.attach(videoFeed, audioFeed);
        const auto sourceEpoch = videoFeed->snapshot().epoch;
        QElapsedTimer elapsed;
        elapsed.start();
        bool attached = false, stopped = false;
        std::future<void> closing;
        while (elapsed.elapsed() < 2400)
        {
            if (!attached && elapsed.elapsed() >= 350)
            {
                live.attach(videoFeed, audioFeed);
                attached = true;
                check(videoFeed->snapshot().epoch == sourceEpoch, "second output does not invalidate source epoch");
            }
            if (!stopped && elapsed.elapsed() >= 1500)
            {
                closing = std::async(std::launch::async,
                                     [&]
                                     {
                                         if (stopRecordingFirst)
                                         {
                                             videoFeed->detach(local.videoId);
                                             check(audioFeed->finish(local.audioId), "file tail drains independently");
                                             local.audio->stop();
                                             local.video->stop();
                                             mux.close();
                                         }
                                         else
                                         {
                                             videoFeed->detach(live.videoId);
                                             audioFeed->detach(live.audioId);
                                             live.audio->stop();
                                             live.video->stop();
                                             QThread::msleep(50);
                                             send = false;
                                             sender.join();
                                             publisher.close();
                                         }
                                     });
                stopped = true;
            }
            router.submit(&source, Oran7RecordingFeed::clockUs());
            QThread::msleep(4);
        }
        closing.get();
        check(videoFeed->outputs().size() == 1 && videoFeed->snapshot().epoch == sourceEpoch,
              "stopping one output keeps other subscription and source epoch");
        if (stopRecordingFirst)
        {
            videoFeed->detach(live.videoId);
            audioFeed->detach(live.audioId);
            live.audio->stop();
            live.video->stop();
            QThread::msleep(50);
            send = false;
            sender.join();
            publisher.close();
        }
        else
        {
            videoFeed->detach(local.videoId);
            check(audioFeed->finish(local.audioId), "surviving file tail drains");
            local.audio->stop();
            local.video->stop();
            mux.close();
        }
        capture = false;
        audioSource.join();
        audioFeed->close();
        videoFeed->close();
        av_frame_free(&bgra);
        check(local.errors == 0 && live.errors == 0 && sendErrors == 0, "dual encoding and RTMP sender have no errors");
        check(receiver.waitForFinished(7000), "receiver finalizes after independent RTMP stop");
        readback(localPath, 96, 64, 60, stopRecordingFirst ? 1.2 : 2.1);
        readback(remotePath, 64, 48, 30, stopRecordingFirst ? 1.7 : .8);
    }
    std::puts("PASS: one source, dual scale/FPS, MP4 plus real RTMP, staggered start, both independent stop orders");
}
