#include "Oran7ProcessLoadMonitor.h"
#include "Oran7Async.h"

#include <winsock2.h>
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr int Unavailable = -1;
constexpr int SampleIntervalMs = 1000;
constexpr wchar_t GpuEngineCounter[] = L"\\GPU Engine(*)\\Utilization Percentage";

std::uint64_t ticks(const FILETIME& time)
{
    return (std::uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

int percent(double value)
{
    return static_cast<int>(std::lround(std::clamp(value, 0.0, 100.0)));
}

struct Oran7ProcessLoad
{
    int cpu = Unavailable;
    int gpu = Unavailable;
};
}

// 工作线程串行读写采样基线和 PDH 查询，界面只接收整数快照。
class Oran7ProcessLoadSampler
{
public:
    ~Oran7ProcessLoadSampler()
    {
        if (m_query)
            PdhCloseQuery(m_query);
    }

    Oran7ProcessLoad collect()
    {
        const int cpu = collectCpu();
        try {
            return {cpu, collectGpu()};
        } catch (const std::bad_alloc&) {
            return {cpu, Unavailable};
        }
    }

private:
    int collectCpu()
    {
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
            return Unavailable;

        const auto now = std::chrono::steady_clock::now();
        const auto totalTicks = ticks(kernel) + ticks(user);
        const double seconds = std::chrono::duration<double>(now - m_cpuSampleTime).count();
        int usage = Unavailable;
        if (m_cpuSampleTime != std::chrono::steady_clock::time_point{} && seconds > 0
            && totalTicks >= m_cpuTicks && m_processorCount > 0) {
            // 按全机逻辑处理器容量归一化，分子只计入本进程各线程的执行时间。
            const double cpuSeconds = (totalTicks - m_cpuTicks) / 10'000'000.0;
            usage = percent(100.0 * cpuSeconds / (seconds * m_processorCount));
        }
        m_cpuSampleTime = now;
        m_cpuTicks = totalTicks;
        return usage;
    }

    int collectGpu()
    {
        if (!m_query) {
            if (PdhOpenQueryW(nullptr, 0, &m_query) != ERROR_SUCCESS)
                return Unavailable;
            if (PdhAddEnglishCounterW(m_query, GpuEngineCounter, 0, &m_gpuCounter) != ERROR_SUCCESS)
                return Unavailable;
        }
        if (!m_gpuCounter || PdhCollectQueryData(m_query) != ERROR_SUCCESS)
            return Unavailable;

        DWORD bufferBytes = 0;
        DWORD count = 0;
        if (PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &bufferBytes, &count, nullptr) != PDH_MORE_DATA
            || bufferBytes == 0)
            return Unavailable;

        using CounterItem = PDH_FMT_COUNTERVALUE_ITEM_W;
        m_gpuBuffer.resize((std::size_t(bufferBytes) + sizeof(CounterItem) - 1) / sizeof(CounterItem));
        if (PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &bufferBytes, &count, m_gpuBuffer.data())
            != ERROR_SUCCESS)
            return Unavailable;

        int busiestEngine = Unavailable;
        bool processPresent = false;
        bool anyValid = false;
        for (DWORD i = 0; i < count; ++i) {
            const auto& item = m_gpuBuffer[i];
            const std::wstring_view name = item.szName ? item.szName : L"";
            const bool ownProcess = name.substr(0, m_pidPrefix.size()) == m_pidPrefix;
            processPresent |= ownProcess;
            const auto& value = item.FmtValue;
            if ((value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA)
                || !std::isfinite(value.doubleValue) || value.doubleValue < 0)
                continue;

            anyValid = true;
            if (ownProcess)
                busiestEngine = std::max(busiestEngine, percent(value.doubleValue));
        }
        // 引擎可用但本进程没有实例时为闲置。刚出现的实例可能还没有第二个有效样本。
        return anyValid && !processPresent ? 0 : busiestEngine;
    }

    const DWORD m_processorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    const std::wstring m_pidPrefix = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";
    std::chrono::steady_clock::time_point m_cpuSampleTime;
    std::uint64_t m_cpuTicks = 0;
    PDH_HQUERY m_query = nullptr;
    PDH_HCOUNTER m_gpuCounter = nullptr;
    std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> m_gpuBuffer;
};

Oran7ProcessLoadMonitor::Oran7ProcessLoadMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(SampleIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &Oran7ProcessLoadMonitor::collect);
}

Oran7ProcessLoadMonitor::~Oran7ProcessLoadMonitor()
{
    m_task.cancel();
}

void Oran7ProcessLoadMonitor::setActive(bool enabled)
{
    if (enabled == active())
        return;

    if (enabled) {
        m_sampler = std::make_shared<Oran7ProcessLoadSampler>();
        m_timer.start();
        collect();
    } else {
        m_timer.stop();
        m_sampler.reset();
        publish(Unavailable, Unavailable);
    }
    emit activeChanged();
}

void Oran7ProcessLoadMonitor::collect()
{
    if (!m_sampler || m_task.isValid())
        return;

    const auto sampler = m_sampler;
    m_task = Oran7Async::run(this, [sampler] { return sampler->collect(); },
        [this, sampler](const Oran7ProcessLoad& result) {
            m_task = {};
            if (sampler == m_sampler)
                publish(result.cpu, result.gpu);
            else
                collect();
        });
}

void Oran7ProcessLoadMonitor::publish(int cpuPercent, int gpuPercent)
{
    if (cpuPercent == m_cpuPercent && gpuPercent == m_gpuPercent)
        return;
    m_cpuPercent = cpuPercent;
    m_gpuPercent = gpuPercent;
    emit loadChanged();
}
