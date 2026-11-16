#include "Oran7RecordingSession.h"
#include "Oran7RecordingFeed.h"
#include "Oran7AudioCaptureFeed.h"
#include "Oran7Mp4Muxer.h"
#include "Live/Oran7RtmpPublisher.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QScopeGuard>
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* text)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    av_log_set_level(AV_LOG_QUIET);
    QTemporaryDir dir;
    check(dir.isValid(), "isolated shutdown output");
    QTcpServer server;
    check(server.listen(QHostAddress::LocalHost), "local stalled RTMP server");
    const auto endpoint = QString("rtmp://127.0.0.1:%1/live").arg(server.serverPort());
    const auto path = dir.filePath("shutdown.mp4");
    auto videoFeed = std::make_shared<Oran7RecordingFeed>();
    auto audioFeed = std::make_shared<Oran7AudioCaptureFeed>();
    videoFeed->publish(96, 64, nullptr);
    const auto origin = Oran7WasapiAudioWorker::clockNow100ns();
    audioFeed->setOrigin(origin);
    std::atomic_bool capture{true}, recordReady{false}, fileClosed{false}, liveClosed{false}, tailOk{false};
    std::atomic_int codecErrors{0};
    std::atomic<double> stopAudioSeconds{0};
    // 模拟持续源的 60ms 等待区。退出期间仍须交付文件停止时刻以前的 PCM。
    std::thread source(
        [&]
        {
            int64_t nextAudio = 0;
            while (capture.load())
            {
                const auto now = Oran7RecordingFeed::clockUs();
                for (const auto& out : videoFeed->outputs())
                {
                    auto* f = av_frame_alloc();
                    f->format = AV_PIX_FMT_NV12;
                    f->width = 96;
                    f->height = 64;
                    check(av_frame_get_buffer(f, 32) >= 0, "synthetic video allocation");
                    for (int y = 0; y < 64; ++y)
                        memset(f->data[0] + y * f->linesize[0], 64, 96);
                    for (int y = 0; y < 32; ++y)
                        memset(f->data[1] + y * f->linesize[1], 128, 96);
                    f->pts = (now - out.startUs) * 90000 / 1000000;
                    f->duration = 1500;
                    videoFeed->deliver(out.id, f);
                }
                const auto elapsed = Oran7WasapiAudioWorker::clockNow100ns() - origin;
                const auto available = av_rescale(elapsed, 48000, 10000000) - 2880;
                while (nextAudio + 480 <= available)
                {
                    auto* f = av_frame_alloc();
                    f->format = AV_SAMPLE_FMT_S16;
                    f->sample_rate = 48000;
                    av_channel_layout_default(&f->ch_layout, 2);
                    f->nb_samples = 480;
                    f->pts = nextAudio;
                    check(av_frame_get_buffer(f, 0) >= 0, "synthetic audio allocation");
                    memset(f->data[0], 0, 1920);
                    nextAudio += 480;
                    audioFeed->submit(f);
                }
                QThread::msleep(16);
            }
            videoFeed->close();
            audioFeed->close();
        });
    Oran7RecordingSession recording, live;
    check(recording.start(
              [&](const auto& cancel, const auto& cb) -> QString
              {
                  auto video = std::make_shared<Oran7VideoEncoder>();
                  auto audio = std::make_shared<Oran7AudioEncoder>();
                  Oran7VideoEncoder::EncoderConfig v;
                  v.codecName = "libx264";
                  v.width = 96;
                  v.height = 64;
                  v.fps = 60;
                  v.preset = "ultrafast";
                  v.softwareInputFormat = AV_PIX_FMT_NV12;
                  Oran7AudioEncoder::EncoderConfig a;
                  a.codecName = "aac";
                  a.continuousPts = true;
                  a.drainInputOnStop = true;
                  a.maxFrameQueue = 64;
                  a.maxPacketQueue = 100;
                  const auto error = [&](const auto&) { ++codecErrors; };
                  check(video->configure(v, error) && video->start() && audio->configure(a, error) && audio->start(),
                        "shutdown codecs start");
                  QElapsedTimer ready;
                  ready.start();
                  while ((!video->isOpened() || !audio->isOpened() || !audioFeed->snapshot().ready) &&
                         ready.elapsed() < 3000)
                      QThread::msleep(2);
                  check(video->isOpened() && audio->isOpened(), "shutdown codecs ready");
                  Oran7Mp4Muxer mux;
                  check(mux.open(path.toUtf8().constData(), 96, 64, {1, 60}, video, audio), "shutdown MP4 opens");
                  const auto vi = videoFeed->attach(video, 96, 64, 60), ai = audioFeed->attach(audio);
                  check(vi && ai, "shutdown file subscribes to persistent sources");
                  const auto attachedAt = Oran7WasapiAudioWorker::clockNow100ns();
                  cb.started({});
                  recordReady = true;
                  while (!cancel.load())
                      QThread::msleep(1);
                  stopAudioSeconds = double(Oran7WasapiAudioWorker::clockNow100ns() - attachedAt) / 10000000.;
                  videoFeed->detach(vi);
                  tailOk = audioFeed->finish(ai);
                  audioFeed->detach(ai);
                  audio->stop();
                  video->stop();
                  mux.close();
                  check(videoFeed->snapshot().alive && audioFeed->snapshot().alive,
                        "source remains alive through actual MP4 trailer");
                  fileClosed = true;
                  // aboutToQuit 已进入，同步 join 期间这些通知不能依赖 GUI 被消费。
                  cb.started({});
                  cb.statistics({{"late", true}});
                  return {};
              }),
          "file session starts");
    check(live.start(
              [&](const auto& cancel, const auto&) -> QString
              {
                  // 同正式直播后端一样，把会话取消转发给 FFmpeg interrupt_callback。
                  auto flag = std::shared_ptr<std::atomic_bool>(const_cast<std::atomic_bool*>(&cancel), [](auto*) {});
                  const auto closed = qScopeGuard([&] { liveClosed = true; });
                  auto* params = avcodec_parameters_alloc();
                  params->codec_type = AVMEDIA_TYPE_VIDEO;
                  params->codec_id = AV_CODEC_ID_H264;
                  params->width = 96;
                  params->height = 64;
                  Oran7RtmpPublisher publisher(flag);
                  const bool opened =
                      publisher.open(endpoint, "FICTIONAL", params, {1, 60}, nullptr, {1, 48000}, 15000);
                  avcodec_parameters_free(&params);
                  check(!opened && cancel.load(), "GUI quit cancels actual stalled RTMP handshake");
                  publisher.close();
                  return {};
              }),
          "network session starts");
    bool accepted = false, quitRequested = false;
    QElapsedTimer runtime;
    runtime.start();
    QTimer timer;
    timer.setInterval(5);
    QObject::connect(&timer, &QTimer::timeout, &app,
                     [&]
                     {
                         if (server.hasPendingConnections())
                         {
                             server.nextPendingConnection()->setParent(&server);
                             accepted = true;
                         }
                         check(runtime.elapsed() < 5000, "quit fixture becomes ready");
                         if (recordReady.load() && accepted && runtime.elapsed() > 450)
                         {
                             quitRequested = true;
                             app.quit();
                         }
                     });
    qint64 shutdownMs = 0;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app,
                     [&]
                     {
                         QElapsedTimer elapsed;
                         elapsed.start();
                         recording.stop();
                         live.stop();
                         live.shutdown();
                         recording.shutdown();
                         check(fileClosed.load() && liveClosed.load() && tailOk.load(),
                               "both workers finish including audio tail before source release");
                         capture = false;
                         source.join();
                         shutdownMs = elapsed.elapsed();
                     });
    timer.start();
    app.exec();
    check(quitRequested && shutdownMs < 2500, "aboutToQuit joins workers without further GUI event processing");
    recording.shutdown();
    live.shutdown();
    check(codecErrors.load() == 0 && videoFeed->outputs().empty() && !audioFeed->snapshot().alive,
          "all subscriptions and codecs released");
    AVFormatContext* input = nullptr;
    check(avformat_open_input(&input, path.toUtf8().constData(), nullptr, nullptr) >= 0 &&
              avformat_find_stream_info(input, nullptr) >= 0,
          "MP4 moov is readable after application quit");
    const int vi = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0),
              ai = av_find_best_stream(input, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    check(vi >= 0 && ai >= 0, "quit retains both media tracks");
    double audioEnd = 0;
    int videos = 0;
    AVPacket p{};
    while (av_read_frame(input, &p) >= 0)
    {
        if (p.stream_index == vi)
            ++videos;
        if (p.stream_index == ai)
            audioEnd = std::max(audioEnd, (p.pts + p.duration) * av_q2d(input->streams[ai]->time_base));
        av_packet_unref(&p);
    }
    avformat_close_input(&input);
    check(videos > 10 && audioEnd >= stopAudioSeconds.load() - .035,
          "quit drains delayed audio through the file stop timestamp");
    std::printf("PASS: real Qt aboutToQuit, stalled RTMP cancellation, MP4/AAC tail and source release (%lldms)\n",
                (long long)shutdownMs);
}
