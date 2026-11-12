#include "Oran7AudioVolumeRamp.h"

#include <cstring>
#include <cstddef>

namespace {
constexpr float kRampSeconds = 0.020f; // 约 20ms 到目标
}

void Oran7AudioVolumeRamp::prepare(int sampleRate) noexcept
{
    m_sampleRate = sampleRate;
    m_step = sampleRate > 0
        ? 1.0f / (kRampSeconds * static_cast<float>(sampleRate))
        : 1.0f;
    m_prepared = sampleRate > 0;
}

void Oran7AudioVolumeRamp::reset(float gain) noexcept
{
    m_current = clamp01(gain);
    m_target = m_current;
}

void Oran7AudioVolumeRamp::setTarget(float target) noexcept
{
    m_target = clamp01(target);
    if (!m_prepared)
        m_current = m_target;
}

float Oran7AudioVolumeRamp::nextGain() noexcept
{
    if (m_current < m_target)
        m_current = (m_current + m_step < m_target) ? m_current + m_step : m_target;
    else if (m_current > m_target)
        m_current = (m_current - m_step > m_target) ? m_current - m_step : m_target;
    return m_current;
}

void Oran7AudioVolumeRamp::applyFloat(float* out, const float* in, int samples) noexcept
{
    if (!out || samples <= 0)
        return;

    // 满音量且无斜坡时保持整块拷贝，避免无谓的逐样本乘法。
    if (in && m_current == 1.0f && m_target == 1.0f)
    {
        if (out != in)
            std::memcpy(out, in, static_cast<std::size_t>(samples) * sizeof(float));
        return;
    }

    for (int i = 0; i < samples; ++i)
    {
        const float gain = nextGain();
        out[i] = in ? in[i] * gain : 0.0f;
    }
}

void Oran7AudioVolumeRamp::applyS16(short* out, const short* in, int samples) noexcept
{
    if (!out || samples <= 0)
        return;

    for (int i = 0; i < samples; ++i)
    {
        const float gain = nextGain();
        float value = in ? static_cast<float>(in[i]) * gain : 0.0f;
        if (value > 32767.0f) value = 32767.0f;
        else if (value < -32768.0f) value = -32768.0f;
        out[i] = static_cast<short>(value >= 0.0f ? value + 0.5f : value - 0.5f);
    }
}

float Oran7AudioVolumeRamp::clamp01(float value) noexcept
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}
