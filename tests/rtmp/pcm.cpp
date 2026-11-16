#include "Oran7WasapiPcmConverter.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static WAVEFORMATEXTENSIBLE format(int rate, int bits, bool floating)
{
    WAVEFORMATEXTENSIBLE wf{};
    wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.cbSize = sizeof(wf) - sizeof(WAVEFORMATEX);
    wf.Format.nChannels = 2;
    wf.Format.nSamplesPerSec = rate;
    wf.Format.wBitsPerSample = bits;
    wf.Format.nBlockAlign = bits / 8 * 2;
    wf.Format.nAvgBytesPerSec = rate * wf.Format.nBlockAlign;
    wf.Samples.wValidBitsPerSample = bits;
    wf.dwChannelMask = 3;
    wf.SubFormat = {floating ? 3u : 1u, 0, 0x10, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
    return wf;
}

int main()
{
    for (const int rate : {44100, 48000, 96000, 192000})
    {
        for (const int kind : {0, 1, 2, 3, 4})
        {
            const bool floating = kind == 0;
            const int bits = floating || kind == 1 ? 32 : kind == 2 ? 24 : kind == 3 ? 16 : 8;
            auto wf = format(rate, bits, floating);
            Oran7WasapiPcmConverter converter;
            check(converter.configure(&wf.Format, 48000, 2), "configure supported device format");
            check(floating     ? converter.inputFormat() == AV_SAMPLE_FMT_FLT
                  : bits == 32 ? converter.inputFormat() == AV_SAMPLE_FMT_S32
                               : true,
                  "PCM32 must never be interpreted as float32");
            std::vector<int16_t> output;
            const int packet = rate / 100;
            std::vector<uint8_t> data(packet * wf.Format.nBlockAlign);
            for (int block = 0; block < 100; ++block)
            {
                // 两段正弦中间穿插 200ms 静音；模拟设备 SILENT 包没有数据指针。
                const bool silent = block >= 40 && block < 60;
                for (int i = 0; i < packet; ++i)
                {
                    const double value = 0.25 * std::sin(2 * 3.141592653589793 * 1000 * (block * packet + i) / rate);
                    for (int channel = 0; channel < 2; ++channel)
                    {
                        auto* dst = data.data() + (i * 2 + channel) * (bits / 8);
                        if (floating)
                        {
                            const float v = float(value);
                            std::memcpy(dst, &v, 4);
                        }
                        else if (bits == 32)
                        {
                            const int32_t v = int32_t(value * 2147483647);
                            std::memcpy(dst, &v, 4);
                        }
                        else if (bits == 24)
                        {
                            const uint32_t v = uint32_t(int32_t(value * 8388607));
                            for (int k = 0; k < 3; ++k)
                                dst[k] = uint8_t(v >> (8 * k));
                        }
                        else if (bits == 16)
                        {
                            const int16_t v = int16_t(value * 32767);
                            std::memcpy(dst, &v, 2);
                        }
                        else
                            *dst = uint8_t(128 + value * 127);
                    }
                }
                auto* frame = converter.convert(silent ? nullptr : data.data(), packet, silent);
                check(frame && frame->format == AV_SAMPLE_FMT_S16 && frame->sample_rate == 48000,
                      "convert packet to declared output format");
                auto* samples = reinterpret_cast<int16_t*>(frame->data[0]);
                output.insert(output.end(), samples, samples + frame->nb_samples * 2);
                av_frame_free(&frame);
            }
            const auto frames = output.size() / 2;
            check(frames > 47900 && frames <= 48000, "one second keeps sample duration including silence");
            int crossings = 0;
            double energy = 0;
            for (int i = 4800; i < 14400; ++i)
            {
                const int sample = output[i * 2];
                energy += double(sample) * sample;
                if (sample > 0 && output[(i - 1) * 2] <= 0)
                    ++crossings;
                check(std::abs(sample) < 9000 && sample == output[i * 2 + 1],
                      "no clipping, noise bursts, or channel interleave corruption");
            }
            check(crossings >= 199 && crossings <= 201, "1000 Hz pitch unchanged after resampling");
            check(std::sqrt(energy / 9600) > 5500 && std::sqrt(energy / 9600) < 6100, "PCM amplitude preserved");
            for (int i = 22000; i < 26000; ++i)
                check(std::abs(output[i * 2]) <= 2, "silent packets remain silent through resampler");
            check(!converter.convert(nullptr, packet, false), "missing non-silent data rejected");
        }
    }
    // 常见默认麦克风为单声道，必须正确扩展为双声道，不能按 stereo 步长读取。
    {
        auto mono = format(48000, 32, true);
        mono.Format.nChannels = 1;
        mono.Format.nBlockAlign = 4;
        mono.Format.nAvgBytesPerSec = 192000;
        mono.dwChannelMask = 4;
        Oran7WasapiPcmConverter converter;
        check(converter.configure(&mono.Format, 48000, 2), "mono microphone format accepted");
        std::vector<float> data(480, .25f);
        auto* f = converter.convert(reinterpret_cast<const uint8_t*>(data.data()), 480, false);
        check(f && f->nb_samples == 480 && f->ch_layout.nb_channels == 2, "mono mic becomes stereo at unchanged rate");
        auto* pcm = reinterpret_cast<int16_t*>(f->data[0]);
        for (int n = 0; n < 480; ++n)
            check(pcm[n * 2] > 4000 && pcm[n * 2] == pcm[n * 2 + 1], "mono microphone maps to both output channels");
        av_frame_free(&f);
    }
    auto bad = format(48000, 32, false);
    Oran7WasapiPcmConverter converter;
    bad.SubFormat.Data1 = 99;
    check(!converter.configure(&bad.Format, 48000, 2), "unknown SubFormat rejected");
    bad = format(48000, 32, false);
    bad.Format.nBlockAlign = 4;
    check(!converter.configure(&bad.Format, 48000, 2), "invalid block alignment rejected");
    std::puts("PASS: WASAPI PCM8/16/24/32 and float32, 44.1/48/96/192kHz pitch, amplitude, duration, silence");
}
