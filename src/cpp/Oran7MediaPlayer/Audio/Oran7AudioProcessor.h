#pragma once

#include "Oran7AudioParameters.h"

/**
 * @brief 每播放器独占的音频处理器（A2：EQ/滤波/低音架/宽度/交叉馈送/压缩/限幅）。
 *
 * 线程契约：
 * - prepare() 只在设备协商完成、回调仍停用时调用。
 * - process()/reset()/setParameters() 只在所属音频线程调用。
 * - process 不分配、不加锁、不记日志，接受任意有效帧数并按内部控制块切分。
 */
class Oran7AudioProcessor
{
public:
    static constexpr int kMaxChannels = 8;
    static constexpr int kControlBlock = 32;
    static constexpr int kCrossfeedMaxDelay = 64;

    bool prepare(int sampleRate, int channels, int maxFrames) noexcept;
    void process(const float* in, float* out, int frames) noexcept;
    void reset() noexcept;
    void setParameters(const Oran7AudioParameters& parameters) noexcept;

    bool prepared() const noexcept { return m_prepared; }
    bool enabled() const noexcept { return m_prepared && m_parameters.isEnabled(); }
    int sampleRate() const noexcept { return m_sampleRate; }
    int channels() const noexcept { return m_channels; }
    int maxFrames() const noexcept { return m_maxFrames; }
    int latencyFrames() const noexcept { return 0; }
    const Oran7AudioParameters& parameters() const noexcept { return m_parameters; }

private:
    struct Biquad
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double z1[kMaxChannels] = {};
        double z2[kMaxChannels] = {};
    };

    void smoothBandGains(int frames) noexcept;
    void updateCoefficients() noexcept;
    static void configurePeaking(Biquad& biquad, int sampleRate, double frequency,
                                 double gainDb, double q) noexcept;
    static void configureHighPass(Biquad& biquad, int sampleRate, double frequency) noexcept;
    static void configureLowPass(Biquad& biquad, int sampleRate, double frequency) noexcept;
    static void configureLowShelf(Biquad& biquad, int sampleRate, double frequency,
                                  double gainDb) noexcept;
    static double linearFromDb(double db) noexcept;

    int m_sampleRate = 0;
    int m_channels = 0;
    int m_maxFrames = 0;
    bool m_prepared = false;
    bool m_coefficientsDirty = true;

    Oran7AudioParameters m_parameters{};
    Biquad m_bands[Oran7AudioParameters::kBandCount];
    Biquad m_highPass;
    Biquad m_lowPass;
    Biquad m_bass;
    double m_currentBandGainDb[Oran7AudioParameters::kBandCount] = {};
    double m_currentHighPassHz = 20.0;
    double m_currentLowPassHz = 20000.0;
    double m_currentBassGainDb = 0.0;
    double m_currentWidth = 1.0;
    double m_currentCrossfeed = 0.0;
    double m_preampCurrent = 1.0;
    double m_outputCurrent = 1.0;
    double m_limiterGain = 1.0;
    double m_compressorGain = 1.0;
    double m_compressorAttackCoef = 0.0;
    double m_compressorReleaseCoef = 0.0;
    double m_crossfeedLow[2] = {};
    double m_crossfeedDelay[2][kCrossfeedMaxDelay] = {};
    int m_crossfeedDelaySamples = 1;
    int m_crossfeedIndex = 0;
    double m_crossfeedLowCoef = 0.0;
    double m_gainSmoothing = 0.0;
    double m_limiterRelease = 0.0;
};
