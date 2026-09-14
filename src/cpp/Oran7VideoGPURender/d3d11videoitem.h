// d3d11videoitem.h
#pragma once

#include "GlobalHelper.h"
#include "D3d11VideoRenderNode.h"
#include "D3d11DeviceProvider.h"

#include <QQuickItem>
#include <QMutex>
#include <QQueue>
#include <QSize>

#include <QSGSimpleTextureNode>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <QQuickWindow>

#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/hwcontext_d3d11va.h>   // AVD3D11FrameDescriptor
#include <libavutil/pixdesc.h>
}

class D3D11VideoItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QRectF videoContentRect READ videoContentRect NOTIFY videoContentRectChanged)
public:
    explicit D3D11VideoItem(QQuickItem *parent = nullptr);
    void submitFrame(AVFrame *frameRef);
    void renderBlackFrame();

    // 由 syncVideoItemSize() 调用：计算 Fit/Fill 后的真实绘制区域。
    void setContentRect(const QRectF &r) {
        if (m_contentRect != r) {
            m_contentRect = r;
            emit videoContentRectChanged();
            update(); // 触发 updatePaintNode，把新 contentRect 传给 render node
        }
    }

    QRectF videoContentRect() const { return m_contentRect; }

    Q_INVOKABLE void setDebugLog(bool on) { m_debugLog.store(on, std::memory_order_release); }
    Q_INVOKABLE bool debugLog() const { return m_debugLog.load(std::memory_order_acquire); }

signals:
    void sourceSizeChanged(int w, int h);
    void sendVideoFrameInfo(Oran7VideoInfo info);
    void videoContentRectChanged();
private slots:
    void onBeforeRendering();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void releaseResources() override;

private:
    void hookWindow(QQuickWindow *window);
    AVFrame* takeLatestQueuedFrame();  // 取队列里最新帧，丢弃旧帧
    bool initD3D11Resources();
    bool ensureRgbaTarget(int w, int h,DXGI_FORMAT fmt);
    bool ensureVideoProcessor(int srcW, int srcH);
    bool blitNv12ToRgba(ID3D11Texture2D *srcTex, int srcW, int srcH, int slice);

    //QMutex m_mutex;
    //AVFrame *m_pendingFrame = nullptr;
    ComPtr<ID3D11Device> m_dev;
    ComPtr<ID3D11DeviceContext> m_ctx;
    ComPtr<ID3D11VideoDevice> m_videoDev;
    ComPtr<ID3D11VideoContext> m_videoCtx;
    ComPtr<ID3D11Texture2D> m_rgbaTex;

    int   m_lastSrcW = 0;
    int   m_lastSrcH = 0;
    QSize m_bgraSize;
    DXGI_FORMAT m_targetFmt = DXGI_FORMAT_UNKNOWN;
    ComPtr<ID3D11VideoProcessorEnumerator> m_vpEnum;
    ComPtr<ID3D11VideoProcessor> m_vp;
    std::atomic_bool m_needClearBlack{false};

    std::atomic_bool m_updatePending{false};
    std::atomic<AVFrame*> m_latestFrame{nullptr};

    // 帧队列：避免单槽 m_latestFrame 互相覆盖丢帧
    QMutex m_frameMutex;
    QQueue<AVFrame*> m_frameQueue;
    static constexpr int kMaxQueued = 3;

    // seq 追踪丢帧
    std::atomic<uint64_t> m_submitSeq{0};
    std::atomic<uint64_t> m_renderSeq{0};
    std::atomic_bool m_debugLog{false};  // 默认开，QML 可 setDebugLog(false) 关闭

    // 每秒 submit 帧数统计（每实例独立）
    int m_submitFps{0};
    int m_submitCount{0};
    qint64 m_submitT0{0};

    // 每秒视频信息发射周期计时（每实例独立，避免多实例 static 共享）
    qint64 m_infoEmitT0{0};

    // 帧间隔抖动检测（每实例独立）
    qint64 m_lastRenderMs{0};

    // 信号连接守卫（每实例独立，之前 static 导致多实例互相断连）
    QMetaObject::Connection m_beforeConn;
    QMetaObject::Connection m_swapConn;

    D3D11VideoRenderNode *m_renderNode = nullptr;
    QQuickWindow *m_window = nullptr;
    QRectF m_contentRect;  // Fit/Fill 后的视频绘制区域（item-local 坐标）
};

static bool isFormatSupported(ID3D11VideoProcessorEnumerator* e, DXGI_FORMAT fmt) {
    UINT flags = 0;
    if (FAILED(e->CheckVideoProcessorFormat(fmt, &flags))) return false;
    return (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) != 0;
}
