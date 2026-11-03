#include "Oran7StartupActivityItem.h"
#include "Oran7StartupActivityBackend.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>

Oran7StartupActivityItem::Oran7StartupActivityItem(QQuickItem* parent) : QQuickItem(parent)
{
    connect(this, &QQuickItem::visibleChanged, this,
            [this]
            {
                if (!isVisible() && m_native)
                    syncNative();
                else
                    scheduleSync();
            });
    connect(this, &QQuickItem::windowChanged, this,
            [this](QQuickWindow* window)
            {
                for (const auto& connection : m_windowConnections)
                    disconnect(connection);
                m_windowConnections.clear();
                dropNative();
                m_unavailable = false;
                if (window)
                {
                    m_windowConnections << connect(window, &QWindow::visibleChanged, this,
                                                   [this](bool visible)
                                                   {
                                                       if (!visible && m_native)
                                                           syncNative();
                                                       else
                                                           scheduleSync();
                                                   });
                    m_windowConnections << connect(window, &QWindow::xChanged, this,
                                                   &Oran7StartupActivityItem::scheduleSync);
                    m_windowConnections << connect(window, &QWindow::yChanged, this,
                                                   &Oran7StartupActivityItem::scheduleSync);
                    m_windowConnections << connect(window, &QWindow::opacityChanged, this,
                                                   &Oran7StartupActivityItem::scheduleSync);
                    m_windowConnections << connect(window, &QWindow::screenChanged, this,
                                                   &Oran7StartupActivityItem::scheduleSync);
                }
                scheduleSync();
            });
}
Oran7StartupActivityItem::~Oran7StartupActivityItem() = default;
void Oran7StartupActivityItem::componentComplete()
{
    QQuickItem::componentComplete();
    scheduleSync();
}
void Oran7StartupActivityItem::geometryChange(const QRectF& current, const QRectF& old)
{
    QQuickItem::geometryChange(current, old);
    scheduleSync();
}
void Oran7StartupActivityItem::itemChange(ItemChange change, const ItemChangeData& data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemDevicePixelRatioHasChanged)
        scheduleSync();
}
void Oran7StartupActivityItem::setRunning(bool value)
{
    if (m_running == value)
        return;
    m_running = value;
    emit runningChanged();
    scheduleSync();
}
void Oran7StartupActivityItem::setAccent(const QColor& value)
{
    if (m_accent == value)
        return;
    m_accent = value;
    dropNative();
    emit colorsChanged();
    scheduleSync();
}
void Oran7StartupActivityItem::setTrackColor(const QColor& value)
{
    if (m_track == value)
        return;
    m_track = value;
    dropNative();
    emit colorsChanged();
    scheduleSync();
}
void Oran7StartupActivityItem::scheduleSync()
{
    if (m_pending || !isComponentComplete())
        return;
    m_pending = true;
    QTimer::singleShot(0, this,
                       [this]
                       {
                           m_pending = false;
                           syncNative();
                       });
}
void Oran7StartupActivityItem::dropNative()
{
    m_native.reset();
    if (m_nativeActive)
    {
        m_nativeActive = false;
        emit nativeActiveChanged();
    }
}
void Oran7StartupActivityItem::syncNative()
{
    auto* host = window();
    // 离屏检查继续使用原 QML 降级路径，不意外创建桌面子窗口/硬件设备。
    if (!host || QGuiApplication::platformName() != "windows" || m_unavailable)
        return;
    const auto ratio = host->devicePixelRatio();
    const auto point = mapToScene(QPointF(0, 0)) * ratio;
    const QRect bounds(qRound(point.x()), qRound(point.y()), qRound(width() * ratio), qRound(height() * ratio));
    if (bounds.isEmpty())
    {
        dropNative();
        return;
    }
    if (m_native && m_size != bounds.size())
        dropNative();
    if (!m_native)
    {
        if (!host->isVisible() || !isVisible())
            return;
        auto backend = std::make_unique<Oran7StartupActivityBackend>();
        if (!backend->initialize(reinterpret_cast<void*>(host->winId()), bounds.size(), m_accent, m_track))
        {
            m_unavailable = true;
            qWarning() << "[Startup] native activity initialization failed; using QML fallback";
            return;
        }
        m_size = bounds.size();
        m_native = std::move(backend);
    }
    const bool show = host->isVisible() && isVisible();
    if (!m_native->setRunning(show && m_running) || !m_native->present(bounds, show, host->opacity()))
    {
        dropNative();
        m_unavailable = true;
        qWarning() << "[Startup] native activity unavailable; using QML fallback";
        return;
    }
    if (!m_nativeActive)
    {
        m_nativeActive = true;
        emit nativeActiveChanged();
    }
}
