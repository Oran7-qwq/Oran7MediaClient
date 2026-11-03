#pragma once

#include "D3d11Videoitem.h"
#include <functional>
#include <memory>

struct Frame;

// 由 Oran7MediaClient 持有；渲染槽位、设备和 QQuickItem 操作只在 GUI 线程执行。
class Oran7RenderBridge : public QObject
{
    Q_OBJECT
public:
    enum class ScaleMode { Fit, Fill };
    enum class RenderObject { VideoPlayerRender, ScreenCaptureRender };

    explicit Oran7RenderBridge(QObject* parent = nullptr);
    ~Oran7RenderBridge() override;

    void createVideoItem(RenderObject key, QQuickItem* host);
    bool attachVideoItem(RenderObject key, QQuickItem* host);
    void setVideoSourceSize(RenderObject key, const QSize& size);
    void setScaleMode(RenderObject key, ScaleMode mode);
    bool renderBlackFrame(RenderObject key);
    QQuickItem* getVideoItem(RenderObject key) const;
    ID3D11Device* device() const;

    // 播放器只保留共享邮箱，不捕获桥或 Client；旧播放器销毁可在后台完成。
    std::function<int(const Frame*, AVFrame*)> beginPlayback();
    void stopPlayback();

signals:
    void deviceChanged();
    void videoRenderInfoUpdated(const QVariantMap& info);

private:
    struct Oran7RenderSlot {
        QPointer<D3D11VideoItem> item;
        QPointer<QQuickItem> host;
        QMetaObject::Connection cW, cH, cWindow;
        QSize srcSize;
        ScaleMode scaleMode = ScaleMode::Fit;
        bool syncPending = false;
    };
    struct Oran7FrameMailbox;
    void drainFrame();
    void syncVideoItemSize(RenderObject key);
    void scheduleSyncVideoItemSize(RenderObject key);
    void refreshDevice();

    QHash<RenderObject, Oran7RenderSlot> m_slots;
    ComPtr<ID3D11Device> m_device;
    std::shared_ptr<Oran7FrameMailbox> m_mailbox;
};
