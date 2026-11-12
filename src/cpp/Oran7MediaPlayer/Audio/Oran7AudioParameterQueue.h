#pragma once

#include "Oran7AudioParameters.h"

#include <atomic>
#include <cstdint>

/**
 * @brief 单生产者/单消费者（SPSC）固定容量参数队列。
 *
 * 线程契约：
 * - push() 只允许 GUI 线程调用，且同一队列只有一个写者。
 * - popLatest() 只允许所属播放器的音频线程调用，且只有一个读者。
 * - empty()/size() 仅供调试与测试，不在音频线程关键路径使用。
 *
 * 语义：
 * - 队列满时 push() 返回 false，由 GUI 合并最新值后重试，绝不覆盖未读槽。
 * - popLatest() 一次取走当前可见的全部槽位，只返回最后一个完整快照；
 *   音频线程不需要逐条追赶写者，也不会等待 GUI。
 *
 * 内存序：
 * - 写者：先读 read（acquire）判满，再写槽位，最后发布 write（release）。
 * - 读者：先读 write（acquire）取快照，再读槽位，最后回收 read（release）。
 * - 槽位在 read 推进前不会被写者复用，因此读者读取槽位期间没有并发写。
 */
class Oran7AudioParameterQueue
{
public:
    static constexpr std::uint32_t kCapacity = 8;
    static_assert((kCapacity & (kCapacity - 1)) == 0,
                  "kCapacity 必须是 2 的幂，才能用位掩码取模");
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                  "目标平台必须提供无锁的 32 位原子索引");

    bool push(const Oran7AudioParameters& value) noexcept
    {
        const std::uint32_t write = m_write.load(std::memory_order_relaxed);
        const std::uint32_t read = m_read.load(std::memory_order_acquire);
        if (write - read >= kCapacity)
            return false; // 满：保留未读槽，由调用方合并后重试

        m_slots[write & (kCapacity - 1)] = value;
        m_write.store(write + 1, std::memory_order_release);
        return true;    }

    bool popLatest(Oran7AudioParameters* out) noexcept
    {
        if (!out)
            return false;

        std::uint32_t read = m_read.load(std::memory_order_relaxed);
        const std::uint32_t write = m_write.load(std::memory_order_acquire);
        if (read == write)
            return false; // 空：沿用上一份快照，不阻塞

        Oran7AudioParameters latest{};
        while (read != write)
        {
            latest = m_slots[read & (kCapacity - 1)];
            ++read;
        }
        m_read.store(read, std::memory_order_release);
        *out = latest;
        return true;
    }

    bool empty() const noexcept
    {
        return m_write.load(std::memory_order_acquire)
            == m_read.load(std::memory_order_acquire);
    }

    std::uint32_t size() const noexcept
    {
        const std::uint32_t write = m_write.load(std::memory_order_acquire);
        const std::uint32_t read = m_read.load(std::memory_order_acquire);
        return write - read;
    }

private:
    Oran7AudioParameters m_slots[kCapacity];
    // 写索引与读索引分处不同缓存行，避免假共享。
    alignas(64) std::atomic<std::uint32_t> m_write{0};
    alignas(64) std::atomic<std::uint32_t> m_read{0};
};
