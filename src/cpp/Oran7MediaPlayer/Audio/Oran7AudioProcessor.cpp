#include "Oran7AudioProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr double kPi = 3.14159265358979323846;
const double kBandFrequencies[Oran7AudioParameters::kBandCount] = {
    31.0, 62.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
};
constexpr double kBandQ = 1.41;
constexpr double kGainSmoothingSeconds = 0.020;
constexpr double kLimiterReleaseSeconds = 0.050;
constexpr double kMaxBandGainDb = 12.0;
constexpr double kMaxPreampDb = 24.0;
constexpr double kMinCeilingDb = -24.0;
constexpr double kCrossfeedLowpassHz = 700.0;
constexpr double kCrossfeedDelaySeconds = 0.0003;
const double kCrossfeedAmounts[4] = {0.0, 0.15, 0.25, 0.35};

double clampFinite(double value, double minimum, double maximum, double fallback)
{
    if (!std::isfinite(value)) return fallback;
    return std::max(minimum, std::min(maximum, value));
}

double clampValue(double value, double minimum, double maximum)
{
    return std::max(minimum, std::min(maximum, value));
}
}

bool Oran7AudioProcessor::prepare(int sampleRate, int channels, int maxFrames) noexcept
{
    if (sampleRate <= 0 || channels <= 0 || maxFrames <= 0)
        return false;

    m_sampleRate = sampleRate;
    m_channels = channels;
    m_maxFrames = maxFrames;
    m_prepared = true;
    m_gainSmoothing = 1.0 - std::exp(-1.0 / (kGainSmoothingSeconds * sampleRate));
    m_limiterRelease = 1.0 - std::exp(-1.0 / (kLimiterReleaseSeconds * sampleRate));
    m_crossfeedDelaySamples = static_cast<int>(std::lround(
        kCrossfeedDelaySeconds * sampleRate));
    m_crossfeedDelaySamples = std::max(1, std::min(kCrossfeedMaxDelay, m_crossfeedDelaySamples));
    m_crossfeedLowCoef = std::exp(-2.0 * kPi * kCrossfeedLowpassHz / sampleRate);
    reset();
    return true;
}

void Oran7AudioProcessor::setParameters(const Oran7AudioParameters& parameters) noexcept
{
    m_parameters = parameters;
    if (m_parameters.version != Oran7AudioParameters::kVersion)
        m_parameters.enabled = 0; // 未来版本结构未识别时旁路，不猜测字段。

    m_parameters.preampDb = static_cast<float>(
        clampFinite(parameters.preampDb, -kMaxPreampDb, kMaxPreampDb, 0.0));
    m_parameters.outputGainDb = static_cast<float>(
        clampFinite(parameters.outputGainDb, -kMaxPreampDb, kMaxPreampDb, 0.0));
    m_parameters.limiterCeilingDb = static_cast<float>(
        clampFinite(parameters.limiterCeilingDb, kMinCeilingDb, 0.0, -1.0));
    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
    {
        m_parameters.bandGainsDb[i] = static_cast<float>(
            clampFinite(parameters.bandGainsDb[i], -kMaxBandGainDb, kMaxBandGainDb, 0.0));
    }

    m_parameters.highPassHz = static_cast<float>(
        clampFinite(parameters.highPassHz, 0.0, 1000.0, 20.0));
    m_parameters.lowPassHz = static_cast<float>(
        clampFinite(parameters.lowPassHz, 1000.0, 24000.0, 20000.0));
    if (m_parameters.highPassHz >= m_parameters.lowPassHz)
    {
        m_parameters.highPassHz = 20.0f;
        m_parameters.lowPassHz = 20000.0f;
    }
    m_parameters.bassBoostDb = static_cast<float>(
        clampFinite(parameters.bassBoostDb, 0.0, 12.0, 0.0));
    m_parameters.bassFreqHz = static_cast<float>(
        clampFinite(parameters.bassFreqHz, 40.0, 300.0, 120.0));
    m_parameters.stereoWidth = static_cast<float>(
        clampFinite(parameters.stereoWidth, 0.0, 2.0, 1.0));
    m_parameters.crossfeed = static_cast<std::uint32_t>(
        clampFinite(parameters.crossfeed, 0.0, 3.0, 0.0));
    m_parameters.compressorEnabled = parameters.compressorEnabled ? 1u : 0u;
    m_parameters.compressorThresholdDb = static_cast<float>(
        clampFinite(parameters.compressorThresholdDb, -60.0, 0.0, -18.0));
    m_parameters.compressorRatio = static_cast<float>(
        clampFinite(parameters.compressorRatio, 1.0, 20.0, 3.0));
    m_parameters.compressorAttackMs = static_cast<float>(
        clampFinite(parameters.compressorAttackMs, 0.1, 100.0, 10.0));
    m_parameters.compressorReleaseMs = static_cast<float>(
        clampFinite(parameters.compressorReleaseMs, 10.0, 1000.0, 150.0));
    m_parameters.compressorKneeDb = static_cast<float>(
        clampFinite(parameters.compressorKneeDb, 0.0, 12.0, 3.0));
    m_parameters.compressorMakeupDb = static_cast<float>(
        clampFinite(parameters.compressorMakeupDb, -12.0, 12.0, 0.0));

    m_coefficientsDirty = true;
}

void Oran7AudioProcessor::reset() noexcept
{
    for (auto& band : m_bands)
    {
        for (int channel = 0; channel < kMaxChannels; ++channel)
        {
            band.z1[channel] = 0.0;
            band.z2[channel] = 0.0;
        }
    }
    for (Biquad* filter : {&m_highPass, &m_lowPass, &m_bass})
    {
        for (int channel = 0; channel < kMaxChannels; ++channel)
        {
            filter->z1[channel] = 0.0;
            filter->z2[channel] = 0.0;
        }
    }
    for (int channel = 0; channel < 2; ++channel)
    {
        m_crossfeedLow[channel] = 0.0;
        for (int i = 0; i < kCrossfeedMaxDelay; ++i)
            m_crossfeedDelay[channel][i] = 0.0;
    }
    m_crossfeedIndex = 0;

    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
        m_currentBandGainDb[i] = m_parameters.bandGainsDb[i];
    m_currentHighPassHz = m_parameters.highPassHz;
    m_currentLowPassHz = m_parameters.lowPassHz;
    m_currentBassGainDb = m_parameters.bassBoostDb;
    m_currentWidth = m_parameters.stereoWidth;
    m_currentCrossfeed = kCrossfeedAmounts[std::min<std::uint32_t>(3, m_parameters.crossfeed)];
    m_preampCurrent = linearFromDb(m_parameters.preampDb);
    m_outputCurrent = linearFromDb(m_parameters.outputGainDb);
    m_limiterGain = 1.0;
    m_compressorGain = 1.0;
    m_coefficientsDirty = true;
}

void Oran7AudioProcessor::smoothBandGains(int frames) noexcept
{
    const double alpha = 1.0 - std::exp(
        -static_cast<double>(frames) / (kGainSmoothingSeconds * m_sampleRate));
    bool changed = false;

    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
    {
        const double target = m_parameters.bandGainsDb[i];
        const double delta = target - m_currentBandGainDb[i];
        if (std::fabs(delta) > 1e-6)
        {
            m_currentBandGainDb[i] += delta * alpha;
            changed = true;
        }
        else
        {
            m_currentBandGainDb[i] = target;
        }
    }

    const auto smooth = [&](double& current, double target, double epsilon) {
        const double delta = target - current;
        if (std::fabs(delta) > epsilon)
        {
            current += delta * alpha;
            changed = true;
        }
        else
        {
            current = target;
        }
    };

    smooth(m_currentHighPassHz, m_parameters.highPassHz, 0.01);
    smooth(m_currentLowPassHz, m_parameters.lowPassHz, 0.01);
    smooth(m_currentBassGainDb, m_parameters.bassBoostDb, 1e-4);
    smooth(m_currentWidth, m_parameters.stereoWidth, 1e-4);
    smooth(m_currentCrossfeed,
           kCrossfeedAmounts[std::min<std::uint32_t>(3, m_parameters.crossfeed)], 1e-5);

    if (changed) m_coefficientsDirty = true;
}

void Oran7AudioProcessor::updateCoefficients() noexcept
{
    configureHighPass(m_highPass, m_sampleRate, m_currentHighPassHz);
    configureLowPass(m_lowPass, m_sampleRate, m_currentLowPassHz);
    configureLowShelf(m_bass, m_sampleRate, m_parameters.bassFreqHz, m_currentBassGainDb);

    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
    {
        configurePeaking(m_bands[i], m_sampleRate, kBandFrequencies[i],
                         m_currentBandGainDb[i], kBandQ);
    }

    const double attackSeconds = std::max(0.0001, m_parameters.compressorAttackMs / 1000.0);
    const double releaseSeconds = std::max(0.001, m_parameters.compressorReleaseMs / 1000.0);
    m_compressorAttackCoef = 1.0 - std::exp(-1.0 / (attackSeconds * m_sampleRate));
    m_compressorReleaseCoef = 1.0 - std::exp(-1.0 / (releaseSeconds * m_sampleRate));
}

void Oran7AudioProcessor::process(const float* in, float* out, int frames) noexcept
{
    if (!m_prepared || !in || !out || frames <= 0)
        return;

    if (!enabled() || m_channels > kMaxChannels)
    {
        if (out != in)
            std::memcpy(out, in, static_cast<std::size_t>(frames)
                                 * static_cast<std::size_t>(m_channels)
                                 * sizeof(float));
        return;
    }

    const bool stereo = m_channels == 2;
    const double ceiling = linearFromDb(m_parameters.limiterCeilingDb);
    const double threshold = m_parameters.compressorThresholdDb;
    const double ratio = m_parameters.compressorRatio;
    const double knee = m_parameters.compressorKneeDb;
    const double makeup = m_parameters.compressorMakeupDb;
    const bool compressor = m_parameters.compressorEnabled != 0;

    int offset = 0;
    while (offset < frames)
    {
        const int blockFrames = std::min(kControlBlock, frames - offset);
        smoothBandGains(blockFrames);
        if (m_coefficientsDirty)
        {
            updateCoefficients();
            m_coefficientsDirty = false;
        }

        const double preampTarget = linearFromDb(m_parameters.preampDb);
        const double outputTarget = linearFromDb(m_parameters.outputGainDb);

        for (int frame = 0; frame < blockFrames; ++frame)
        {
            const int base = (offset + frame) * m_channels;
            m_preampCurrent += (preampTarget - m_preampCurrent) * m_gainSmoothing;
            m_outputCurrent += (outputTarget - m_outputCurrent) * m_gainSmoothing;

            double frameSamples[kMaxChannels] = {};

            for (int channel = 0; channel < m_channels; ++channel)
            {
                double sample = static_cast<double>(in[base + channel]) * m_preampCurrent;

                const double highPassed = m_highPass.b0 * sample + m_highPass.z1[channel];
                m_highPass.z1[channel] = m_highPass.b1 * sample - m_highPass.a1 * highPassed
                                       + m_highPass.z2[channel];
                m_highPass.z2[channel] = m_highPass.b2 * sample - m_highPass.a2 * highPassed;
                sample = highPassed;

                const double lowPassed = m_lowPass.b0 * sample + m_lowPass.z1[channel];
                m_lowPass.z1[channel] = m_lowPass.b1 * sample - m_lowPass.a1 * lowPassed
                                       + m_lowPass.z2[channel];
                m_lowPass.z2[channel] = m_lowPass.b2 * sample - m_lowPass.a2 * lowPassed;
                sample = lowPassed;

                for (auto& band : m_bands)
                {
                    const double filtered = band.b0 * sample + band.z1[channel];
                    band.z1[channel] = band.b1 * sample - band.a1 * filtered + band.z2[channel];
                    band.z2[channel] = band.b2 * sample - band.a2 * filtered;
                    sample = filtered;
                }

                const double bass = m_bass.b0 * sample + m_bass.z1[channel];
                m_bass.z1[channel] = m_bass.b1 * sample - m_bass.a1 * bass
                                   + m_bass.z2[channel];
                m_bass.z2[channel] = m_bass.b2 * sample - m_bass.a2 * bass;
                sample = bass;

                frameSamples[channel] = sample;
            }

            if (stereo)
            {
                const double mid = (frameSamples[0] + frameSamples[1]) * 0.5;
                const double side = (frameSamples[0] - frameSamples[1]) * 0.5 * m_currentWidth;
                frameSamples[0] = mid + side;
                frameSamples[1] = mid - side;

                if (m_currentCrossfeed > 1e-6)
                {
                    m_crossfeedLow[0] = m_crossfeedLow[0] * m_crossfeedLowCoef
                                      + frameSamples[1] * (1.0 - m_crossfeedLowCoef);
                    m_crossfeedLow[1] = m_crossfeedLow[1] * m_crossfeedLowCoef
                                      + frameSamples[0] * (1.0 - m_crossfeedLowCoef);

                    const double delayedLeft = m_crossfeedDelay[0][m_crossfeedIndex];
                    const double delayedRight = m_crossfeedDelay[1][m_crossfeedIndex];
                    m_crossfeedDelay[0][m_crossfeedIndex] = m_crossfeedLow[0];
                    m_crossfeedDelay[1][m_crossfeedIndex] = m_crossfeedLow[1];
                    m_crossfeedIndex = (m_crossfeedIndex + 1) % m_crossfeedDelaySamples;

                    frameSamples[0] = frameSamples[0] * (1.0 - m_currentCrossfeed)
                                    + delayedLeft * m_currentCrossfeed;
                    frameSamples[1] = frameSamples[1] * (1.0 - m_currentCrossfeed)
                                    + delayedRight * m_currentCrossfeed;
                }
            }

            if (compressor)
            {
                double peak = 0.0;
                for (int channel = 0; channel < m_channels; ++channel)
                    peak = std::max(peak, std::fabs(frameSamples[channel]));

                double desiredGain = 1.0;
                if (peak > 1e-9)
                {
                    const double levelDb = 20.0 * std::log10(peak);
                    const double over = levelDb - threshold;
                    double gainDb = 0.0;
                    if (over >= knee * 0.5)
                        gainDb = (threshold + over / ratio) - levelDb;
                    else if (over > -knee * 0.5 && knee > 1e-6)
                    {
                        const double x = over + knee * 0.5;
                        gainDb = (1.0 / ratio - 1.0) * x * x / (2.0 * knee);
                    }
                    desiredGain = std::pow(10.0, (gainDb + makeup) / 20.0);
                }

                const double coef = desiredGain < m_compressorGain
                    ? m_compressorAttackCoef : m_compressorReleaseCoef;
                m_compressorGain += (desiredGain - m_compressorGain) * coef;
            }
            else
            {
                m_compressorGain = 1.0;
            }

            double peak = 0.0;
            for (int channel = 0; channel < m_channels; ++channel)
            {
                double sample = frameSamples[channel] * m_compressorGain * m_outputCurrent;
                frameSamples[channel] = sample;
                peak = std::max(peak, std::fabs(sample));
            }

            if (peak > 0.0 && peak > ceiling)
                m_limiterGain = std::min(m_limiterGain, ceiling / peak);
            else
                m_limiterGain += (1.0 - m_limiterGain) * m_limiterRelease;

            for (int channel = 0; channel < m_channels; ++channel)
            {
                double sample = frameSamples[channel] * m_limiterGain;
                if (sample > ceiling) sample = ceiling;
                else if (sample < -ceiling) sample = -ceiling;
                out[base + channel] = static_cast<float>(sample);
            }
        }

        offset += blockFrames;
    }
}

void Oran7AudioProcessor::configurePeaking(Biquad& biquad, int sampleRate,
                                           double frequency, double gainDb,
                                           double q) noexcept
{
    if (sampleRate <= 0 || frequency <= 0.0 || frequency >= sampleRate * 0.49
        || q <= 0.0 || std::fabs(gainDb) < 1e-9)
    {
        biquad.b0 = 1.0;
        biquad.b1 = 0.0;
        biquad.b2 = 0.0;
        biquad.a1 = 0.0;
        biquad.a2 = 0.0;
        return;
    }

    const double amplitude = std::pow(10.0, gainDb / 40.0);
    const double omega = 2.0 * kPi * frequency / sampleRate;
    const double alpha = std::sin(omega) / (2.0 * q);
    const double cosine = std::cos(omega);
    const double a0 = 1.0 + alpha / amplitude;

    biquad.b0 = (1.0 + alpha * amplitude) / a0;
    biquad.b1 = (-2.0 * cosine) / a0;
    biquad.b2 = (1.0 - alpha * amplitude) / a0;
    biquad.a1 = (-2.0 * cosine) / a0;
    biquad.a2 = (1.0 - alpha / amplitude) / a0;
}

void Oran7AudioProcessor::configureHighPass(Biquad& biquad, int sampleRate,
                                            double frequency) noexcept
{
    if (sampleRate <= 0 || frequency <= 20.0 || frequency >= sampleRate * 0.49)
    {
        biquad.b0 = 1.0; biquad.b1 = 0.0; biquad.b2 = 0.0;
        biquad.a1 = 0.0; biquad.a2 = 0.0;
        return;
    }

    constexpr double q = 0.7071067811865476;
    const double omega = 2.0 * kPi * frequency / sampleRate;
    const double alpha = std::sin(omega) / (2.0 * q);
    const double cosine = std::cos(omega);
    const double a0 = 1.0 + alpha;

    biquad.b0 = (1.0 + cosine) * 0.5 / a0;
    biquad.b1 = -(1.0 + cosine) / a0;
    biquad.b2 = (1.0 + cosine) * 0.5 / a0;
    biquad.a1 = -2.0 * cosine / a0;
    biquad.a2 = (1.0 - alpha) / a0;
}

void Oran7AudioProcessor::configureLowPass(Biquad& biquad, int sampleRate,
                                           double frequency) noexcept
{
    if (sampleRate <= 0 || frequency >= 20000.0 || frequency >= sampleRate * 0.49)
    {
        biquad.b0 = 1.0; biquad.b1 = 0.0; biquad.b2 = 0.0;
        biquad.a1 = 0.0; biquad.a2 = 0.0;
        return;
    }

    constexpr double q = 0.7071067811865476;
    const double omega = 2.0 * kPi * frequency / sampleRate;
    const double alpha = std::sin(omega) / (2.0 * q);
    const double cosine = std::cos(omega);
    const double a0 = 1.0 + alpha;

    biquad.b0 = (1.0 - cosine) * 0.5 / a0;
    biquad.b1 = (1.0 - cosine) / a0;
    biquad.b2 = (1.0 - cosine) * 0.5 / a0;
    biquad.a1 = -2.0 * cosine / a0;
    biquad.a2 = (1.0 - alpha) / a0;
}

void Oran7AudioProcessor::configureLowShelf(Biquad& biquad, int sampleRate,
                                            double frequency, double gainDb) noexcept
{
    if (sampleRate <= 0 || frequency <= 0.0 || frequency >= sampleRate * 0.49
        || std::fabs(gainDb) < 1e-9)
    {
        biquad.b0 = 1.0; biquad.b1 = 0.0; biquad.b2 = 0.0;
        biquad.a1 = 0.0; biquad.a2 = 0.0;
        return;
    }

    const double amplitude = std::pow(10.0, gainDb / 40.0);
    const double omega = 2.0 * kPi * frequency / sampleRate;
    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    const double beta = std::sqrt(amplitude) * sine * std::sqrt(2.0);
    const double a0 = (amplitude + 1.0) + (amplitude - 1.0) * cosine + beta;

    biquad.b0 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine + beta) / a0;
    biquad.b1 = 2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosine) / a0;
    biquad.b2 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine - beta) / a0;
    biquad.a1 = -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosine) / a0;
    biquad.a2 = ((amplitude + 1.0) + (amplitude - 1.0) * cosine - beta) / a0;
}

double Oran7AudioProcessor::linearFromDb(double db) noexcept
{
    return std::pow(10.0, db / 20.0);
}
