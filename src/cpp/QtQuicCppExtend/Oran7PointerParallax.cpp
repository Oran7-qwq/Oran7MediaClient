#include "Oran7PointerParallax.h"

#include <QCursor>
#include <QQuickWindow>
#include <QtMath>
#include <algorithm>

Oran7PointerParallax::Oran7PointerParallax(QQuickItem *parent) : QQuickItem(parent)
{
    setAcceptedMouseButtons(Qt::NoButton);
    m_timer.setInterval(32);
    connect(&m_timer, &QTimer::timeout, this, &Oran7PointerParallax::sample);
    connect(this, &QQuickItem::visibleChanged, this, &Oran7PointerParallax::updateTracking);
    connect(this, &QQuickItem::widthChanged, this, &Oran7PointerParallax::updateTracking);
    connect(this, &QQuickItem::heightChanged, this, &Oran7PointerParallax::updateTracking);
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *host) {
        QObject::disconnect(m_windowVisibility);
        if (host)
            m_windowVisibility = connect(host, &QWindow::visibilityChanged, this,
                                         &Oran7PointerParallax::updateTracking);
        updateTracking();
    });
}

void Oran7PointerParallax::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
    updateTracking();
}

void Oran7PointerParallax::setMaximumOffset(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = std::max(qreal(0), value);
    if (m_maximumOffset == value)
        return;
    m_maximumOffset = value;
    emit maximumOffsetChanged();
    updateTracking();
}

QPointF Oran7PointerParallax::offsetForPosition(QPointF position, QSizeF bounds, qreal maximum)
{
    if (bounds.width() <= 0 || bounds.height() <= 0 || !qIsFinite(maximum) || maximum <= 0
        || !qIsFinite(position.x()) || !qIsFinite(position.y()))
        return {};
    // 工作台中心为原点；鼠标移出窗口后也限制在每轴的最大行程内。
    return {-std::clamp(position.x() * 2 / bounds.width() - 1, qreal(-1), qreal(1)) * maximum,
            -std::clamp(position.y() * 2 / bounds.height() - 1, qreal(-1), qreal(1)) * maximum};
}

void Oran7PointerParallax::setOffset(QPointF offset)
{
    if (m_offset == offset)
        return;
    m_offset = offset;
    emit offsetChanged();
}

void Oran7PointerParallax::updateTracking()
{
    const auto *host = window();
    const bool shouldTrack = m_active && isVisible() && width() > 0 && height() > 0
        && m_maximumOffset > 0 && host && host->isVisible()
        && host->visibility() != QWindow::Minimized;
    const bool wasTracking = tracking();
    if (shouldTrack) {
        if (!wasTracking)
            m_timer.start();
        sample();
    } else {
        m_timer.stop();
        setOffset({});
    }
    if (wasTracking != tracking())
        emit trackingChanged();
}

void Oran7PointerParallax::sample()
{
    setOffset(offsetForPosition(mapFromGlobal(QCursor::pos()), QSizeF(width(), height()), m_maximumOffset));
}
