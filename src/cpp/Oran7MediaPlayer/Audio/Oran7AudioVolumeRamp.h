#pragma once

/**
 * @brief 采样级音量斜坡（纯 C++，无 Qt/FFmpeg 依赖）。
 *
 * 目标音量在约 20ms 内线性逼近，避免拖动音量条时的 zipper 噪声。
 * applyFloat()/applyS16() 不分配、不加锁、不记日志，可由音频线程直接调用。
 */
class Oran7AudioVolumeRamp
{
public:
    void prepare(int sampleRate) noexcept;
    ///< 直接落到指定增益（0…1），用于打开设备/切歌后避免淡入。
    void reset(float gain) noexcept;
    void setTarget(float target) noexcept;

    float current() const noexcept { return m_current; }
    float target() const noexcept { return m_target; }
    bool prepared() const noexcept { return m_prepared; }

    void applyFloat(float* out, const float* in, int samples) noexcept;
    void applyS16(short* out, const short* in, int samples) noexcept;

private:
    float nextGain() noexcept;
    static float clamp01(float value) noexcept;

    int m_sampleRate = 0;
    float m_step = 0.0f;
    float m_current = 1.0f;
    float m_target = 1.0f;
    bool m_prepared = false;
};
