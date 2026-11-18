#pragma once

#include <QPointF>
#include <QQuickItem>
#include <QTimer>
#include <QtQml/qqml.h>

// 只读全局鼠标位置，不注册全局钩子、不截获输入。
class Oran7PointerParallax : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(qreal maximumOffset READ maximumOffset WRITE setMaximumOffset NOTIFY maximumOffsetChanged)
    Q_PROPERTY(QPointF offset READ offset NOTIFY offsetChanged)
    Q_PROPERTY(bool tracking READ tracking NOTIFY trackingChanged)

public:
    explicit Oran7PointerParallax(QQuickItem *parent = nullptr);
    bool active() const { return m_active; }
    void setActive(bool active);
    qreal maximumOffset() const { return m_maximumOffset; }
    void setMaximumOffset(qreal value);
    QPointF offset() const { return m_offset; }
    bool tracking() const { return m_timer.isActive(); }
    static QPointF offsetForPosition(QPointF position, QSizeF bounds, qreal maximum);

signals:
    void activeChanged();
    void maximumOffsetChanged();
    void offsetChanged();
    void trackingChanged();

private:
    void updateTracking();
    void sample();
    void setOffset(QPointF offset);
    QTimer m_timer;
    QMetaObject::Connection m_windowVisibility;
    bool m_active = true;
    qreal m_maximumOffset = 30;
    QPointF m_offset;
};
