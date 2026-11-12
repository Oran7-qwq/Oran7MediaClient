#pragma once
#include "Oran7HdrCompositor.h"
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QAbstractNativeEventFilter>
#include <QTimer>

// 仅主窗口采用 HDR 输出。Qt 自行校验当前屏幕支持，不修改全局环境或其它悬浮窗。
class Oran7HdrWindowOutput : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
  public:
    explicit Oran7HdrWindowOutput(QObject* parent = nullptr);
    ~Oran7HdrWindowOutput() override;
    QQuickWindow* window() const { return m_window; }
    void setWindow(QQuickWindow* window);
    static bool isActive(QQuickWindow* window);
    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override;
  signals:
    void windowChanged();

  private:
    QPointer<QQuickWindow> m_window;
    QList<QMetaObject::Connection> m_connections;
    Oran7HdrCompositor m_compositor;
    bool m_logged = false;
    bool m_disableHdr = false;
    bool m_reconfigure = false;
    QTimer m_refresh;
    void refreshDisplay();
};
