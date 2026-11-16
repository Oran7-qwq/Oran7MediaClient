#include "Oran7AudioMixPipeline.h"
#include "Oran7AudioCaptureBackend.h"
#include "Oran7WasapiAudioWorker.h"
#include "Oran7Mp4Muxer.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static std::atomic_int inputs{0}, microphoneOpens{0};
static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
// 只替换真实音频设备：生产公共混音、AAC、视频编码器和 MP4/FLV 封装均实际执行。
bool Oran7WasapiAudioWorker::start(const Oran7WasapiConfig& c)
{
    _cfg = c;
    _running = true;
    ++inputs;
    if (c.microphone)
        ++microphoneOpens;
    _recStartUs = av_rescale(clockNow100ns() - c.clockStart100ns, 48000, 10000000);
    return true;
}
void Oran7WasapiAudioWorker::stop()
{
    if (_running.exchange(false))
        --inputs;
}
bool Oran7WasapiAudioWorker::isRunning() const
{
    return _running.load();
}
bool Oran7WasapiAudioWorker::getPcmFrame(AVFrame** out, int)
{
    if (!_running || av_rescale(clockNow100ns() - _cfg.clockStart100ns, 48000, 10000000) < _recStartUs + 480)
        return false;
    auto* f = av_frame_alloc();
    f->format = AV_SAMPLE_FMT_S16;
    f->sample_rate = 48000;
    f->nb_samples = 480;
    f->pts = _recStartUs;
    av_channel_layout_default(&f->ch_layout, 2);
    check(av_frame_get_buffer(f, 0) >= 0, "allocate synthetic capture");
    auto* pcm = reinterpret_cast<int16_t*>(f->data[0]);
    for (int i = 0; i < 480; ++i)
        pcm[i * 2] = pcm[i * 2 + 1] =
            int16_t(8000 * std::sin(6.283185307179586 * (_cfg.microphone ? 500 : 1000) * (f->pts + i) / 48000.));
    _recStartUs += 480;
    *out = f;
    return true;
}
template <class F> static void until(F f)
{
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < 3000)
        QThread::msleep(2);
    check(f(), "real codec opens within bound");
}
struct Spectrum
{
    double systemSin = 0, systemCos = 0, micSin = 0, micCos = 0, energy = 0;
    int samples = 0;
    void add(double x, int64_t n)
    {
        const double t = 6.283185307179586 * n / 48000.;
        systemSin += x * std::sin(1000 * t);
        systemCos += x * std::cos(1000 * t);
        micSin += x * std::sin(500 * t);
        micCos += x * std::cos(500 * t);
        energy += x * x;
        ++samples;
    }
    double system() const { return 2 * std::hypot(systemSin, systemCos) / samples; }
    double mic() const { return 2 * std::hypot(micSin, micCos) / samples; }
};
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "temporary media directory");
    auto control = std::make_shared<Oran7LiveAudioControl>();
    control->set({true, false, 100, 100});
    auto captureFeed = std::make_shared<Oran7AudioCaptureFeed>();
    std::atomic_bool cancelCapture{false};
    QString captureError;
    Oran7RecordingCallbacks captureCallbacks;
    captureCallbacks.started = [](const auto&) {};
    std::thread sourceThread(
        [&] { captureError = Oran7AudioCaptureBackend::run(captureFeed, control, cancelCapture, captureCallbacks); });
    until([&] { return captureFeed->snapshot().ready; });
    QThread::msleep(100);
    check(control->takeLevels()[0].peak > .20f && inputs.load() == 1,
          "idle capture measures system audio without any encoder attached");
    for (const auto* container : {"mp4", "flv"})
    {
        std::atomic_int errors{0};
        auto error = [&](const std::string&) { ++errors; };
        auto video = std::make_shared<Oran7VideoEncoder>();
        Oran7VideoEncoder::EncoderConfig vc;
        vc.codecName = "libx264";
        vc.width = 96;
        vc.height = 64;
        vc.fps = 30;
        vc.gopSize = 30;
        vc.bitrateKbps = 300;
        check(video->configure(vc, error) && video->start(), "configure local recording video");
        until([&] { return video->isOpened(); });
        auto audio = std::make_shared<Oran7AudioEncoder>();
        Oran7AudioEncoder::EncoderConfig ac;
        ac.codecName = avcodec_find_encoder_by_name("libfdk_aac") ? "libfdk_aac" : "aac";
        ac.continuousPts = true;
        ac.maxFrameQueue = 64;
        ac.maxPacketQueue = 100;
        ac.drainInputOnStop = true;
        check(audio->configure(ac, error) && audio->start(), "configure local mixed AAC");
        until([&] { return audio->isOpened(); });
        Oran7Mp4Muxer mux;
        mux.setErrorCallback(error);
        const auto path = directory.filePath(QString("mixed.%1").arg(container));
        check(mux.open(path.toUtf8().constData(), 96, 64, {1, 30}, video, audio, container),
              "open real recording container");
        control->set({true, false, 100, 100});
        const auto subscription = captureFeed->attach(audio);
        check(subscription != 0, "recording subscribes to existing continuous audio stream");
        check(captureFeed->attach(audio) == 0, "single output subscription enforced");
        captureFeed->detach(subscription + 1); // 旧/错误编号不能撤销当前订阅。
        QElapsedTimer elapsed;
        elapsed.start();
        int frameIndex = 0, stage = -1;
        while (elapsed.elapsed() < 2000)
        {
            const int now = int(elapsed.elapsed());
            const int next = now < 400 ? 0 : now < 800 ? 1 : now < 1200 ? 2 : now < 1500 ? 3 : 4;
            if (stage != next)
            {
                stage = next;
                const std::array<Oran7LiveAudioSettings, 5> choices{{{true, false, 100, 100},
                                                                     {true, true, 50, 75},
                                                                     {false, true, 50, 100},
                                                                     {false, false, 50, 100},
                                                                     {true, true, 100, 50}}};
                control->set(choices[stage]);
            }
            if (frameIndex * 1000 / 30 <= now)
            {
                auto* f = av_frame_alloc();
                f->format = AV_PIX_FMT_YUV420P;
                f->width = 96;
                f->height = 64;
                f->pts = frameIndex * 3000;
                check(av_frame_get_buffer(f, 32) >= 0, "allocate synthetic recording video");
                for (int y = 0; y < 64; ++y)
                    memset(f->data[0] + y * f->linesize[0], 64, 96);
                for (int y = 0; y < 32; ++y)
                    for (int plane = 1; plane < 3; ++plane)
                        memset(f->data[plane] + y * f->linesize[plane], 128, 48);
                video->pushFrame(f);
                ++frameIndex;
            }
            QThread::msleep(2);
        }
        const auto stopMs = elapsed.elapsed();
        check(captureFeed->finish(subscription), "drain shared stream through recording stop timestamp");
        audio->stop();
        video->stop();
        mux.close();
        check(inputs.load() == 2 && captureFeed->snapshot().alive,
              "recording stop leaves continuous capture and meters running");
        check(errors.load() == 0 && audio->droppedInputFrames() == 0 && audio->droppedOutputPackets() == 0,
              "tail drain and recording produce no audio drops");
        AVFormatContext* format = nullptr;
        check(avformat_open_input(&format, path.toUtf8().constData(), nullptr, nullptr) >= 0 &&
                  avformat_find_stream_info(format, nullptr) >= 0,
              "read back mixed MP4/FLV");
        const int stream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        check(stream >= 0, "recording has mixed AAC track");
        auto* decoder = avcodec_alloc_context3(avcodec_find_decoder(AV_CODEC_ID_AAC));
        check(avcodec_parameters_to_context(decoder, format->streams[stream]->codecpar) >= 0 &&
                  avcodec_open2(decoder, avcodec_find_decoder(AV_CODEC_ID_AAC), nullptr) >= 0,
              "decode recording mixed audio");
        decoder->pkt_timebase = format->streams[stream]->time_base;
        std::array<Spectrum, 5> windows{};
        int64_t end = 0, sampleCursor = AV_NOPTS_VALUE;
        AVPacket packet{};
        auto* decoded = av_frame_alloc();
        while (av_read_frame(format, &packet) >= 0)
        {
            if (packet.stream_index == stream)
            {
                check(avcodec_send_packet(decoder, &packet) >= 0, "submit recorded AAC");
                while (avcodec_receive_frame(decoder, decoded) == 0)
                {
                    check(decoded->format == AV_SAMPLE_FMT_FLTP && decoded->ch_layout.nb_channels == 2,
                          "decoded mix stays stereo");
                    const auto packetFirst = av_rescale_q(decoded->pts, format->streams[stream]->time_base, {1, 48000});
                    // FLV 时间基为毫秒，频谱分析按解码样本连续计数，避免毫秒取整破坏正弦相位。
                    if (sampleCursor == AV_NOPTS_VALUE)
                        sampleCursor = packetFirst;
                    const auto first = sampleCursor;
                    sampleCursor += decoded->nb_samples;
                    auto* pcm = reinterpret_cast<float*>(decoded->data[0]);
                    end = std::max(end, packetFirst + decoded->nb_samples);
                    for (int i = 0; i < decoded->nb_samples; ++i)
                    {
                        const auto n = first + i;
                        const double ms = n / 48.;
                        check(std::isfinite(pcm[i]) && std::abs(pcm[i]) < .9, "no overload noise in recording");
                        const int bin = ms >= 160 && ms < 300     ? 0
                                        : ms >= 560 && ms < 700   ? 1
                                        : ms >= 960 && ms < 1100  ? 2
                                        : ms >= 1360 && ms < 1450 ? 3
                                        : ms >= 1860 && ms < 1950 ? 4
                                                                  : -1;
                        if (bin >= 0)
                            windows[bin].add(pcm[i], n);
                    }
                    av_frame_unref(decoded);
                }
            }
            av_packet_unref(&packet);
        }
        for (const auto& w : windows)
            check(w.samples > 2000, "readback covers all dynamic control states");
        check(windows[0].system() > .20 && windows[0].mic() < .005, "initial recording includes system only");
        check(windows[1].system() > .09 && windows[1].system() < .15 && windows[1].mic() > .15 &&
                  windows[1].mic() < .21,
              "independent recording gains survive AAC and container");
        check(windows[2].system() < .005 && windows[2].mic() > .20,
              "system mute preserves microphone in recorded file");
        check(std::sqrt(windows[3].energy / windows[3].samples) < .002,
              "both disabled produce silent recording without stopping track");
        check(windows[4].system() > .20 && windows[4].mic() > .09, "inputs can reopen in same recording");
        check(end / 48. >= stopMs - 35 && end / 48. < stopMs + 100,
              "recording drains 60ms mix tail and queued AAC input");
        av_frame_free(&decoded);
        avcodec_free_context(&decoder);
        avformat_close_input(&format);
    }
    cancelCapture = true;
    sourceThread.join();
    check(captureError.isEmpty() && inputs.load() == 0 && !captureFeed->snapshot().alive,
          "leaving workspace closes persistent capture and releases devices");
    check(microphoneOpens.load() >= 4, "default microphone reopened after dynamic disable in both containers");
    std::puts(
        "PASS: shared pipeline, dynamic recording controls, real AAC MP4/FLV decode, tail duration and input release");
}
