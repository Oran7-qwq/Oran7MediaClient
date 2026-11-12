#include "Oran7HdrWindowOutput.h"
#include <QSGRendererInterface>
#include <QDebug>
#include <QCoreApplication>
#include <QScreen>
#include <private/qrhid3d11_p.h>
#include <windows.h>
#include <vector>

namespace
{
struct Oran7DisplayColor
{
    bool hdr = false;
    float whiteNits = 80;
};
Oran7DisplayColor displayColor(QQuickWindow* window)
{
    Oran7DisplayColor result;
    const auto center = window->geometry().center();
    MONITORINFOEXW monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromPoint({center.x(), center.y()}, MONITOR_DEFAULTTONEAREST), &monitor))
        return result;
    // 显示拓扑变化可能使数组尺寸失效，只做有限重试，不等待驱动/显示状态稳定。
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        UINT32 pathCount = 0, modeCount = 0;
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
            break;
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        const auto status =
            QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
        if (status == ERROR_INSUFFICIENT_BUFFER)
            continue;
        if (status != ERROR_SUCCESS)
            break;
        for (UINT32 i = 0; i < pathCount; ++i)
        {
            const auto& path = paths[i];
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source), path.sourceInfo.adapterId,
                             path.sourceInfo.id};
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
                QString::fromWCharArray(source.viewGdiDeviceName)
                        .compare(QString::fromWCharArray(monitor.szDevice), Qt::CaseInsensitive) != 0)
                continue;
            DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO advanced{};
            advanced.header = {DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO, sizeof(advanced),
                               path.targetInfo.adapterId, path.targetInfo.id};
            if (DisplayConfigGetDeviceInfo(&advanced.header) == ERROR_SUCCESS)
                result.hdr = advanced.advancedColorEnabled && !advanced.advancedColorForceDisabled;
            DISPLAYCONFIG_SDR_WHITE_LEVEL white{};
            white.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL, sizeof(white), path.targetInfo.adapterId,
                            path.targetInfo.id};
            if (DisplayConfigGetDeviceInfo(&white.header) == ERROR_SUCCESS)
                result.whiteNits = qBound(80.0f, float(white.SDRWhiteLevel) * 80.0f / 1000.0f, 1000.0f);
            return result;
        }
        break;
    }
    return result;
}
} // namespace

Oran7HdrWindowOutput::Oran7HdrWindowOutput(QObject* parent) : QObject(parent)
{
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(200);
    connect(&m_refresh, &QTimer::timeout, this, &Oran7HdrWindowOutput::refreshDisplay);
    QCoreApplication::instance()->installNativeEventFilter(this);
}
bool Oran7HdrWindowOutput::nativeEventFilter(const QByteArray&, void* message, qintptr*)
{
    const auto* msg = static_cast<const MSG*>(message);
    if (msg->message == WM_DISPLAYCHANGE || msg->message == WM_SETTINGCHANGE)
        m_refresh.start();
    return false;
}
void Oran7HdrWindowOutput::refreshDisplay()
{
    if (!m_window)
        return;
    const auto color = displayColor(m_window);
    m_window->setProperty("_oran7_hdr_display", color.hdr);
    m_window->setProperty("_oran7_sdr_white_nits", color.whiteNits);
    m_reconfigure = true;
    m_window->update();
}

bool Oran7HdrWindowOutput::isActive(QQuickWindow* window)
{
    // 应用固定 basic 渲染循环，所有访问均在 GUI/场景图共用线程。
    return window && window->property("_oran7_hdr_composite").toBool();
}
Oran7HdrWindowOutput::~Oran7HdrWindowOutput()
{
    QCoreApplication::instance()->removeNativeEventFilter(this);
    for (const auto& connection : m_connections)
        disconnect(connection);
    // sceneGraphInvalidated 已在场景图线程释放；basic 循环析构也是同一线程。
    m_compositor.release();
}
void Oran7HdrWindowOutput::setWindow(QQuickWindow* window)
{
    if (m_window == window)
        return;
    for (const auto& connection : m_connections)
        disconnect(connection);
    m_connections.clear();
    m_compositor.release();
    if (m_window)
        m_window->setProperty("_oran7_hdr_composite", false);
    m_window = window;
    m_logged = false;
    m_disableHdr = false;
    if (window)
    {
        // 此 Qt 6.7.3 内部属性在首次创建交换链时读取，不使用影响所有窗口的 QSG_RHI_HDR。
        window->setProperty("_qt_sg_hdr_format", QByteArrayLiteral("scrgb"));
        refreshDisplay();
        m_connections << connect(window, &QWindow::screenChanged, this, [this] { m_refresh.start(); });
        m_connections << connect(window, &QWindow::xChanged, this, [this] { m_refresh.start(); });
        m_connections << connect(window, &QWindow::yChanged, this, [this] { m_refresh.start(); });
        m_connections << connect(
            window, &QQuickWindow::beforeRendering, this,
            [this]
            {
                auto* ri = m_window->rendererInterface();
                auto* sc =
                    static_cast<QRhiSwapChain*>(ri->getResource(m_window, QSGRendererInterface::RhiSwapchainResource));
                // 请求格式可能被 Qt/驱动回退，必须以实际创建的表面格式为准。
                const bool hdr = sc && m_window->rhi() && m_window->rhi()->backend() == QRhi::D3D11 &&
                                 static_cast<QD3D11SwapChain*>(sc)->colorFormat == DXGI_FORMAT_R16G16B16A16_FLOAT;
                const bool ready = hdr && m_compositor.prepare(m_window->rhi(), sc->currentFrameRenderTarget(),
                                                               m_window->property("_oran7_sdr_white_nits").toFloat());
                m_window->setProperty("_oran7_hdr_composite", ready);
                if (hdr && !ready)
                {
                    m_disableHdr = true;
                    m_reconfigure = true;
                }
                if (!m_logged && sc)
                {
                    m_logged = true;
                    qInfo() << "Video output:" << (ready ? "FP16 scRGB HDR" : "SDR (HDR tone mapping)");
                    if (hdr && !ready)
                        qWarning() << "HDR compositor initialization failed";
                }
            },
            Qt::DirectConnection);
        m_connections << connect(
            window, &QQuickWindow::afterFrameEnd, this,
            [this]
            {
                if (!m_reconfigure)
                    return;
                m_reconfigure = false;
                auto* sc = static_cast<QRhiSwapChain*>(
                    m_window->rendererInterface()->getResource(m_window, QSGRendererInterface::RhiSwapchainResource));
                if (!sc || !m_window->rhi() || m_window->rhi()->backend() != QRhi::D3D11)
                    return;
                const auto format = !m_disableHdr && m_window->property("_oran7_hdr_display").toBool() &&
                                            sc->isFormatSupported(QRhiSwapChain::HDRExtendedSrgbLinear)
                                        ? QRhiSwapChain::HDRExtendedSrgbLinear
                                        : QRhiSwapChain::SDR;
                if (sc->format() == format)
                    return;
                // Qt 6.7.3/D3D11 只在创建时读取格式。完整帧结束后重建同一交换链，保留 HWND、
                // 播放器、场景图和窗口几何；D3D11 pass descriptor 与格式无关，原描述符继续兼容。
                m_compositor.release();
                m_window->setProperty("_oran7_hdr_composite", false);
                sc->destroy();
                // destroy() 注销了资源，但 Qt 6.7.3 保留这个借用字段；清空以便 create 再登记。
                static_cast<QD3D11SwapChain*>(sc)->window = nullptr;
                sc->setFormat(format);
                if (!sc->createOrResize())
                {
                    qWarning() << "HDR output recreation failed; falling back to SDR";
                    sc->destroy();
                    static_cast<QD3D11SwapChain*>(sc)->window = nullptr;
                    sc->setFormat(QRhiSwapChain::SDR);
                    m_disableHdr = true;
                    if (!sc->createOrResize())
                        qWarning() << "SDR output recreation failed";
                }
                m_logged = false;
                m_window->update();
            },
            Qt::DirectConnection);
        m_connections << connect(
            window, &QQuickWindow::afterRendering, this,
            [this]
            {
                if (!isActive(m_window))
                    return;
                auto* sc = static_cast<QRhiSwapChain*>(
                    m_window->rendererInterface()->getResource(m_window, QSGRendererInterface::RhiSwapchainResource));
                if (sc && !m_compositor.render(sc->currentFrameCommandBuffer()))
                {
                    m_disableHdr = true;
                    m_reconfigure = true;
                }
            },
            Qt::DirectConnection);
        m_connections << connect(
            window, &QQuickWindow::sceneGraphInvalidated, this,
            [this]
            {
                m_compositor.release();
                m_window->setProperty("_oran7_hdr_composite", false);
                m_logged = false;
            },
            Qt::DirectConnection);
    }
    emit windowChanged();
}
