// d3d11videoitem.h
#pragma once

#include "globalhelper.h"

#include <QQuickItem>
#include <QMutex>
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
public:
    explicit D3D11VideoItem(QQuickItem *parent = nullptr);
    void submitFrame(AVFrame *frameRef);
    void renderBlackFrame();

signals:

    void sourceSizeChanged(int w, int h);
    void sendVideoFrameInfo(Oran7VideoInfo info);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void releaseResources() override;

private:
    void hookWindow(QQuickWindow *window);
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

    std::atomic<AVFrame*> m_latestFrame{nullptr};
    QQuickWindow *m_window = nullptr;
};

static bool isFormatSupported(ID3D11VideoProcessorEnumerator* e, DXGI_FORMAT fmt) {
    UINT flags = 0;
    if (FAILED(e->CheckVideoProcessorFormat(fmt, &flags))) return false;
    return (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) != 0;
}
