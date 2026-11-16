#include "Oran7RtmpPublisher.h"
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QThread>
#include <future>
#include <QProcess>
#include <QTemporaryDir>
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
    check(Oran7RtmpPublisher::available(), "bundled FFmpeg library supports RTMP and FLV");
    check(!Oran7RtmpPublisher::validate("https://example.invalid/live", "fictional").isEmpty(),
          "reject alternate protocol");
    check(!Oran7RtmpPublisher::validate("rtmp://user:secret@example.invalid/live", "fictional").isEmpty(),
          "reject URL userinfo");
    check(!Oran7RtmpPublisher::validate("rtmp://example.invalid/live", "fake\r\nkey").isEmpty(), "reject newline");
    check(Oran7RtmpPublisher::validate("rtmp://example.invalid/live/", "fake?token=FICTIONAL").isEmpty(),
          "accept signed stream key");
    av_log_set_level(AV_LOG_QUIET); // 替身握手失败预期发生，不输出端点路径。
    for (bool cancel : {false, true})
    {
        QTcpServer server;
        check(server.listen(QHostAddress::LocalHost), "local stalled receiver");
        auto flag = std::make_shared<std::atomic_bool>(false);
        auto future = std::async(std::launch::async,
                                 [&]
                                 {
                                     AVCodecParameters* video = avcodec_parameters_alloc();
                                     video->codec_type = AVMEDIA_TYPE_VIDEO;
                                     video->codec_id = AV_CODEC_ID_H264;
                                     video->width = 1280;
                                     video->height = 720;
                                     Oran7RtmpPublisher publisher(flag);
                                     const bool result = publisher.open(
                                         QString("rtmp://127.0.0.1:%1/live").arg(server.serverPort()), "FICTIONAL",
                                         video, {1, 60}, nullptr, {1, 48000}, cancel ? 4000 : 400);
                                     avcodec_parameters_free(&video);
                                     check(!publisher.error().contains("FICTIONAL"), "error never exposes key");
                                     publisher.close();
                                     publisher.close();
                                     return result;
                                 });
        QElapsedTimer timer;
        timer.start();
        bool accepted = false;
        while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready && timer.elapsed() < 6000)
        {
            QCoreApplication::processEvents();
            if (server.hasPendingConnections())
            {
                auto* socket = server.nextPendingConnection();
                socket->setParent(&server);
                accepted = true;
            }
            if (cancel && timer.elapsed() > 150)
                flag->store(true);
            QThread::msleep(2);
        }
        check(timer.elapsed() < 2500, "deadline or cancellation interrupts RTMP handshake promptly");
        check(!future.get(), "stalled handshake cannot report connected");
        check(accepted, "test exercised real local TCP handshake");
    }

    QTemporaryDir directory;
    check(directory.isValid(), "temporary output directory");
    const auto ffmpeg = QStringLiteral(APP_ROOT) + "/ffmpeg6.0/bin/ffmpeg.exe";
    const auto inputPath = directory.filePath("source.flv");
    QProcess generate;
    generate.start(ffmpeg, {"-nostdin",  "-hide_banner",
                            "-loglevel", "error",
                            "-f",        "lavfi",
                            "-i",        "testsrc2=size=320x180:rate=30",
                            "-f",        "lavfi",
                            "-i",        "sine=frequency=440:sample_rate=48000",
                            "-t",        "2",
                            "-c:v",      "libx264",
                            "-preset",   "ultrafast",
                            "-g",        "30",
                            "-c:a",      "aac",
                            "-y",        inputPath});
    check(generate.waitForFinished(15000) && generate.exitCode() == 0, "synthetic H264 AAC input");
    for (bool rootEndpoint : {false, true})
    {
        QTcpServer reserve;
        check(reserve.listen(QHostAddress::LocalHost), "reserve local receiver port");
        const int port = reserve.serverPort();
        reserve.close();
        const auto serverAddress = QString(rootEndpoint ? "rtmp://127.0.0.1:%1" : "rtmp://127.0.0.1:%1/live").arg(port);
        const auto outputPath = directory.filePath("received.flv");
        QProcess receiver;
        receiver.start(ffmpeg, {"-nostdin",
                                "-hide_banner",
                                "-loglevel",
                                "error",
                                "-listen",
                                "1",
                                "-timeout",
                                "5",
                                "-rtmp_app",
                                rootEndpoint ? "" : "live",
                                "-rtmp_playpath",
                                "FICTIONAL",
                                "-i",
                                serverAddress + "/FICTIONAL",
                                "-c",
                                "copy",
                                "-f",
                                "flv",
                                "-y",
                                outputPath});
        check(receiver.waitForStarted(3000), "local RTMP receiver starts");
        QThread::msleep(300);
        AVFormatContext* input = nullptr;
        check(avformat_open_input(&input, inputPath.toUtf8().constData(), nullptr, nullptr) >= 0 &&
                  avformat_find_stream_info(input, nullptr) >= 0,
              "read synthetic compressed streams");
        const int vi = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        const int ai = av_find_best_stream(input, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        check(vi >= 0 && ai >= 0, "synthetic input contains both streams");
        auto stop = std::make_shared<std::atomic_bool>(false);
        Oran7RtmpPublisher publisher(stop);
        check(publisher.open(serverAddress, "FICTIONAL", input->streams[vi]->codecpar, input->streams[vi]->time_base,
                             input->streams[ai]->codecpar, input->streams[ai]->time_base),
              "production publisher completes real RTMP handshake");
        AVPacket* packet = av_packet_alloc();
        check(packet, "packet allocation");
        while (av_read_frame(input, packet) >= 0)
        {
            if (packet->stream_index == vi || packet->stream_index == ai)
            {
                const auto pts = packet->pts, dts = packet->dts;
                const int stream = packet->stream_index;
                check(publisher.write(packet, stream == vi, input->streams[stream]->time_base),
                      "production RTMP packet write");
                check(packet->pts == pts && packet->dts == dts && packet->stream_index == stream,
                      "publisher preserves caller packet");
            }
            av_packet_unref(packet);
        }
        check(publisher.videoPackets() == 60 && publisher.payloadBytes() > 0,
              "payload counters report submitted frames");
        publisher.close();
        av_packet_free(&packet);
        avformat_close_input(&input);
        if (!receiver.waitForFinished(7000))
        {
            receiver.kill();
            receiver.waitForFinished();
            check(false, "local receiver finishes after output closes");
        }
        AVFormatContext* received = nullptr;
        check(avformat_open_input(&received, outputPath.toUtf8().constData(), nullptr, nullptr) >= 0 &&
                  avformat_find_stream_info(received, nullptr) >= 0,
              "real RTMP output is readable FLV");
        const int rv = av_find_best_stream(received, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        const int ra = av_find_best_stream(received, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        check(rv >= 0 && ra >= 0 && received->streams[rv]->codecpar->codec_id == AV_CODEC_ID_H264 &&
                  received->streams[ra]->codecpar->codec_id == AV_CODEC_ID_AAC,
              "receiver retains H264 AAC configuration");
        packet = av_packet_alloc();
        int frames = 0;
        int64_t last = AV_NOPTS_VALUE;
        while (av_read_frame(received, packet) >= 0)
        {
            if (packet->stream_index == rv)
            {
                check(last == AV_NOPTS_VALUE || packet->dts >= last, "received video DTS remains monotonic");
                last = packet->dts;
                ++frames;
            }
            av_packet_unref(packet);
        }
        check(frames == 60, "all 60 synthetic frames arrive through RTMP");
        av_packet_free(&packet);
        avformat_close_input(&received);
    }
    std::puts("PASS: RTMP protocol, validation, local handshake deadline and cancellation");
}
