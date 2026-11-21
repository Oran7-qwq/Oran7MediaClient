#pragma once

#include "Oran7CaptureControl.h"
#include "Oran7CaptureScene.h"
#include <QObject>
#include <QVariantList>
#include <windows.h>

struct Oran7CaptureWindowTarget
{
    HWND window = nullptr;
    DWORD process = 0;
    DWORD thread = 0;

    bool matches() const;
    QString token() const;
};

QVariantList oran7CaptureWindows();
Oran7CaptureWindowTarget oran7FindCaptureWindow(const QString& token);
QString oran7MatchCaptureWindow(const QVariantMap& identity, const QVariantList& candidates);

// 会话、D3D 设备和帧池在同一个采集线程内创建和关闭。
class Oran7WindowCaptureWorker : public QObject
{
    Q_OBJECT
  public:
    Oran7WindowCaptureWorker(Oran7CaptureWindowTarget target, bool drawMouse, int fps,
                             std::shared_ptr<Oran7CapturePreviewFanout> preview,
                             std::shared_ptr<Oran7CaptureScene> scene,
                             std::shared_ptr<Oran7SceneFrameMailbox> frames);
    std::shared_ptr<Oran7CaptureControl> control() const { return m_control; }
    void start();

  signals:
    void stopped();
    void firstFrameReady();
    void temporarilyUnavailable(const QString& message);
    void errorOccurred(const QString& message);

  private:
    QString capture();
    Oran7CaptureWindowTarget m_target;
    bool m_drawMouse;
    int m_fps;
    std::shared_ptr<Oran7CapturePreviewFanout> m_preview;
    std::shared_ptr<Oran7CaptureScene> m_scene;
    std::shared_ptr<Oran7SceneFrameMailbox> m_frames;
    std::shared_ptr<Oran7CaptureControl> m_control = std::make_shared<Oran7CaptureControl>();
};
