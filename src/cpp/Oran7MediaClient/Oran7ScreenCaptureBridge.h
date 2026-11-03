#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <functional>
#include <memory>

class QQuickItem;
class Oran7ScreenCaptureController;

// Client 独占的录屏桥；只在 GUI 线程使用，不负责采集/编码/推流内部实现。
class Oran7ScreenCaptureBridge : public QObject
{
    Q_OBJECT
public:
    struct Oran7RenderHooks {
        std::function<bool(QQuickItem*)> attach;
        std::function<QObject*()> videoItem;
    };
    explicit Oran7ScreenCaptureBridge(Oran7RenderHooks render);
    ~Oran7ScreenCaptureBridge() override;

    // 保留旧 QML 句柄：返回同一个控制器，不移交所有权。
    QObject* controller() const;
    bool attachVideoItem(QQuickItem* host);
    QVariantList monitors() const;
    QVariantMap settings() const;
    QVariantMap applySettings(int outputIndex, int fps, bool drawMouse, bool debugLog);
    bool recordingBusy() const;
    void loadSettings();
    void shutdown();

signals:
    void errorOccurred(const QString& error);
    void captureApplyFinished(bool ok, const QString& message);

private:
    Oran7RenderHooks m_render;
    std::unique_ptr<Oran7ScreenCaptureController> m_controller;
    bool m_shutdown = false;
};
