#pragma once
#include <QObject>
#include <QPointer>
#include <QVideoSink>

// hasVideo 是轨道元数据；此对象只在收到当前代际的有效视频帧后确认就绪。
class Oran7BackgroundFrameMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVideoSink* sink READ sink WRITE setSink NOTIFY sinkChanged)
    Q_PROPERTY(bool frameReady READ frameReady NOTIFY frameReadyChanged)
  public:
    explicit Oran7BackgroundFrameMonitor(QObject* parent = nullptr) : QObject(parent) {}
    QVideoSink* sink() const { return m_sink.data(); }
    void setSink(QVideoSink* sink);
    bool frameReady() const { return m_frameReady; }
    Q_INVOKABLE void reset();
  signals:
    void sinkChanged();
    void frameReadyChanged();

  private:
    QPointer<QVideoSink> m_sink;
    QMetaObject::Connection m_frames, m_destroyed;
    quint64 m_generation = 0;
    bool m_frameReady = false;
};
