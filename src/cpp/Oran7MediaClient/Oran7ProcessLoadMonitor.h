#pragma once

#include "Oran7TaskHandle.h"

#include <QObject>
#include <QTimer>
#include <memory>

class Oran7ProcessLoadSampler;

// 工作台可见时采样本进程。负值表示正在采样或系统计数器不可用。
class Oran7ProcessLoadMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(int cpuPercent READ cpuPercent NOTIFY loadChanged)
    Q_PROPERTY(int gpuPercent READ gpuPercent NOTIFY loadChanged)

public:
    explicit Oran7ProcessLoadMonitor(QObject* parent = nullptr);
    ~Oran7ProcessLoadMonitor() override;

    bool active() const { return bool(m_sampler); }
    void setActive(bool active);
    int cpuPercent() const { return m_cpuPercent; }
    int gpuPercent() const { return m_gpuPercent; }

signals:
    void activeChanged();
    void loadChanged();

private:
    void collect();
    void publish(int cpuPercent, int gpuPercent);

    QTimer m_timer;
    std::shared_ptr<Oran7ProcessLoadSampler> m_sampler;
    Oran7TaskHandle m_task;
    int m_cpuPercent = -1;
    int m_gpuPercent = -1;
};
