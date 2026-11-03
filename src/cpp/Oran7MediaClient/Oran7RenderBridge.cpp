#include "Oran7RenderBridge.h"
#include <QThread>
#include <cmath>
#include <algorithm>
#include <utility>
#include <QMutexLocker>

// 唯一跨线程共享的数据。receiver 的使用与析构失效在同一把锁内串行化。
struct Oran7RenderBridge::Oran7FrameMailbox {
    QMutex mutex;
    Oran7RenderBridge* receiver = nullptr;
    AVFrame* latest = nullptr;
    quint64 generation = 0;
    bool accepting = false;
    bool scheduled = false;
    ~Oran7FrameMailbox() { av_frame_free(&latest); }
};

Oran7RenderBridge::Oran7RenderBridge(QObject* parent)
    : QObject(parent), m_mailbox(std::make_shared<Oran7FrameMailbox>())
{
    m_mailbox->receiver = this;
    connect(D3D11DeviceProvider::instance(), &D3D11DeviceProvider::deviceReady,
        this, &Oran7RenderBridge::refreshDevice, Qt::QueuedConnection);
    // 窗口可能已先于桥初始化，不能只等下一次 deviceReady。
    refreshDevice();
}

Oran7RenderBridge::~Oran7RenderBridge()
{
    Q_ASSERT(QThread::currentThread() == thread());
    {
        QMutexLocker locker(&m_mailbox->mutex);
        m_mailbox->receiver = nullptr;
        m_mailbox->accepting = false;
        av_frame_free(&m_mailbox->latest);
    }
    // Client 先停止采集，再销毁桥；所有渲染项在 GUI 线程释放。
    for (auto& slot : m_slots) {
        if (slot.item) {
            disconnect(slot.item, nullptr, this, nullptr);
            delete slot.item.data();
        }
    }
}

void Oran7RenderBridge::refreshDevice()
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_device.Attach(D3D11DeviceProvider::instance()->acquireDevice());
    emit deviceChanged();
}

ID3D11Device* Oran7RenderBridge::device() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return m_device.Get();
}

std::function<int(const Frame*, AVFrame*)> Oran7RenderBridge::beginPlayback()
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto mailbox = m_mailbox;
    QMutexLocker locker(&mailbox->mutex);
    const quint64 generation = ++mailbox->generation;
    mailbox->accepting = true;
    av_frame_free(&mailbox->latest);
    return [mailbox, generation](const Frame*, AVFrame* frame) {
        if (!frame) return -1;
        QMutexLocker locker(&mailbox->mutex);
        if (!mailbox->receiver || !mailbox->accepting || generation != mailbox->generation) {
            av_frame_free(&frame);
            return 0;
        }
        // 接管回调交来的 AVFrame，无须再 clone；覆盖时立即释放旧帧。
        av_frame_free(&mailbox->latest);
        mailbox->latest = frame;
        if (!mailbox->scheduled) {
            mailbox->scheduled = true;
            auto* receiver = mailbox->receiver;
            if (!QMetaObject::invokeMethod(receiver, [receiver] { receiver->drainFrame(); }, Qt::QueuedConnection)) {
                mailbox->scheduled = false;
                av_frame_free(&mailbox->latest);
            }
        }
        return 0;
    };
}

void Oran7RenderBridge::stopPlayback()
{
    Q_ASSERT(QThread::currentThread() == thread());
    QMutexLocker locker(&m_mailbox->mutex);
    ++m_mailbox->generation;
    m_mailbox->accepting = false;
    av_frame_free(&m_mailbox->latest);
}

void Oran7RenderBridge::drainFrame()
{
    Q_ASSERT(QThread::currentThread() == thread());
    AVFrame* frame = nullptr;
    {
        QMutexLocker locker(&m_mailbox->mutex);
        frame = std::exchange(m_mailbox->latest, nullptr);
        m_mailbox->scheduled = false;
    }
    if (!frame) return;
    const auto it = m_slots.constFind(RenderObject::VideoPlayerRender);
    if (it == m_slots.constEnd() || !it->item) {
        av_frame_free(&frame);
        return;
    }
    it->item->submitFrame(frame);
}

void Oran7RenderBridge::setVideoSourceSize(RenderObject key, const QSize& size)
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_slots[key].srcSize = size;
    scheduleSyncVideoItemSize(key);
}

void Oran7RenderBridge::setScaleMode(RenderObject key, ScaleMode mode)
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_slots[key].scaleMode = mode;
    syncVideoItemSize(key);
}

bool Oran7RenderBridge::renderBlackFrame(RenderObject key)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto it = m_slots.constFind(key);
    if (it == m_slots.constEnd() || !it->item) return false;
    it->item->renderBlackFrame();
    return true;
}

QQuickItem* Oran7RenderBridge::getVideoItem(RenderObject key) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto it = m_slots.constFind(key);
    return it != m_slots.constEnd() ? it->item.data() : nullptr;
}

namespace {
static inline qreal snapToPixel(qreal v, qreal dpr)
{
    return std::round(v * dpr) / dpr;
}

static QRectF calcAspectRectSnapped(const QSizeF &src, const QSizeF &dst, bool fill, qreal dpr)
{
    if (src.isEmpty() || dst.isEmpty())
        return QRectF(QPointF(0,0), dst);

    if (fill) {
        qreal dstWidth = snapToPixel(dst.width(), dpr);
        qreal dstHeight = snapToPixel(dst.height(), dpr);
        return QRectF(QPointF(0, 0), QSizeF(dstWidth, dstHeight));
    }

    const qreal sx = dst.width()  / src.width();
    const qreal sy = dst.height() / src.height();
    const qreal s = std::min(sx, sy);

    QSizeF out(src.width() * s, src.height() * s);
    QPointF topLeft((dst.width() - out.width()) * 0.5,
                    (dst.height() - out.height()) * 0.5);

    topLeft.setX(snapToPixel(topLeft.x(), dpr));
    topLeft.setY(snapToPixel(topLeft.y(), dpr));
    out.setWidth (snapToPixel(out.width(),  dpr));
    out.setHeight(snapToPixel(out.height(), dpr));

    return QRectF(topLeft, out);
}

}

void Oran7RenderBridge::createVideoItem(const RenderObject key,QQuickItem *host)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!host) {
        WARNING_LOG << "createVideoItem: host is nullptr";
        return;
    }

    auto &s = m_slots[key];
    if (!s.item) 
    {
        D3D11VideoItem* item = new D3D11VideoItem(nullptr);
        if(item)
        {
            //<----Successed create D3D11VideoItem
            s.item = item;
            //传递 D3D11VideoItem::sourceVideoSize,回调syncVideoItemSize,设置VideoRenderItem中渲染大小
            connect(s.item, &D3D11VideoItem::sourceSizeChanged,
                    this, [this,key](int w, int h){ setVideoSourceSize(key,QSize(w,h)); },
                    Qt::QueuedConnection);
            //传递 D3D11VideoItem中updatePaintNode 获取到的VideoInfo
            connect(s.item,&D3D11VideoItem::sendVideoFrameInfo,this,[this,key](Oran7VideoInfo info){
                //加入ScaleMode信息
                const auto mode = m_slots[key].scaleMode;
                switch (mode) {
                case ScaleMode::Fit:
                    info.fillModeName = "Fit";
                    break;
                case ScaleMode::Fill:
                    info.fillModeName = "Fill";
                    break;
                default:
                    break;
                }
                info.sourceTag = (key == RenderObject::VideoPlayerRender) ? "player" : "screen";
                auto map = Oran7VideoInfo_To_QVariantMap(info);
                // INFO_LOG << "emit videoRenderInfoUpdated sourceTag=" << info.sourceTag
                //          << " srcWidth=" << info.srcSize.width()
                //          << " fps=" << info.fps;
                emit videoRenderInfoUpdated(map);
            }, Qt::QueuedConnection);
            INFO_LOG << "Success create D3D11VideoItem of "<<(key ==RenderObject::VideoPlayerRender ? "VideoPlayerRender" : "ScreenCaptureRender");
        }
        else
        {
            WARNING_LOG<<"Failed to create D3D11VideoItem !!!";
        }
    }

    if (!s.item) return;
    s.item->setParentItem(host);
    s.item->setParent(this);
    s.item->setVisible(true);

}

bool Oran7RenderBridge::attachVideoItem(const RenderObject key, QQuickItem *host)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!host) return false;

    auto &s = m_slots[key];
    if (!s.item) createVideoItem(key, host);
    if (!s.item) return false;

    if (s.host != host) {
        if (s.cWindow) QObject::disconnect(s.cWindow);
        s.cWindow = connect(host, &QQuickItem::windowChanged, this,
            [this, key](QQuickWindow* window) {
                if (window) D3D11DeviceProvider::instance()->attachWindow(window);
                scheduleSyncVideoItemSize(key);
            });
    }
    if (host->window()) D3D11DeviceProvider::instance()->attachWindow(host->window());

    if (s.host == host) {
        syncVideoItemSize(key);
        return true;
    }

    s.item->setParentItem(host);
    s.item->setVisible(true);

    s.item->setSize(host->size());
    s.item->setPosition({0, 0});
    s.host = host;
    syncVideoItemSize(key);

    if (s.cW) QObject::disconnect(s.cW);
    if (s.cH) QObject::disconnect(s.cH);
    s.cW = QObject::connect(host, &QQuickItem::widthChanged, this, [this, key]{
        this->scheduleSyncVideoItemSize(key);
    });
    s.cH = QObject::connect(host, &QQuickItem::heightChanged, this, [this, key]{
        this->scheduleSyncVideoItemSize(key);
    });

    return true;
}



void Oran7RenderBridge::syncVideoItemSize(RenderObject key)
{
    Q_ASSERT(QThread::currentThread() == thread());
    auto &s =m_slots[key];
    if (!s.item || !s.host) return;

    const QSizeF hostSize(s.host->width(), s.host->height());
    if (hostSize.isEmpty()) return;

    // D3D11VideoItem 几何永远铺满 host（x=0, y=0, w=hostW, h=hostH）。
    // Fit/Fill 模式只影响 render node 内部的 quad 顶点（contentRect），
    // 不移动/缩放 item 本体。这样 ShaderEffectSource offscreen 捕获时
    // 坐标系始终和 host 一致，不会混入布局偏移。
    s.item->setX(0);
    s.item->setY(0);
    s.item->setWidth(hostSize.width());
    s.item->setHeight(hostSize.height());

    // 计算 Fit/Fill 后的真实视频绘制区域（item-local 坐标）
    QRectF contentRect;
    if (s.srcSize.isEmpty()) {
        contentRect = QRectF(0, 0, hostSize.width(), hostSize.height());
    } else {
        const bool fill = (s.scaleMode == ScaleMode::Fill);
        qreal dpr = 1.0;
        if (s.host && s.host->window())
            dpr = s.host->window()->devicePixelRatio();
        contentRect = calcAspectRectSnapped(QSizeF(s.srcSize), hostSize, fill, dpr);
    }

    // 传递给 D3D11VideoItem → QSGRenderNode，用于 quad 顶点定位
    s.item->setContentRect(contentRect);
}

void Oran7RenderBridge::scheduleSyncVideoItemSize(RenderObject key)
{
    Q_ASSERT(QThread::currentThread() == thread());
    auto &s = m_slots[key];
    if (s.syncPending) return;
    s.syncPending = true;
    QMetaObject::invokeMethod(this, [this,key]{
        auto &s = m_slots[key];
        s.syncPending = false;
        syncVideoItemSize(key);
    }, Qt::QueuedConnection);
}


