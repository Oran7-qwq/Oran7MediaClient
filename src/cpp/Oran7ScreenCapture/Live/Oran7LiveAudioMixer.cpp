#include "Oran7LiveAudioMixer.h"
#include <algorithm>
#include <cmath>
#include <limits>

Oran7LiveAudioMixer::Oran7LiveAudioMixer()
{
    for (auto& input : m_inputs)
    {
        input.pcm.resize(capacity * 2);
        input.stamps.assign(capacity, -1);
    }
}
void Oran7LiveAudioMixer::clear(int source)
{
    if (source >= 0 && source < 2)
        std::fill(m_inputs[source].stamps.begin(), m_inputs[source].stamps.end(), -1);
}
bool Oran7LiveAudioMixer::push(int source, const AVFrame* f)
{
    if (source < 0 || source > 1 || !f || f->format != AV_SAMPLE_FMT_S16 || f->sample_rate != sampleRate ||
        f->ch_layout.nb_channels != 2 || !f->data[0] || f->nb_samples < 0 || f->nb_samples > capacity ||
        f->pts == AV_NOPTS_VALUE || f->pts > std::numeric_limits<int64_t>::max() - f->nb_samples)
        return false;
    const auto* pcm = reinterpret_cast<const int16_t*>(f->data[0]);
    auto& input = m_inputs[source];
    for (int i = 0; i < f->nb_samples; ++i)
    {
        const auto pts = f->pts + i;
        if (pts < m_nextPts || pts >= m_nextPts + capacity)
            continue;
        const auto slot = size_t(pts % capacity);
        input.pcm[slot * 2] = pcm[i * 2];
        input.pcm[slot * 2 + 1] = pcm[i * 2 + 1];
        input.stamps[slot] = pts;
    }
    return true;
}
AVFrame* Oran7LiveAudioMixer::render(Oran7LiveAudioSettings s)
{
    AVFrame* f = av_frame_alloc();
    if (!f)
        return nullptr;
    f->format = AV_SAMPLE_FMT_S16;
    f->sample_rate = sampleRate;
    f->nb_samples = blockSize;
    f->pts = m_nextPts;
    av_channel_layout_default(&f->ch_layout, 2);
    if (av_frame_get_buffer(f, 0) < 0)
    {
        av_frame_free(&f);
        return nullptr;
    }
    const std::array<float, 2> target{s.systemEnabled ? std::clamp(s.systemVolume, 0, 100) / 100.f : 0.f,
                                      s.microphoneEnabled ? std::clamp(s.microphoneVolume, 0, 100) / 100.f : 0.f};
    std::array<float, blockSize * 2> mix{};
    m_levels = {};
    float peak = 0.f;
    for (int i = 0; i < blockSize; ++i)
    {
        const auto pts = m_nextPts + i;
        const auto slot = size_t(pts % capacity);
        for (int lane = 0; lane < 2; ++lane)
        {
            // 约 8ms 平滑增益，避免滑块或静音切换引入阶跃爆音。
            m_gain[lane] += (target[lane] - m_gain[lane]) * 0.0026f;
            if (m_inputs[lane].stamps[slot] != pts)
                continue;
            for (int ch = 0; ch < 2; ++ch)
            {
                const float input = m_inputs[lane].pcm[slot * 2 + ch];
                const float value = input * m_gain[lane];
                mix[i * 2 + ch] += value;
                m_levels[lane].peak = std::max(m_levels[lane].peak, std::abs(value) / 32768.f);
                if (target[lane] > 0.f && std::abs(input) >= 32767.f)
                    m_levels[lane].fullScale = true;
            }
        }
        peak = std::max({peak, std::abs(mix[i * 2]), std::abs(mix[i * 2 + 1])});
    }
    const float safeGain = peak > 32700.f ? 32700.f / peak : 1.f;
    // 每块预判峰值，快速压低、缓慢释放，避免整数溢出及连续硬削波。
    m_limiter = safeGain < m_limiter ? safeGain : std::min(safeGain, m_limiter + 0.008f);
    auto* output = reinterpret_cast<int16_t*>(f->data[0]);
    for (int i = 0; i < blockSize * 2; ++i)
        output[i] = int16_t(std::clamp(std::lround(mix[i] * m_limiter), -32767L, 32767L));
    m_nextPts += blockSize;
    return f;
}
