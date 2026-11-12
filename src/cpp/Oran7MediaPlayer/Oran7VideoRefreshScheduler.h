#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>

// 刷新线程独占的一次性计时器，不忙等、不影响进程全局计时精度。
class Oran7VideoRefreshScheduler
{
  public:
    Oran7VideoRefreshScheduler()
    {
        // 兼容工程较低的 _WIN32_WINNT 声明范围，不改变全局 Windows SDK 宏。
        using CreateTimer = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
        const auto createTimer =
            reinterpret_cast<CreateTimer>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreateWaitableTimerExW"));
        if (createTimer)
            m_timer = createTimer(nullptr, nullptr, 0x2 /* HIGH_RESOLUTION */, TIMER_MODIFY_STATE | SYNCHRONIZE);
        if (!m_timer)
            m_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
    ~Oran7VideoRefreshScheduler()
    {
        if (m_timer)
            CloseHandle(m_timer);
    }
    Oran7VideoRefreshScheduler(const Oran7VideoRefreshScheduler&) = delete;
    Oran7VideoRefreshScheduler& operator=(const Oran7VideoRefreshScheduler&) = delete;
    bool wait(double seconds)
    {
        if (seconds <= 0 || !std::isfinite(seconds))
            return true;
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>(std::ceil(std::min(seconds, 0.01) * 10000000.0));
        return m_timer && SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE) &&
               WaitForSingleObject(m_timer, INFINITE) == WAIT_OBJECT_0;
    }
    static double remaining(double now, double deadline)
    {
        return std::isfinite(deadline) ? std::clamp(deadline - now, 0.0, 0.01) : 0.01;
    }

  private:
    HANDLE m_timer = nullptr;
};
