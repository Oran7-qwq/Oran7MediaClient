#include "Live/Oran7LiveAudioMixer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
static AVFrame* tone(int64_t pts, double hz, double amplitude)
{
    auto* f = av_frame_alloc();
    f->format = AV_SAMPLE_FMT_S16;
    f->sample_rate = 48000;
    f->nb_samples = 480;
    f->pts = pts;
    av_channel_layout_default(&f->ch_layout, 2);
    check(av_frame_get_buffer(f, 0) >= 0, "allocate synthetic input");
    auto* pcm = reinterpret_cast<int16_t*>(f->data[0]);
    for (int i = 0; i < 480; ++i)
        pcm[i * 2] = pcm[i * 2 + 1] = int16_t(amplitude * std::sin(6.283185307179586 * hz * (pts + i) / 48000));
    return f;
}
int main()
{
    Oran7LiveAudioMixer mixer;
    Oran7LiveAudioSettings settings{true, true, 100, 50};
    double system = 0, mic = 0;
    for (int block = 0; block < 100; ++block)
    {
        auto* a = tone(block * 480, 1000, 8000);
        auto* b = tone(block * 480, 500, 8000);
        check(mixer.push(0, a) && mixer.push(1, b), "place both input streams by common timestamp");
        av_frame_free(&a);
        av_frame_free(&b);
        auto* mixed = mixer.render(settings);
        check(mixed && mixed->pts == block * 480 && mixed->nb_samples == 480, "continuous 10ms output timeline");
        auto* pcm = reinterpret_cast<int16_t*>(mixed->data[0]);
        if (block >= 10)
            for (int i = 0; i < 480; ++i)
            {
                const double t = double(mixed->pts + i) / 48000;
                system += pcm[i * 2] * std::sin(6.283185307179586 * 1000 * t);
                mic += pcm[i * 2] * std::sin(6.283185307179586 * 500 * t);
                check(pcm[i * 2] == pcm[i * 2 + 1], "stereo channels remain correctly interleaved");
            }
        av_frame_free(&mixed);
    }
    check(system > 1e8 && mic / system > .49 && mic / system < .51,
          "both pitches survive mix and independent microphone gain");
    settings = {false, true, 100, 100};
    for (int block = 0; block < 25; ++block)
    {
        auto* a = tone(mixer.nextPts(), 1000, 8000);
        auto* b = tone(mixer.nextPts(), 500, 8000);
        mixer.push(0, a);
        mixer.push(1, b);
        av_frame_free(&a);
        av_frame_free(&b);
        auto* mixed = mixer.render(settings);
        if (block == 24)
        {
            double systemOnly = 0, micOnly = 0;
            auto* pcm = reinterpret_cast<int16_t*>(mixed->data[0]);
            for (int i = 0; i < 480; ++i)
            {
                const double t = double(mixed->pts + i) / 48000;
                systemOnly += pcm[i * 2] * std::sin(6.283185307179586 * 1000 * t);
                micOnly += pcm[i * 2] * std::sin(6.283185307179586 * 500 * t);
            }
            check(std::abs(systemOnly) < 1000 && micOnly > 1e6, "system can mute while microphone remains live");
        }
        av_frame_free(&mixed);
    }
    // 源缺失时不重播环形缓冲旧数据，也不改变输出速度。
    for (int i = 0; i < 120; ++i)
    {
        const auto pts = mixer.nextPts();
        auto* f = mixer.render(settings);
        check(f && f->pts == pts, "silent input cannot stall clock");
        auto* pcm = reinterpret_cast<int16_t*>(f->data[0]);
        for (int k = 0; k < 960; ++k)
            check(pcm[k] == 0, "missing or expired input produces silence");
        av_frame_free(&f);
    }
    settings = {true, true, 100, 100};
    for (int i = 0; i < 30; ++i)
    {
        auto* a = tone(mixer.nextPts(), 1000, 30000);
        mixer.push(0, a);
        mixer.push(1, a);
        av_frame_free(&a);
        auto* f = mixer.render(settings);
        auto* pcm = reinterpret_cast<int16_t*>(f->data[0]);
        int peak = 0;
        for (int k = 0; k < 960; ++k)
            peak = std::max(peak, std::abs(int(pcm[k])));
        check(peak > 0 && peak <= 32701, "loud mix is limited without integer wrap or hard clipping");
        av_frame_free(&f);
    }
    auto* invalid = tone(mixer.nextPts(), 1000, 8000);
    invalid->sample_rate = 44100;
    check(!mixer.push(1, invalid), "unconverted input format rejected");
    av_frame_free(&invalid);
    {
        Oran7LiveAudioMixer meterMixer;
        for (int i = 0; i < 30; ++i)
        {
            auto* frame = tone(meterMixer.nextPts(), 1000, 16384);
            meterMixer.push(0, frame);
            meterMixer.push(1, frame);
            av_frame_free(&frame);
            auto* mixed = meterMixer.render({true, true, 100, 50});
            av_frame_free(&mixed);
        }
        check(std::abs(meterMixer.levels()[0].peak - .5f) < .002f &&
                  std::abs(meterMixer.levels()[1].peak - .25f) < .002f,
              "meters measure independent post-gain source peaks before limiting");
        auto* silent = meterMixer.render({true, true, 100, 50});
        av_frame_free(&silent);
        check(meterMixer.levels()[0].peak == 0 && meterMixer.levels()[1].peak == 0,
              "missing sources reset block meters");
        auto* full = tone(meterMixer.nextPts(), 1000, 0);
        auto* samples = reinterpret_cast<int16_t*>(full->data[0]);
        for (int i = 0; i < 960; ++i)
            samples[i] = 32767;
        meterMixer.push(1, full);
        av_frame_free(&full);
        auto* limited = meterMixer.render({true, true, 100, 50});
        av_frame_free(&limited);
        check(meterMixer.levels()[1].fullScale && !meterMixer.levels()[0].fullScale,
              "input full-scale warning remains visible after attenuation");
        Oran7LiveAudioControl mailbox;
        mailbox.publishLevels({Oran7AudioSourceLevel{.9f, true}, Oran7AudioSourceLevel{.4f, false}});
        mailbox.publishLevels({Oran7AudioSourceLevel{.1f, false}, Oran7AudioSourceLevel{.2f, false}});
        auto peak = mailbox.takeLevels();
        check(peak[0].peak > .89f && peak[0].fullScale && peak[1].peak > .39f,
              "display polling preserves brief peaks between GUI ticks");
        peak = mailbox.takeLevels();
        check(peak[0].peak == 0 && !peak[0].fullScale && peak[1].peak == 0,
              "peak mailbox consumes without stale values");
    }
    Oran7LiveAudioControl control;
    std::thread writer(
        [&]
        {
            for (int i = 0; i < 50000; ++i)
                control.set(i % 2 ? Oran7LiveAudioSettings{true, false, 10, 90}
                                  : Oran7LiveAudioSettings{false, true, 90, 10});
        });
    for (int i = 0; i < 50000; ++i)
    {
        auto s = control.get();
        check((s.systemEnabled && !s.microphoneEnabled &&
               ((s.systemVolume == 100 && s.microphoneVolume == 100) ||
                (s.systemVolume == 10 && s.microphoneVolume == 90))) ||
                  (!s.systemEnabled && s.microphoneEnabled && s.systemVolume == 90 && s.microphoneVolume == 10),
              "cross-thread settings snapshot never tears");
    }
    writer.join();
    std::puts("PASS: dual source tones, independent gains/mute, missing input clock, bounded buffers, peak limiter and "
              "atomic controls");
}
