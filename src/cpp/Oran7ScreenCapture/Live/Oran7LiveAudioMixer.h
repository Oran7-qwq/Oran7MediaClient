#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
extern "C"
{
#include <libavutil/frame.h>
}

struct Oran7LiveAudioSettings
{
    bool systemEnabled = true, microphoneEnabled = false;
    int systemVolume = 100, microphoneVolume = 100;
};

struct Oran7AudioSourceLevel
{
    float peak = 0;
    bool fullScale = false;
};

// GUI 仅交换一个原子快照，音频线程不持有 QObject。
class Oran7LiveAudioControl
{
  public:
    void set(Oran7LiveAudioSettings s)
    {
        m_value.store(uint32_t(s.systemEnabled) | uint32_t(s.microphoneEnabled) << 1 | uint32_t(s.systemVolume) << 8 |
                      uint32_t(s.microphoneVolume) << 16);
    }
    Oran7LiveAudioSettings get() const
    {
        const auto v = m_value.load();
        return {bool(v & 1), bool(v & 2), int((v >> 8) & 255), int((v >> 16) & 255)};
    }
    // 保留 GUI 两次读取之间的最高值，短瞬态不会被 20Hz 显示采样漏掉。
    void publishLevels(const std::array<Oran7AudioSourceLevel, 2>& levels)
    {
        for (int i = 0; i < 2; ++i)
        {
            const float peak = std::isfinite(levels[i].peak) ? std::clamp(levels[i].peak, 0.f, 1.f) : 0.f;
            const uint32_t magnitude = uint32_t(peak * 65535.f);
            auto old = m_levels[i].load();
            uint32_t next;
            do
            {
                next = std::max(old & 65535u, magnitude) | (old & 65536u) | (levels[i].fullScale ? 65536u : 0u);
            } while (!m_levels[i].compare_exchange_weak(old, next));
        }
    }
    std::array<Oran7AudioSourceLevel, 2> takeLevels()
    {
        std::array<Oran7AudioSourceLevel, 2> result{};
        for (int i = 0; i < 2; ++i)
        {
            const auto value = m_levels[i].exchange(0);
            result[i] = {float(value & 65535u) / 65535.f, bool(value & 65536u)};
        }
        return result;
    }

  private:
    std::atomic<uint32_t> m_value{1u | 100u << 8 | 100u << 16};
    std::array<std::atomic<uint32_t>, 2> m_levels{{0u, 0u}};
};

// 两路已归一化的 S16/stereo/48k PCM，按公共 PTS 放入有界环形缓冲。
// 缺帧补静音，输出时钟不依赖任一设备回调或网络写入。
class Oran7LiveAudioMixer
{
  public:
    static constexpr int sampleRate = 48000, blockSize = 480, capacity = 48000;
    Oran7LiveAudioMixer();
    bool push(int source, const AVFrame* frame);
    void clear(int source);
    AVFrame* render(Oran7LiveAudioSettings settings);
    int64_t nextPts() const { return m_nextPts; }
    const std::array<Oran7AudioSourceLevel, 2>& levels() const { return m_levels; }

  private:
    struct Buffer
    {
        std::vector<int16_t> pcm;
        std::vector<int64_t> stamps;
    };
    std::array<Buffer, 2> m_inputs;
    int64_t m_nextPts = 0;
    std::array<float, 2> m_gain{1.f, 0.f};
    float m_limiter = 1.f;
    std::array<Oran7AudioSourceLevel, 2> m_levels{};
};
