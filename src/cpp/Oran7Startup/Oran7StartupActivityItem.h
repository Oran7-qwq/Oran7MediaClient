#pragma once
#include <QQuickItem>
#include <QColor>
#include <memory>

class Oran7StartupActivityBackend;
class Oran7StartupActivityItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY colorsChanged)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY colorsChanged)
    Q_PROPERTY(bool nativeActive READ nativeActive NOTIFY nativeActiveChanged)
  public:
    explicit Oran7StartupActivityItem(QQuickItem* parent = nullptr);
    ~Oran7StartupActivityItem() override;
    bool running() const { return m_running; }
    void setRunning(bool running);
    QColor accent() const { return m_accent; }
    void setAccent(const QColor& value);
    QColor trackColor() const { return m_track; }
    void setTrackColor(const QColor& value);
    bool nativeActive() const { return m_nativeActive; }
  signals:
    void runningChanged();
    void colorsChanged();
    void nativeActiveChanged();

  protected:
    void componentComplete() override;
    void geometryChange(const QRectF& current, const QRectF& old) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;

  private:
    void scheduleSync();
    void syncNative();
    void dropNative();
    std::unique_ptr<Oran7StartupActivityBackend> m_native;
    QList<QMetaObject::Connection> m_windowConnections;
    QSize m_size;
    QColor m_accent{"#beb7ff"}, m_track{"#323746"};
    bool m_running = false, m_nativeActive = false, m_pending = false, m_unavailable = false;
};
