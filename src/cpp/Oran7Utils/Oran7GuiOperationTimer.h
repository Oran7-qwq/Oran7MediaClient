#pragma once
#include <QElapsedTimer>
#include <QDebug>

// 仅在主线程操作明显超时时记录阶段与耗时，不记录曲目地址、凭据或响应内容。
class Oran7GuiOperationTimer
{
  public:
    explicit Oran7GuiOperationTimer(const char* stage) : m_stage(stage) { m_timer.start(); }
    ~Oran7GuiOperationTimer()
    {
        const auto elapsed = m_timer.elapsed();
        if (elapsed >= 150)
            qWarning().nospace() << "[GuiPerf] stage=" << m_stage << " elapsed_ms=" << elapsed;
    }

  private:
    const char* m_stage;
    QElapsedTimer m_timer;
};
