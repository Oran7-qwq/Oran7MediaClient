#include "Oran7AudioParameterQueue.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

int checks = 0;

void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
    ++checks;
}

Oran7AudioParameters makeParameters(std::uint32_t revision)
{
    Oran7AudioParameters p;
    p.version = Oran7AudioParameters::kVersion;
    p.revision = revision;
    p.enabled = revision & 1u;
    p.preampDb = static_cast<float>(revision % 7u) - 3.0f;
    p.outputGainDb = static_cast<float>(revision % 5u) - 2.0f;
    p.limiterCeilingDb = -1.0f - static_cast<float>(revision % 3u);
    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
        p.bandGainsDb[i] = static_cast<float>(revision % 13u) + i * 0.25f;
    return p;
}

void checkSnapshot(const Oran7AudioParameters& p)
{
    const std::uint32_t revision = p.revision;
    check(p.version == Oran7AudioParameters::kVersion, "version preserved");
    check(p.enabled == (revision & 1u), "enabled preserved");
    check(p.preampDb == static_cast<float>(revision % 7u) - 3.0f, "preamp preserved");
    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
    {
        const float expected = static_cast<float>(revision % 13u) + i * 0.25f;
        check(std::fabs(p.bandGainsDb[i] - expected) < 1e-6f, "band gain preserved");
    }
}

void testEmptyAndSingle()
{
    Oran7AudioParameterQueue queue;
    Oran7AudioParameters out;
    check(queue.empty(), "new queue empty");
    check(!queue.popLatest(&out), "pop empty returns false");
    check(!queue.popLatest(nullptr), "pop null returns false");

    const auto value = makeParameters(1);
    check(queue.push(value), "push one");
    check(!queue.empty(), "queue non-empty after push");
    check(queue.size() == 1, "size one");
    check(queue.popLatest(&out), "pop one");
    checkSnapshot(out);
    check(out.revision == 1, "single revision");
    check(queue.empty(), "queue empty after pop");
}

void testLatestWins()
{
    Oran7AudioParameterQueue queue;
    Oran7AudioParameters out;
    for (std::uint32_t revision = 1; revision <= 3; ++revision)
        check(queue.push(makeParameters(revision)), "push latest sequence");

    check(queue.popLatest(&out), "pop latest sequence");
    check(out.revision == 3, "popLatest returns newest complete snapshot");
    check(queue.empty(), "all visible slots consumed");
    check(!queue.popLatest(&out), "no stale snapshot left");
}

void testFullAndRetry()
{
    Oran7AudioParameterQueue queue;
    Oran7AudioParameters out;
    for (std::uint32_t revision = 1; revision <= Oran7AudioParameterQueue::kCapacity; ++revision)
        check(queue.push(makeParameters(revision)), "fill to capacity");

    check(!queue.push(makeParameters(99)), "full queue rejects instead of overwriting");
    check(queue.size() == Oran7AudioParameterQueue::kCapacity, "capacity preserved");

    check(queue.popLatest(&out), "drain full queue");
    check(out.revision == Oran7AudioParameterQueue::kCapacity, "newest of full queue");
    check(queue.push(makeParameters(100)), "push succeeds after drain");
    check(queue.popLatest(&out), "pop after retry");
    check(out.revision == 100, "retry value delivered");
}

void testWrapAround()
{
    Oran7AudioParameterQueue queue;
    Oran7AudioParameters out;
    std::uint32_t newest = 0;
    for (std::uint32_t revision = 1; revision <= 2000; ++revision)
    {
        while (!queue.push(makeParameters(revision)))
        {
            check(queue.popLatest(&out), "wrap: consumer must make room");
            newest = out.revision;
        }
        if ((revision % 3u) == 0)
        {
            check(queue.popLatest(&out), "wrap: periodic pop");
            newest = out.revision;
        }
    }
    if (queue.popLatest(&out))
        newest = out.revision;
    check(newest == 2000, "wrap-around keeps final revision");
}

void testConcurrentProducerConsumer()
{
    constexpr std::uint32_t kTotal = 200000;
    Oran7AudioParameterQueue queue;
    std::atomic<bool> done{false};

    std::thread producer([&queue, &done] {
        for (std::uint32_t revision = 1; revision <= kTotal; ++revision)
        {
            const auto value = makeParameters(revision);
            while (!queue.push(value))
                std::this_thread::yield(); // 模拟 GUI 合并最新值后重试
        }
        done.store(true, std::memory_order_release);
    });

    std::uint32_t last = 0;
    std::uint32_t pops = 0;
    while (!done.load(std::memory_order_acquire) || !queue.empty())
    {
        Oran7AudioParameters out;
        if (!queue.popLatest(&out))
        {
            std::this_thread::yield();
            continue;
        }
        check(out.revision >= last, "consumer never sees an older revision");
        checkSnapshot(out);
        last = out.revision;
        ++pops;
    }
    producer.join();

    check(last == kTotal, "final revision delivered");
    check(pops > 0, "consumer observed at least one snapshot");
    check(queue.empty(), "queue empty after producer joined");
}

} // namespace

int main()
{
    static_assert(std::is_trivially_copyable_v<Oran7AudioParameters>,
                  "parameters must be trivially copyable");
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                  "index atomics must be lock-free");

    testEmptyAndSingle();
    testLatestWins();
    testFullAndRetry();
    testWrapAround();
    testConcurrentProducerConsumer();

    std::printf("audio parameter queue checks passed: %d\n", checks);
    return 0;
}
