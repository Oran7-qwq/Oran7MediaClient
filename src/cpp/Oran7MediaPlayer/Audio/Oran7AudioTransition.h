#pragma once
#include <atomic>
#include <algorithm>
#include <cmath>

// 生命周期包络独立于用户音量；仅音频线程推进，每声道共享同一采样帧增益。
class Oran7AudioTransition
{
  public:
    void prepare(int sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
        m_position = 0.0f;
        m_command.store(35 << 1, std::memory_order_release);
        m_silent.store(true, std::memory_order_release);
    }
    void setMuted(bool value, int durationMs = 35) noexcept
    {
        m_command.store((std::clamp(durationMs, 1, 500) << 1) | int(value), std::memory_order_release);
    }
    bool silent() const noexcept { return m_silent.load(std::memory_order_acquire); }
    template <class Sample> void apply(Sample* samples, int frames, int channels) noexcept
    {
        if (!samples || frames <= 0 || channels <= 0)
            return;
        const int command = m_command.load(std::memory_order_acquire);
        const bool muted = (command & 1) != 0;
        const float step = 1.0f / std::max(1, m_sampleRate * (command >> 1) / 1000);
        for (int frame = 0; frame < frames; ++frame)
        {
            m_position = muted ? std::max(0.0f, m_position - step) : std::min(1.0f, m_position + step);
            const float gain = m_position * m_position * (3.0f - 2.0f * m_position);
            for (int channel = 0; channel < channels; ++channel)
            {
                const int i = frame * channels + channel;
                const float value = float(samples[i]);
                samples[i] = static_cast<Sample>(gain > 0 && std::isfinite(value) ? value * gain : 0);
            }
        }
        m_silent.store(muted && m_position == 0.0f, std::memory_order_release);
    }

  private:
    std::atomic<int> m_command{35 << 1}; // 方向与时长一次提交，音频回调只读取一个快照。
    std::atomic_bool m_silent{true};
    int m_sampleRate = 48000;
    float m_position = 0.0f;
};
