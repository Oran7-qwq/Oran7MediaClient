#include "Oran7AudioSpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>

Oran7AudioSpectrumAnalyzer::Oran7AudioSpectrumAnalyzer(int windowSize)
    : m_size(windowSize == fastSize ? fastSize : size)
{
    av_tx_init(&m_context, &m_transform, AV_TX_FLOAT_FFT, 0, m_size, nullptr, 0);
    for (int i = 0; i < m_size; ++i)
        m_window[i] = 0.5f - 0.5f * std::cos(6.283185307179586 * i / m_size);
}

Oran7AudioSpectrumAnalyzer::~Oran7AudioSpectrumAnalyzer() { av_tx_uninit(&m_context); }

void Oran7AudioSpectrumAnalyzer::reset() noexcept
{
    m_next = m_count = m_rate = 0;
    m_magnitudes.fill(0);
}

void Oran7AudioSpectrumAnalyzer::append(const Oran7AudioSpectrumTap::Block& block) noexcept
{
    append(block.samples.data(), block.count, block.rate);
}

void Oran7AudioSpectrumAnalyzer::append(const float* samples, int count, int rate) noexcept
{
    if (rate != m_rate) {
        reset();
        m_rate = rate;
    }
    for (int i = 0; i < count; ++i) {
        const float sample = samples[i];
        m_samples[m_next] = std::isfinite(sample) ? sample : 0;
        m_next = (m_next + 1) & (m_size - 1);
    }
    m_count = std::min(m_size, m_count + count);
}

bool Oran7AudioSpectrumAnalyzer::analyze() noexcept
{
    if (!m_context || m_count < m_size) return false;
    for (int i = 0; i < m_size; ++i) m_input[i] = {m_samples[(m_next + i) & (m_size - 1)] * m_window[i], 0};
    m_transform(m_context, m_output.data(), m_input.data(), sizeof(AVComplexFloat));
    for (int i = 1; i <= m_size / 2; ++i)
        m_magnitudes[i] = std::hypot(m_output[i].re, m_output[i].im) * (4.f / m_size);
    return true;
}

float Oran7AudioSpectrumAnalyzer::band(float lowHz, float highHz, float gain) const noexcept
{
    if (!m_rate || lowHz >= m_rate * .5f) return 0;
    const float low = lowHz * m_size / m_rate;
    const float high = std::min(highHz * m_size / m_rate, float(m_size / 2));
    const float middle = std::sqrt(low * high);
    const int first = int(middle), next = std::min(first + 1, m_size / 2);
    float amplitude = m_magnitudes[first] + (middle - first) * (m_magnitudes[next] - m_magnitudes[first]);
    for (int bin = std::max(1, int(std::ceil(low))); bin <= int(high); ++bin)
        amplitude = std::max(amplitude, m_magnitudes[bin]);
    const float db = 20 * std::log10(amplitude * gain + 1e-9f);
    // 收紧低电平的高度，给节拍和强音留出更明显的起伏。
    constexpr float knee = .90f;
    const float height = std::max(0.f, (db + 60) / 60);
    const float level = height <= knee ? height : 1.f - (1.f - knee) * std::exp(-(height - knee) / (1.f - knee));
    return level * (.68f + .32f * level);
}
