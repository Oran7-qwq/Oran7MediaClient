#pragma once
#include <windows.h>

// 仅高帧率采集期间请求短等待精度；取消、失败与退出均成对归还。
class Oran7CaptureTimerPeriod
{
  public:
    using Operation = UINT(WINAPI*)(UINT);
    explicit Oran7CaptureTimerPeriod(bool enabled)
    {
        if (!enabled)
            return;
        m_module = LoadLibraryExW(L"winmm.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!m_module)
            return;
        auto begin = reinterpret_cast<Operation>(GetProcAddress(m_module, "timeBeginPeriod"));
        m_end = reinterpret_cast<Operation>(GetProcAddress(m_module, "timeEndPeriod"));
        m_active = begin && m_end && begin(1) == 0;
    }
    // 隔离检查注入调用边界，不改变系统计时设置。
    Oran7CaptureTimerPeriod(bool enabled, Operation begin, Operation end) : m_end(end)
    {
        m_active = enabled && begin && end && begin(1) == 0;
    }
    ~Oran7CaptureTimerPeriod()
    {
        if (m_active)
            m_end(1);
        if (m_module)
            FreeLibrary(m_module);
    }
    bool active() const { return m_active; }
    Oran7CaptureTimerPeriod(const Oran7CaptureTimerPeriod&) = delete;
    Oran7CaptureTimerPeriod& operator=(const Oran7CaptureTimerPeriod&) = delete;

  private:
    HMODULE m_module = nullptr;
    Operation m_end = nullptr;
    bool m_active = false;
};
