#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>

// 每个音乐播放器各有一份。SDL 唯一写入，频谱线程唯一消费。
class Oran7AudioSpectrumTap
{
public:
    struct Block {
        std::array<float, 1024> samples;
        int count;
        int rate;
        std::uint32_t epoch;
    };
    static constexpr std::uint32_t capacity = 8;
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    std::uint32_t epoch() const noexcept { return m_epoch.load(std::memory_order_acquire); }
    void invalidate() noexcept { m_epoch.fetch_add(2, std::memory_order_acq_rel); }
    void setActive(bool active) noexcept {
        if (bool(epoch() & 1u) != active) m_epoch.fetch_add(1, std::memory_order_acq_rel);
    }

    template<class Sample>
    void push(const Sample* data, int frames, int channels, int rate, std::uint32_t generation) noexcept {
        static_assert(std::is_same_v<Sample, float> || std::is_same_v<Sample, std::int16_t>);
        if (!(generation & 1u) || generation != epoch() || !data || frames <= 0 || channels <= 0 || rate <= 0) return;
        const float scale = (std::is_same_v<Sample, float> ? 1.f : 1.f / 32768.f) / channels;
        auto write = m_write.load(std::memory_order_relaxed);
        while (frames > 0 && write - m_read.load(std::memory_order_acquire) < capacity) {
            auto& block = m_blocks[write % capacity];
            block.count = frames < int(block.samples.size()) ? frames : int(block.samples.size());
            block.rate = rate;
            block.epoch = generation;
            for (int i = 0; i < block.count; ++i) {
                float sum = 0.f;
                for (int channel = 0; channel < channels; ++channel) sum += *data++;
                block.samples[i] = sum * scale;
            }
            frames -= block.count;
            m_write.store(++write, std::memory_order_release);
        }
    }

    bool pop(Block& block) noexcept {
        const auto read = m_read.load(std::memory_order_relaxed);
        if (read == m_write.load(std::memory_order_acquire)) return false;
        const auto& source = m_blocks[read % capacity];
        block.count = source.count;
        block.rate = source.rate;
        block.epoch = source.epoch;
        for (int i = 0; i < source.count; ++i) block.samples[i] = source.samples[i];
        m_read.store(read + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<Block, capacity> m_blocks;
    alignas(64) std::atomic<std::uint32_t> m_write{0};
    alignas(64) std::atomic<std::uint32_t> m_read{0};
    std::atomic<std::uint32_t> m_epoch{0}; // 最低位表示启用，其他位标记不连续边界。
};
