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

signals:
    void d3d11DeviceReady(ID3D11Device* dev);
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
    bool ensureBgraTarget(int w, int h);
    bool ensureVideoProcessor(int srcW, int srcH);
    bool blitNv12ToBgra(ID3D11Texture2D *srcTex, int srcW, int srcH, int slice);

    QMutex m_mutex;
    AVFrame *m_pendingFrame = nullptr;
    ComPtr<ID3D11Device> m_dev;
    ComPtr<ID3D11DeviceContext> m_ctx;
    ComPtr<ID3D11VideoDevice> m_videoDev;
    ComPtr<ID3D11VideoContext> m_videoCtx;
    ComPtr<ID3D11Texture2D> m_bgraTex;
    QSize m_bgraSize;
    ComPtr<ID3D11VideoProcessorEnumerator> m_vpEnum;
    ComPtr<ID3D11VideoProcessor> m_vp;

    QQuickWindow *m_window = nullptr;
};
