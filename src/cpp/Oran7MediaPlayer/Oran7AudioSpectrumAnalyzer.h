#pragma once

#include "Oran7AudioSpectrumTap.h"
#include <array>

extern "C" {
#include <libavutil/tx.h>
}

// 频谱线程独占。FFT 与采样窗固定大小，和屏幕上的柱数解耦。
class Oran7AudioSpectrumAnalyzer
{
public:
    static constexpr int size = 2048;
    static constexpr int fastSize = 1024;
    explicit Oran7AudioSpectrumAnalyzer(int windowSize = size);
    ~Oran7AudioSpectrumAnalyzer();
    Oran7AudioSpectrumAnalyzer(const Oran7AudioSpectrumAnalyzer&) = delete;
    Oran7AudioSpectrumAnalyzer& operator=(const Oran7AudioSpectrumAnalyzer&) = delete;
    void reset() noexcept;
    void append(const Oran7AudioSpectrumTap::Block& block) noexcept;
    void append(const float* samples, int count, int rate) noexcept;
    bool analyze() noexcept;
    float band(float lowHz, float highHz, float gain) const noexcept;

private:
    AVTXContext* m_context = nullptr;
    av_tx_fn m_transform = nullptr;
    const int m_size;
    alignas(64) std::array<AVComplexFloat, size> m_input{}, m_output{};
    std::array<float, size> m_samples{}, m_window{};
    std::array<float, size / 2 + 1> m_magnitudes{};
    int m_next = 0;
    int m_count = 0;
    int m_rate = 0;
};
