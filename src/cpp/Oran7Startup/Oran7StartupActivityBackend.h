#pragma once
#include <QColor>
#include <QRect>
#include <memory>

// 启动窗专用的小型原生合成层；不接触 Qt 场景图、播放器或采集设备。
class Oran7StartupActivityBackend
{
  public:
    Oran7StartupActivityBackend();
    ~Oran7StartupActivityBackend();
    bool initialize(void* parentWindow, const QSize& size, const QColor& accent, const QColor& track,
                    bool softwareOnly = false);
    bool setRunning(bool running);
    bool present(const QRect& bounds, bool visible, qreal opacity);

  private:
    struct Oran7Native;
    std::unique_ptr<Oran7Native> m_native;
};
