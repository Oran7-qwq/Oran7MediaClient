#include "Oran7AudioProcessor.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

int checks = 0;
std::atomic<int> allocations{0};

void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
    ++checks;
}

bool sameSamples(const float* a, const float* b, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (std::memcmp(&a[i], &b[i], sizeof(float)) != 0)
            return false;
    }
    return true;
}

struct SineResult
{
    double rms = 0.0;
    double peak = 0.0;
};

SineResult runSine(Oran7AudioProcessor& processor, double frequency, double amplitude)
{
    constexpr int kBlockFrames = 256;
    constexpr int kSampleRate = 48000;
    constexpr int kTotalFrames = 48000;
    constexpr double kPi = 3.14159265358979323846;
    float input[kBlockFrames * 2];
    float output[kBlockFrames * 2];

    double sum = 0.0;
    double peak = 0.0;
    int count = 0;
    for (int offset = 0; offset < kTotalFrames; offset += kBlockFrames)
    {
        for (int i = 0; i < kBlockFrames; ++i)
        {
            const double t = static_cast<double>(offset + i) / kSampleRate;
            const float sample = static_cast<float>(amplitude * std::sin(2.0 * kPi * frequency * t));
            input[i * 2] = sample;
            input[i * 2 + 1] = sample;
        }
        processor.process(input, output, kBlockFrames);
        if (offset < kTotalFrames / 2) continue; // 跳过滤波器/平滑建立阶段
        for (int i = 0; i < kBlockFrames * 2; ++i)
        {
            sum += static_cast<double>(output[i]) * output[i];
            peak = std::max(peak, std::fabs(static_cast<double>(output[i])));
            ++count;
        }
    }
    SineResult result;
    result.rms = count > 0 ? std::sqrt(sum / count) : 0.0;
    result.peak = peak;
    return result;
}

void testPrepareValidation()
{
    Oran7AudioProcessor processor;
    check(!processor.prepared(), "new processor not prepared");
    check(!processor.enabled(), "new processor disabled");
    check(!processor.prepare(0, 2, 512), "zero sample rate rejected");
    check(!processor.prepare(48000, 0, 512), "zero channels rejected");
    check(!processor.prepare(48000, 2, 0), "zero frames rejected");
    check(!processor.prepared(), "invalid prepare leaves processor unprepared");

    check(processor.prepare(48000, 2, 512), "prepare valid");
    check(processor.prepared(), "processor prepared");
    check(processor.sampleRate() == 48000, "sample rate stored");
    check(processor.channels() == 2, "channels stored");
    check(processor.maxFrames() == 512, "max frames stored");
    check(processor.latencyFrames() == 0, "A0 adds no latency");
}

void testBypassIsBitExact()
{
    Oran7AudioProcessor processor;
    check(processor.prepare(48000, 2, 512), "prepare bypass");

    constexpr int kFrames = 256;
    constexpr int kSamples = kFrames * 2;
    float input[kSamples];
    float output[kSamples];
    for (int i = 0; i < kSamples; ++i)
        input[i] = std::sin(i * 0.01f) * 0.5f + (i % 7) * 0.001f;
    std::memset(output, 0, sizeof(output));

    const int before = allocations.load();
    processor.process(input, output, kFrames);
    check(allocations.load() == before, "process must not allocate");
    check(sameSamples(input, output, kSamples), "disabled bypass is bit exact");
}

void testEnabledFlat()
{
    Oran7AudioProcessor processor;
    check(processor.prepare(44100, 2, 512), "prepare in-place");

    constexpr int kFrames = 128;
    constexpr int kSamples = kFrames * 2;
    float buffer[kSamples];
    for (int i = 0; i < kSamples; ++i)
        buffer[i] = static_cast<float>(i) / kSamples;

    float reference[kSamples];
    std::memcpy(reference, buffer, sizeof(buffer));
    processor.process(buffer, buffer, kFrames);
    check(sameSamples(buffer, reference, kSamples), "in-place bypass leaves samples unchanged");

    Oran7AudioParameters parameters;
    parameters.revision = 1;
    parameters.enabled = 1;
    parameters.limiterCeilingDb = 0.0f; // 平直路径不触发限幅
    processor.setParameters(parameters);
    check(processor.enabled(), "enabled snapshot reported");
    check(processor.parameters().revision == 1, "snapshot stored");

    std::memset(buffer, 0, sizeof(buffer));
    processor.process(reference, buffer, kFrames);
    check(sameSamples(reference, buffer, kSamples), "enabled flat gain is bit exact");
}

void testEqualizerAndLimiter()
{
    Oran7AudioProcessor bypass;
    check(bypass.prepare(48000, 2, 512), "prepare bypass reference");
    Oran7AudioParameters disabled;
    disabled.enabled = 0;
    bypass.setParameters(disabled);
    const auto reference = runSine(bypass, 1000.0, 0.1);

    Oran7AudioProcessor flat;
    check(flat.prepare(48000, 2, 512), "prepare flat");
    Oran7AudioParameters flatParameters;
    flatParameters.enabled = 1;
    flatParameters.limiterCeilingDb = 0.0f;
    flat.setParameters(flatParameters);
    const auto flatResult = runSine(flat, 1000.0, 0.1);
    check(reference.rms > 0.0, "reference rms non-zero");
    check(std::fabs(flatResult.rms / reference.rms - 1.0) < 0.01,
          "enabled flat response is unity");

    Oran7AudioProcessor boost;
    check(boost.prepare(48000, 2, 512), "prepare boost");
    Oran7AudioParameters boostParameters;
    boostParameters.enabled = 1;
    boostParameters.limiterCeilingDb = 0.0f;
    boostParameters.bandGainsDb[5] = 6.0f; // 1kHz
    boost.setParameters(boostParameters);
    const auto boostResult = runSine(boost, 1000.0, 0.1);
    const double gain = boostResult.rms / reference.rms;
    check(gain > 1.85 && gain < 2.15, "1kHz +6dB band gain");

    Oran7AudioProcessor limited;
    check(limited.prepare(48000, 2, 512), "prepare limiter");
    Oran7AudioParameters limiterParameters;
    limiterParameters.enabled = 1;
    limiterParameters.preampDb = 12.0f;
    limiterParameters.limiterCeilingDb = -6.0f;
    limited.setParameters(limiterParameters);
    const auto limitedResult = runSine(limited, 1000.0, 0.25);
    check(limitedResult.peak <= 0.502, "limiter respects ceiling");
}

void testFiltersAndDynamics()
{
    Oran7AudioProcessor referenceProcessor;
    check(referenceProcessor.prepare(48000, 2, 512), "prepare reference");
    Oran7AudioParameters referenceParameters;
    referenceParameters.enabled = 0;
    referenceProcessor.setParameters(referenceParameters);
    const auto reference = runSine(referenceProcessor, 1000.0, 0.1);

    Oran7AudioProcessor loudReferenceProcessor;
    check(loudReferenceProcessor.prepare(48000, 2, 512), "prepare loud reference");
    loudReferenceProcessor.setParameters(referenceParameters);
    const auto loudReference = runSine(loudReferenceProcessor, 1000.0, 0.5);

    Oran7AudioProcessor highPass;
    check(highPass.prepare(48000, 2, 512), "prepare high pass");
    Oran7AudioParameters highPassParameters;
    highPassParameters.enabled = 1;
    highPassParameters.limiterCeilingDb = 0.0f;
    highPassParameters.highPassHz = 200.0f;
    highPass.setParameters(highPassParameters);
    const auto lowTone = runSine(highPass, 50.0, 0.1);
    const auto highTone = runSine(highPass, 1000.0, 0.1);
    check(lowTone.rms < highTone.rms * 0.5, "high pass attenuates 50Hz");

    Oran7AudioProcessor lowPass;
    check(lowPass.prepare(48000, 2, 512), "prepare low pass");
    Oran7AudioParameters lowPassParameters;
    lowPassParameters.enabled = 1;
    lowPassParameters.limiterCeilingDb = 0.0f;
    lowPassParameters.lowPassHz = 8000.0f;
    lowPass.setParameters(lowPassParameters);
    const auto brightTone = runSine(lowPass, 12000.0, 0.1);
    const auto midTone = runSine(lowPass, 1000.0, 0.1);
    check(brightTone.rms < midTone.rms * 0.5, "low pass attenuates 12kHz");

    Oran7AudioProcessor bass;
    check(bass.prepare(48000, 2, 512), "prepare bass");
    Oran7AudioParameters bassParameters;
    bassParameters.enabled = 1;
    bassParameters.limiterCeilingDb = 0.0f;
    bassParameters.bassBoostDb = 9.0f;
    bassParameters.bassFreqHz = 120.0f;
    bass.setParameters(bassParameters);
    const auto bassTone = runSine(bass, 60.0, 0.1);
    check(bassTone.rms > reference.rms * 1.2, "bass shelf boosts 60Hz");

    Oran7AudioProcessor wide;
    check(wide.prepare(48000, 2, 512), "prepare width");
    Oran7AudioParameters widthParameters;
    widthParameters.enabled = 1;
    widthParameters.limiterCeilingDb = 0.0f;
    widthParameters.stereoWidth = 0.0f;
    wide.setParameters(widthParameters);
    double wideRms = 0.0;
    double widePeak = 0.0;
    for (int block = 0; block < 100; ++block)
    {
        float input[512];
        float output[512];
        for (int i = 0; i < 256; ++i)
        {
            const float sample = static_cast<float>(0.1 * std::sin(2.0 * 3.14159265358979323846
                * 1000.0 * (block * 256 + i) / 48000.0));
            input[i * 2] = sample;
            input[i * 2 + 1] = -sample;
        }
        wide.process(input, output, 256);
        if (block < 50) continue;
        for (int i = 0; i < 512; ++i)
        {
            wideRms += static_cast<double>(output[i]) * output[i];
            widePeak = std::max(widePeak, std::fabs(static_cast<double>(output[i])));
        }
    }
    check(wideRms < 1e-6 && widePeak < 1e-3, "zero width cancels pure side signal");

    Oran7AudioProcessor compressed;
    check(compressed.prepare(48000, 2, 512), "prepare compressor");
    Oran7AudioParameters compressorParameters;
    compressorParameters.enabled = 1;
    compressorParameters.limiterCeilingDb = 0.0f;
    compressorParameters.compressorEnabled = 1;
    compressorParameters.compressorThresholdDb = -20.0f;
    compressorParameters.compressorRatio = 10.0f;
    compressorParameters.compressorAttackMs = 5.0f;
    compressorParameters.compressorReleaseMs = 100.0f;
    compressed.setParameters(compressorParameters);
    const auto compressedTone = runSine(compressed, 1000.0, 0.5);
    check(compressedTone.rms < loudReference.rms * 0.7, "compressor reduces loud input");
}

void testNullAndReset()
{
    Oran7AudioProcessor processor;
    check(processor.prepare(48000, 2, 256), "prepare null/reset");
    float sample[8] = {};
    processor.process(nullptr, sample, 4);
    processor.process(sample, nullptr, 4);
    processor.process(sample, sample, 0);
    processor.process(sample, sample, -1);
    processor.reset();
    check(processor.prepared(), "reset keeps prepared state");
    processor.process(sample, sample, 4);
    check(true, "invalid inputs and reset are safe");
}

void testChannelCount()
{
    Oran7AudioProcessor processor;
    check(processor.prepare(96000, 1, 1024), "prepare mono");
    float input[32];
    float output[32];
    for (int i = 0; i < 32; ++i)
        input[i] = static_cast<float>(i);
    processor.process(input, output, 32);
    check(sameSamples(input, output, 32), "mono bypass copies all frames");
}

} // namespace

void* operator new(std::size_t size)
{
    allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

int main()
{
    testPrepareValidation();
    testBypassIsBitExact();
    testEnabledFlat();
    testEqualizerAndLimiter();
    testFiltersAndDynamics();
    testNullAndReset();
    testChannelCount();

    std::printf("audio processor checks passed: %d\n", checks);
    return 0;
}
