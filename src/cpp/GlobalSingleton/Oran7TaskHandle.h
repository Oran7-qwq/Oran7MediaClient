#pragma once

#include <atomic>
#include <memory>

// 取消是协作式的：尚未开始的工作跳过，已运行的工作结束后不再回调界面。
class Oran7TaskHandle
{
public:
    Oran7TaskHandle() = default;
    explicit Oran7TaskHandle(std::shared_ptr<std::atomic_bool> canceled)
        : m_canceled(std::move(canceled)) {}
    void cancel() const { if (m_canceled) m_canceled->store(true); }
    bool isCanceled() const { return !m_canceled || m_canceled->load(); }
    bool isValid() const { return bool(m_canceled); }
private:
    std::shared_ptr<std::atomic_bool> m_canceled;
};
