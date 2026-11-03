#include "Oran7ScreenCaptureBridge.h"
#include "Oran7ScreenCapture.h"
#include <QQmlEngine>
#include <QPointer>
#include <utility>

Oran7ScreenCaptureBridge::Oran7ScreenCaptureBridge(Oran7RenderHooks render)
    : m_render(std::move(render)), m_controller(std::make_unique<Oran7ScreenCaptureController>())
{
    // 无 QObject parent 的 unique_ptr 所有权必须显式告知 QML，避免返回句柄后被 GC 删除。
    QQmlEngine::setObjectOwnership(m_controller.get(), QQmlEngine::CppOwnership);
    connect(m_controller.get(), &Oran7ScreenCaptureController::errorOccurred,
        this, [this](const QString& error) {
            if (!m_shutdown) emit errorOccurred(error);
        }, Qt::QueuedConnection);
    connect(m_controller.get(), &Oran7ScreenCaptureController::captureApplyFinished,
        this, [this](bool ok, const QString& message) {
            if (!m_shutdown) emit captureApplyFinished(ok, message);
        }, Qt::QueuedConnection);
}

Oran7ScreenCaptureBridge::~Oran7ScreenCaptureBridge()
{
    shutdown();
    // Controller 析构会等待工作线程退出，必须早于 Client 的渲染桥销毁。
    m_controller.reset();
}

QObject* Oran7ScreenCaptureBridge::controller() const
{
    return m_controller.get();
}

bool Oran7ScreenCaptureBridge::attachVideoItem(QQuickItem* host)
{
    if (m_shutdown || !host || !m_render.attach || !m_render.videoItem) return false;
    QPointer<Oran7ScreenCaptureBridge> guard(this);
    if (!m_render.attach(host) || !guard || m_shutdown) return false;
    auto* item = m_render.videoItem();
    if (!guard || m_shutdown || !item) return false;
    // 同一渲染项切换视觉宿主不应重建采集；Controller 保留原来的同项去重逻辑。
    m_controller->setVideoItem(item);
    return true;
}

QVariantList Oran7ScreenCaptureBridge::monitors() const
{
    return m_controller->monitors();
}

QVariantMap Oran7ScreenCaptureBridge::settings() const
{
    return m_controller->captureSettings();
}

QVariantMap Oran7ScreenCaptureBridge::applySettings(int outputIndex, int fps, bool drawMouse, bool debugLog)
{
    if (m_shutdown) return {{QStringLiteral("ok"), false}, {QStringLiteral("msg"), QStringLiteral("录屏服务已关闭")}};
    return m_controller->applyCaptureSettings(outputIndex, fps, drawMouse, debugLog);
}

bool Oran7ScreenCaptureBridge::recordingBusy() const
{
    return m_controller->recordingBusy();
}

void Oran7ScreenCaptureBridge::loadSettings()
{
    if (!m_shutdown) m_controller->loadCaptureSettings();
}

void Oran7ScreenCaptureBridge::shutdown()
{
    if (m_shutdown) return;
    m_shutdown = true;
    m_controller->shutdown();
}
