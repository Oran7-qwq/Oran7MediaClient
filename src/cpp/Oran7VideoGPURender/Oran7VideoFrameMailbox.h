#pragma once
#include "Oran7GlobalHelper.h"
#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <functional>
#include <memory>
#include <atomic>
extern "C"
{
#include <libavutil/frame.h>
}

struct Oran7VideoFrameDeleter
{
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};
using Oran7VideoFrame = std::unique_ptr<AVFrame, Oran7VideoFrameDeleter>;

// 单槽最新帧邮箱：后台只持共享值，接收者投递与析构失效由同一把锁保护。
class Oran7VideoFrameMailbox
{
  public:
    struct Packet
    {
        Oran7VideoFrame frame;
        quint64 epoch = 0;
        int fps = 0;
        bool clear = false;
    };
    struct Delivery
    {
        bool update = false;
        bool sizeChanged = false;
        bool infoChanged = false;
        QSize sourceSize;
        Oran7VideoInfo info;
    };
    void attach(QObject* receiver, std::function<void()> notification)
    {
        QMutexLocker lock(&m_mutex);
        m_receiver = receiver;
        m_notification = std::move(notification);
        m_closed = false;
    }
    void close()
    {
        QMutexLocker lock(&m_mutex);
        m_receiver = nullptr;
        m_notification = {};
        m_closed = true;
        ++m_epoch;
        m_frame.reset();
        m_delivery = {};
    }
    void submit(AVFrame* frame)
    {
        Oran7VideoFrame owned(frame);
        if (!owned)
            return;
        QMutexLocker lock(&m_mutex);
        if (m_closed)
            return;
        if (!m_fpsClock.isValid() || m_fpsClock.elapsed() > 2000)
        {
            m_fpsClock.start();
            m_count = 0;
            m_fps = 0;
        }
        ++m_count;
        if (m_fpsClock.elapsed() >= 1000)
        {
            m_fps = int(m_count * 1000 / m_fpsClock.elapsed());
            m_count = 0;
            m_fpsClock.restart();
        }
        m_frame = std::move(owned);
        m_clear = false;
        m_delivery.update = true;
        notifyLocked();
    }
    void clear()
    {
        QMutexLocker lock(&m_mutex);
        ++m_epoch;
        m_releaseSerial.fetch_add(1, std::memory_order_release);
        m_frame.reset();
        m_clear = true;
        m_delivery = {};
        m_delivery.update = true;
        m_sourceSize = {};
        m_infoClock.invalidate();
        m_fpsClock.invalidate();
        notifyLocked();
    }
    quint64 releaseSerial() const { return m_releaseSerial.load(std::memory_order_acquire); }
    Packet take()
    {
        QMutexLocker lock(&m_mutex);
        Packet packet{std::move(m_frame), m_epoch, m_fps, m_clear};
        m_clear = false;
        return packet;
    }
    void publish(quint64 epoch, const Oran7VideoInfo& info)
    {
        QMutexLocker lock(&m_mutex);
        if (m_closed || epoch != m_epoch)
            return;
        const bool sizeChanged = m_sourceSize != info.srcSize;
        if (sizeChanged)
        {
            m_sourceSize = info.srcSize;
            m_delivery.sourceSize = info.srcSize;
            m_delivery.sizeChanged = true;
        }
        if (sizeChanged || !m_infoClock.isValid() || m_infoClock.elapsed() >= 1000)
        {
            m_delivery.info = info;
            m_delivery.infoChanged = true;
            m_infoClock.start();
        }
        if (m_delivery.sizeChanged || m_delivery.infoChanged)
            notifyLocked();
    }
    Delivery drain()
    {
        QMutexLocker lock(&m_mutex);
        auto delivery = m_delivery;
        m_delivery = {};
        m_scheduled = false;
        return delivery;
    }

  private:
    void notifyLocked()
    {
        if (!m_scheduled && m_receiver)
        {
            m_scheduled = QMetaObject::invokeMethod(m_receiver, m_notification, Qt::QueuedConnection);
        }
    }
    QMutex m_mutex;
    QObject* m_receiver = nullptr; // 借用，close() 在接收者析构入口失效。
    std::function<void()> m_notification;
    bool m_closed = false, m_scheduled = false, m_clear = false;
    quint64 m_epoch = 0;
    std::atomic<quint64> m_releaseSerial{0};
    Oran7VideoFrame m_frame;
    Delivery m_delivery;
    QSize m_sourceSize;
    QElapsedTimer m_fpsClock, m_infoClock;
    qint64 m_count = 0;
    int m_fps = 0;
};
