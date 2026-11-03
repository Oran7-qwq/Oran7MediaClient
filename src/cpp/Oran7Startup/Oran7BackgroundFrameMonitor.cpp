#include "Oran7BackgroundFrameMonitor.h"
#include <QVideoFrame>
#include <QThread>

void Oran7BackgroundFrameMonitor::setSink(QVideoSink* sink)
{
    if (m_sink == sink)
        return;
    QObject::disconnect(m_destroyed);
    m_sink = sink;
    if (sink)
        m_destroyed = connect(sink, &QObject::destroyed, this,
                              [this]
                              {
                                  m_sink = nullptr;
                                  reset();
                                  emit sinkChanged();
                              });
    reset();
    emit sinkChanged();
}
void Oran7BackgroundFrameMonitor::reset()
{
    Q_ASSERT(thread() == QThread::currentThread());
    QObject::disconnect(m_frames);
    const quint64 generation = ++m_generation;
    if (m_frameReady)
    {
        m_frameReady = false;
        emit frameReadyChanged();
    }
    if (!m_sink)
        return;
    m_frames = connect(m_sink, &QVideoSink::videoFrameChanged, this,
                       [this, generation](const QVideoFrame& frame)
                       {
                           if (generation != m_generation || m_frameReady || !frame.isValid())
                               return;
                           m_frameReady = true;
                           emit frameReadyChanged();
                       });
}
