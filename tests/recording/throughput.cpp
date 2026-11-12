#include "Oran7Mp4Muxer.h"
#include "../../src/cpp/Oran7MediaPlayer/Oran7VideoRefreshScheduler.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void check(bool ok, const char* label)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
}
template <class Predicate> static void until(Predicate ready)
{
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 5000)
        QThread::msleep(1);
    check(ready(), "encoder opens within timeout");
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "isolated temporary directory");
    for (const char* format : {"mp4", "flv"})
        for (bool withAudio : {false, true})
        {
            std::atomic<int> errors{0};
            auto error = [&](const std::string& message)
            {
                ++errors;
                std::fprintf(stderr, "%s\n", message.c_str());
            };
            auto video = std::make_shared<Oran7VideoEncoder>();
            Oran7VideoEncoder::EncoderConfig vc;
            vc.codecName = "libx264";
            vc.width = 160;
            vc.height = 96;
            vc.fps = 240;
            vc.gopSize = 240;
            vc.preset = "ultrafast";
            vc.bitrateKbps = 8000;
            check(video->configure(vc, error) && video->start(), "configure synthetic video encoder");
            until([&] { return video->isOpened(); });
            std::shared_ptr<Oran7AudioEncoder> audio;
            if (withAudio)
            {
                audio = std::make_shared<Oran7AudioEncoder>();
                Oran7AudioEncoder::EncoderConfig ac;
                ac.codecName = "aac";
                check(audio->configure(ac, error) && audio->start(), "configure synthetic audio encoder");
                until([&] { return audio->isOpened(); });
            }
            Oran7Mp4Muxer mux;
            mux.setErrorCallback(error);
            const auto path = directory.filePath(QString(withAudio ? "audio-video." : "video.") + format);
            check(mux.open(path.toUtf8().constData(), 160, 96, {1, 240}, video, audio, format),
                  "open production file muxer");
            Oran7VideoRefreshScheduler scheduler;
            const auto started = std::chrono::steady_clock::now();
            for (int i = 0; i < 480; ++i)
            {
                const auto deadline = started + std::chrono::nanoseconds(int64_t(i) * 1000000000 / 240);
                while (std::chrono::steady_clock::now() < deadline)
                    scheduler.wait(std::chrono::duration<double>(deadline - std::chrono::steady_clock::now()).count());
                AVFrame* frame = av_frame_alloc();
                check(frame != nullptr, "allocate video frame");
                frame->format = AV_PIX_FMT_YUV420P;
                frame->width = 160;
                frame->height = 96;
                frame->pts = i * 375;
                check(av_frame_get_buffer(frame, 32) >= 0, "allocate video storage");
                for (int y = 0; y < 96; ++y)
                    memset(frame->data[0] + y * frame->linesize[0], 32 + i % 160, 160);
                for (int y = 0; y < 48; ++y)
                {
                    memset(frame->data[1] + y * frame->linesize[1], 128, 80);
                    memset(frame->data[2] + y * frame->linesize[2], 128, 80);
                }
                video->pushFrame(frame);
                if (audio)
                {
                    AVFrame* pcm = av_frame_alloc();
                    check(pcm != nullptr, "allocate PCM frame");
                    pcm->format = AV_SAMPLE_FMT_S16;
                    pcm->sample_rate = 48000;
                    pcm->nb_samples = 200;
                    pcm->pts = i * 200;
                    av_channel_layout_default(&pcm->ch_layout, 2);
                    check(av_frame_get_buffer(pcm, 0) >= 0, "allocate PCM storage");
                    memset(pcm->data[0], 0, 800);
                    audio->pushFrame(pcm);
                }
            }
            if (audio)
                audio->stop();
            const auto drops = video->droppedFrameCount();
            video->stop();
            mux.close();
            check(errors == 0, "encoding and muxing succeeds");
            AVFormatContext* input = nullptr;
            check(avformat_open_input(&input, path.toUtf8().constData(), nullptr, nullptr) >= 0, "read actual MP4");
            check(avformat_find_stream_info(input, nullptr) >= 0, "read stream metadata");
            check(QString::fromLatin1(input->iformat->name).split(',').contains(format),
                  "actual container matches selected format");
            const int vi = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            check(vi >= 0 && input->streams[vi]->codecpar->codec_id == AV_CODEC_ID_H264, "H264 stream metadata");
            const auto* decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
            auto* context = avcodec_alloc_context3(decoder);
            check(context && avcodec_parameters_to_context(context, input->streams[vi]->codecpar) >= 0 &&
                      avcodec_open2(context, decoder, nullptr) >= 0,
                  "open real software decoder from container headers");
            auto* decoded = av_frame_alloc();
            check(decoded != nullptr, "allocate decoded frame");
            int decodedFrames = 0;
            auto receive = [&]
            {
                int status;
                while ((status = avcodec_receive_frame(context, decoded)) >= 0)
                {
                    ++decodedFrames;
                    av_frame_unref(decoded);
                }
                check(status == AVERROR(EAGAIN) || status == AVERROR_EOF, "decode complete H264 payload");
            };
            int packets = 0, audioPackets = 0;
            int64_t previous = -1;
            bool ordered = true;
            AVPacket packet{};
            while (av_read_frame(input, &packet) >= 0)
            {
                if (packet.stream_index == vi)
                {
                    ++packets;
                    ordered &= packet.pts > previous;
                    previous = packet.pts;
                    check(avcodec_send_packet(context, &packet) >= 0, "send muxed video packet to decoder");
                    receive();
                }
                else
                    ++audioPackets;
                av_packet_unref(&packet);
            }
            const auto* stream = input->streams[vi];
            const double duration = stream->duration != AV_NOPTS_VALUE ? stream->duration * av_q2d(stream->time_base)
                                                                       : input->duration / double(AV_TIME_BASE);
            check(avcodec_send_packet(context, nullptr) >= 0, "flush real decoder");
            receive();
            av_frame_free(&decoded);
            avcodec_free_context(&context);
            avformat_close_input(&input);
            std::printf("format=%s audio=%d: packets=%d/480 input_drops=%lld audio_packets=%d duration=%.4fs\n", format,
                        withAudio, packets, static_cast<long long>(drops), audioPackets, duration);
            check(ordered && duration > 1.95 && duration < 2.05, "timestamps and real duration retained");
            check(decodedFrames == packets, "every muxed video packet decodes to a full frame");
            // 修复前通过 --baseline 记录吞吐，不将坏结果认定为验收通过。
            if (!app.arguments().contains("--baseline"))
                check(packets == 480 && drops == 0 && (!withAudio || audioPackets >= 90),
                      "240fps output is not blocked waiting for audio");
        }
}
