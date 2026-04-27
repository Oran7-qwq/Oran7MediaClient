// d3d11videoitem.cpp
#include "d3d11videoitem.h"

#include <QMutexLocker>
#include <QMetaObject>
#include <QDebug>
#include <QThread>

D3D11VideoItem::D3D11VideoItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *w){
        if (w) hookWindow(w);
        else {
            m_window = nullptr;
        }
    }, Qt::DirectConnection);
}

void D3D11VideoItem::submitFrame(AVFrame *frameRef)
{
#if 0
    static int submitCount = 0;
    static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    submitCount++;
    auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - t0 >= 1000) {
        INFO_LOG << "submitFrame fps =" << submitCount;
        submitCount = 0;
        t0 = now;
    }
#endif
    AVFrame* old = m_latestFrame.exchange(frameRef, std::memory_order_acq_rel);
    if (old) av_frame_free(&old);

    bool expected = false;
    /*if (m_updatePending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))*/ {
        QMetaObject::invokeMethod(this, [this]{
#if 1
            static int submitCount = 0;
            static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
            submitCount++;
            auto now = QDateTime::currentMSecsSinceEpoch();
            if (now - t0 >= 1000) {
                INFO_LOG << "update fps =" << submitCount;
                submitCount = 0;
                t0 = now;
            }
#endif
            update();
        }, Qt::QueuedConnection);
    }
}

void D3D11VideoItem::renderBlackFrame()
{
    INFO_LOG << "renderBlackFrame request";
    m_needClearBlack.store(true, std::memory_order_release);
    update(); // 让渲染线程尽快跑 updatePaintNode
}

void D3D11VideoItem::onBeforeRendering()
{
    if (m_renderNode) {
        AVFrame *cur = m_latestFrame.exchange(nullptr, std::memory_order_acq_rel);
        if (cur) {
            m_renderNode->setFrame(cur);
#if 0
            static int frame_count1 = 0;
            static qint64 t01 = QDateTime::currentMSecsSinceEpoch();
            frame_count1++;
            auto now1 = QDateTime::currentMSecsSinceEpoch();
            if (now1 - t01 >= 1000)//1000ms
            {
                INFO_LOG<<"d3d11VideoItem updataPaintNode frameCount:"<<frame_count1<<" per second.";
                frame_count1 = 0;t01 = now1;
            }
#endif
        }
    }

    if (window())
        window()->update();
}


void D3D11VideoItem::componentComplete()
{
    QQuickItem::componentComplete();
    if (window())
    {
        hookWindow(window());
        INFO_LOG<<"By D3D11VideoItem::componentComplete hookWindow.";
    }
    else INFO_LOG<<"Cannot hookWindow, because of in 'ComponentComplete' window() is nullptr.";
}

void D3D11VideoItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);

    if (change == ItemSceneChange) {
        // 这里 data.window 有时为空，或者时序早
        if (data.window) {
            hookWindow(data.window);
        } else if (window()) {
            hookWindow(window());
        }
    }
}

void D3D11VideoItem::releaseResources()
{
    m_rgbaTex.Reset();
    m_vp.Reset();
    m_vpEnum.Reset();
    // 可把 pendingFrame 清掉
    // QMutexLocker locker(&m_mutex);
    // if (m_pendingFrame) {
    //     av_frame_free(&m_pendingFrame);
    //     m_pendingFrame = nullptr;
    // }
    AVFrame* frame = m_latestFrame.exchange(nullptr, std::memory_order_acq_rel);
    if (frame) {
        av_frame_free(&frame);
    }
}

void D3D11VideoItem::hookWindow(QQuickWindow *w)
{
    if (m_window == w) return;
    m_window = w;
    INFO_LOG<<"D3D11VideoItem New hookWindow.";

    connect(D3D11DeviceProvider::instance(), &D3D11DeviceProvider::deviceLost,
            this, [this](){
                m_ctx.Reset();
                m_dev.Reset();
                m_videoDev.Reset();
                m_videoCtx.Reset();
            }, Qt::DirectConnection);

    // static QMetaObject::Connection beforeConn1;
    // if (beforeConn1)
    //     disconnect(beforeConn1);
    // beforeConn1 = connect(w, &QQuickWindow::beforeRendering,
    //                      this, &D3D11VideoItem::onBeforeRendering,
    //                      Qt::DirectConnection);

    // static QMetaObject::Connection beforeConn2;
    // if (beforeConn2)
    //     disconnect(beforeConn2);
    // beforeConn2 = connect(w, &QQuickWindow::frameSwapped, this, [](){
    //     static int count = 0;
    //     static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    //     count++;
    //     auto now = QDateTime::currentMSecsSinceEpoch();
    //     if (now - t0 >= 1000) {
    //         INFO_LOG << "frameSwapped fps =" << count;
    //         count = 0;
    //         t0 = now;
    //     }
    // });


    w->setPersistentSceneGraph(true);
    w->setPersistentGraphics(true);
}

bool D3D11VideoItem::initD3D11Resources()
{
    if (m_dev) return true;

    QSGRendererInterface *ri = window()->rendererInterface();
    if (!ri) return false;

    auto *dev = static_cast<ID3D11Device*>(ri->getResource(window(), QSGRendererInterface::DeviceResource));
    if (!dev) return false;

    m_dev = dev;
    m_dev->GetImmediateContext(&m_ctx);
    m_dev.As(&m_videoDev);
    m_ctx.As(&m_videoCtx);

    return true;
}

bool D3D11VideoItem::ensureRgbaTarget(int w, int h,DXGI_FORMAT fmt)
{
    if (m_rgbaTex && m_bgraSize == QSize(w, h)&& m_targetFmt == fmt)
        return false; // 没重建

    m_rgbaTex.Reset();
    m_bgraSize = QSize(w, h);
    m_targetFmt = fmt;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = (UINT)w;
    desc.Height = (UINT)h;
    desc.Format = fmt;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    HRESULT hr = m_dev->CreateTexture2D(&desc, nullptr, &m_rgbaTex);
    UINT sup = 0;

    // HRESULT hr2 = m_dev->CheckFormatSupport(DXGI_FORMAT_B8G8R8A8_UNORM, &sup);
    // INFO_LOG << QString("CheckFormatSupport BGRA8 hr=0x%1 sup=0x%2")
    //                 .arg(uint32_t(hr2),8,16,QChar('0'))
    //                 .arg(uint32_t(sup),8,16,QChar('0'));
    // const bool canSample = (sup & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0;
    // INFO_LOG << "BGRA8 shader sample: " << canSample;

    return SUCCEEDED(hr) && m_rgbaTex;
}


bool D3D11VideoItem::ensureVideoProcessor(int srcW, int srcH)
{
    m_vp.Reset();
    m_vpEnum.Reset();

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC c{};
    c.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    c.InputWidth  = srcW;
    c.InputHeight = srcH;
    c.OutputWidth  = srcW;
    c.OutputHeight = srcH;
    c.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr = m_videoDev->CreateVideoProcessorEnumerator(&c, &m_vpEnum);
    if (FAILED(hr)) return false;

    hr = m_videoDev->CreateVideoProcessor(m_vpEnum.Get(), 0, &m_vp);
    return SUCCEEDED(hr);
}

bool D3D11VideoItem::blitNv12ToRgba(ID3D11Texture2D *srcTex, int srcW, int srcH, int slice)
{
    if (!srcTex || !m_vp || !m_vpEnum || !m_videoCtx) return false;

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
    inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inDesc.Texture2D.ArraySlice = slice;

    ComPtr<ID3D11VideoProcessorInputView> inView;
    HRESULT hr = m_videoDev->CreateVideoProcessorInputView(
        srcTex, m_vpEnum.Get(), &inDesc, &inView);
    if (FAILED(hr)) return false;

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{};
    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outDesc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11VideoProcessorOutputView> outView;
    hr = m_videoDev->CreateVideoProcessorOutputView(
        m_rgbaTex.Get(), m_vpEnum.Get(), &outDesc, &outView);
    if (FAILED(hr)) { WARNING_LOG << "CreateVPOutputView failed hr="<<hr; return false; }

    RECT srcRect{0, 0, srcW, srcH};
    RECT dstRect{0, 0, srcW, srcH};

    m_videoCtx->VideoProcessorSetStreamSourceRect(m_vp.Get(), 0, TRUE, &srcRect);
    m_videoCtx->VideoProcessorSetStreamDestRect(m_vp.Get(), 0, TRUE, &dstRect);

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inView.Get();

    hr = m_videoCtx->VideoProcessorBlt(m_vp.Get(), outView.Get(), 0, 1, &stream);
    if (FAILED(hr)) { WARNING_LOG << "VideoProcessorBlt failed hr="<<hr; return false; }
    return SUCCEEDED(hr);
}

// QSGNode* D3D11VideoItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData* data)
// {
//     if (!m_window && window()) {
//         hookWindow(window());
//         INFO_LOG<<"By D3D11VideoItem::updatePaintNode hookWindow.";
//     }

//     auto *node = static_cast<QSGSimpleTextureNode*>(oldNode);
//     if (!node) {
//         node = new QSGSimpleTextureNode();
//         node->setOwnsTexture(true);  // 节点管理纹理生命周期
//     }

//     AVFrame *cur = m_latestFrame.exchange(nullptr);;
//     //=======================Render Black======================
//     const bool needBlack = m_needClearBlack.exchange(false, std::memory_order_acq_rel);
//     if (needBlack) {
//         // 如果拿到了新视频帧，丢掉它，避免覆盖黑屏
//         if (cur) {
//             av_frame_free(&cur);
//             cur = nullptr;
//         }

//         if (!m_dev) {
//             if (!initD3D11Resources()) {
//                 node->setRect(boundingRect());
//                 delete node;
//                 return nullptr;
//             }
//         }
//         // 用当前 item 尺寸来建黑屏纹理
//         int w = int(width());
//         int h = int(height());
//         if (w > 0 && h > 0) {
//             const bool recreated = ensureRgbaTarget(w, h,DXGI_FORMAT_R8G8B8A8_UNORM);
//             // 确保这张纹理是可 RTV 的
//             ComPtr<ID3D11RenderTargetView> rtv;
//             D3D11_RENDER_TARGET_VIEW_DESC desc = {};
//             desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
//             desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
//             desc.Texture2D.MipSlice = 0;

//             HRESULT hr = m_dev->CreateRenderTargetView(m_rgbaTex.Get(), &desc, rtv.GetAddressOf());
//             if (SUCCEEDED(hr) && rtv) {
//                 const float color[4] = {0.f, 0.f, 0.f, 1.f};
//                 m_ctx->ClearRenderTargetView(rtv.Get(), color);
//                 m_ctx->Flush();
//             } else {
//                 WARNING_LOG << "CreateRenderTargetView failed hr=" << QString::number(hr, 16).toStdString();
//             }

//             // 确保 node 有纹理 wrapper
//             if (recreated || node->texture() == nullptr) {
//                 QSGTexture *tex = QNativeInterface::QSGD3D11Texture::fromNative(
//                     m_rgbaTex.Get(), window(), m_bgraSize, QQuickWindow::TextureHasAlphaChannel);
//                 if (tex) node->setTexture(tex);
//             }
//         }
//         node->setRect(boundingRect());
//         return node;
//     }
//     //=============================================================

//     if (!cur) {
//         if (node && node->texture()) {
//             node->setRect(boundingRect());
//             return node;
//         }
//         else
//         {
//             delete node;
//             return nullptr;
//         }
//     }

//     if (!m_dev) {
//         if (!initD3D11Resources()) {
//             av_frame_free(&cur);
//             node->setRect(boundingRect());
//             WARNING_LOG<<"Failed to initD3D11Resources().";
//             delete node;
//             return nullptr;
//         }
//     }

//     auto *srcTex = reinterpret_cast<ID3D11Texture2D*>(cur->data[0]);
//     D3D11_TEXTURE2D_DESC sdesc{};
//     srcTex->GetDesc(&sdesc);
//     int srcW = (int)sdesc.Width;
//     int srcH = (int)sdesc.Height;

//     if (srcW != m_lastSrcW || srcH != m_lastSrcH) {
//         m_lastSrcW = srcW; m_lastSrcH = srcH;
//         INFO_LOG<<"emitSourceSizeChanged"<<srcW<<"/"<<srcH;
//         emit sourceSizeChanged(srcW, srcH);
//     }

//     const bool recreated = ensureRgbaTarget(srcW, srcH,DXGI_FORMAT_R8G8B8A8_UNORM);
//     if (sdesc.Format == DXGI_FORMAT_NV12 || sdesc.Format == DXGI_FORMAT_P010) {
//         if (!m_vp || !m_vpEnum)
//         {
//             if (!ensureVideoProcessor(srcW, srcH)) {
//                 av_frame_free(&cur);
//                 return node;
//             }
//             if(!isFormatSupported(m_vpEnum.Get(),DXGI_FORMAT_R8G8B8A8_UNORM))
//                 WARNING_LOG<<"DXGI_FORMAT_R8G8B8A8_UNORM not Supported";
//             else
//                 INFO_LOG<<"DXGI_FORMAT_R8G8B8A8_UNORM is Supported.";
//         }
//         int slice = (int)(intptr_t)cur->data[1];
//         if (!blitNv12ToRgba(srcTex, srcW, srcH, slice))
//         {
//             av_frame_free(&cur);
//             return node;
//         }
//     }
//     else if (sdesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
//              sdesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
//         //RGBA -> RGBA，直接拷贝
//         m_ctx->CopyResource(m_rgbaTex.Get(), srcTex);
//         //INFO_LOG<<"Oran7ScreenCapture srcW:"<<srcW<<"-srcH:"<<srcH;
//     }
//     else {
//         // 其他格式先不处理
//         av_frame_free(&cur);
//         return node;
//     }

//     // 只有当 BGRA 纹理重建，才需要创建新的 QSGTexture wrapper 并 setTexture
//     if (recreated || node->texture() == nullptr) {
//         INFO_LOG<<"QSGTexture wrapper recreated.";
//         QSGTexture *tex = QNativeInterface::QSGD3D11Texture::fromNative(
//             m_rgbaTex.Get(), window(), m_bgraSize, QQuickWindow::TextureHasAlphaChannel);

//         if (tex)
//             node->setTexture(tex);
//         else
//             WARNING_LOG<<"QSGTexture ERROR.";
//     }
//     node->setRect(boundingRect());

// #if 0
//     static int frame_count1 = 0;
//     static qint64 t01 = QDateTime::currentMSecsSinceEpoch();
//     frame_count1++;
//     auto now1 = QDateTime::currentMSecsSinceEpoch();
//     if (now1 - t01 >= 1000)//1000ms
//     {
//         INFO_LOG<<"d3d11VideoItem updataPaintNode frameCount:"<<frame_count1<<" per second.";
//         frame_count1 = 0;
//         t01 = now1;
//     }
// #endif

//     //==============  static video info ================//
//     static int frame_count = 0;
//     static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
//     frame_count++;
//     auto now = QDateTime::currentMSecsSinceEpoch();
//     if (now - t0 >= 1000)//1000ms
//     {
//         Oran7VideoInfo info;
//         info.srcSize = QSize(cur->width,cur->height);
//         info.srcFormat = cur->format;
//         info.srcFormatName = QString(av_get_pix_fmt_name(static_cast<AVPixelFormat>(cur->format)));
//         info.renderSize =QSize(boundingRect().width(),boundingRect().height());
//         info.dxgiFormat = sdesc.Format;
//         info.dxgiFormatName =dxgiFormatName(sdesc.Format);
//         info.fps = frame_count;
//         info.isFromHardWare = true;
//         info.decodeDevice = "d3d11va";
//         info.renderDevice = "d3d11va";

//         emit sendVideoFrameInfo(info);

//         frame_count = 0;
//         t0 = now;
//     }

//     av_frame_free(&cur);

//     return node;
// }

QSGNode* D3D11VideoItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<D3D11VideoRenderNode*>(oldNode);
    if (!node) {
        node = new D3D11VideoRenderNode(window());
    }

    m_renderNode = node;

    node->setRect(boundingRect());

    if (m_needClearBlack.exchange(false, std::memory_order_acq_rel))
        node->setNeedBlack(true);

    AVFrame *cur = m_latestFrame.exchange(nullptr, std::memory_order_acq_rel);
    if (cur){
#if 1
    static int count = 0;
    static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    count++;
    auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - t0 >= 1000) {
        INFO_LOG << "setFrame fps =" << count;
        count = 0;
        t0 = now;
    }
#endif
        node->setFrame(cur);
        m_updatePending.store(false, std::memory_order_release);
    }

    int w = 0, h = 0;
    if (node->takePendingSourceSizeChanged(w, h))
        emit sourceSizeChanged(w, h);

    //==============  static video info ================//
    // static int frame_count = 0;
    // static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    // frame_count++;
    // auto now = QDateTime::currentMSecsSinceEpoch();
    // if (now - t0 >= 1000)//1000ms
    // {
    //     Oran7VideoInfo info;
    //     info.srcSize = QSize(cur->width,cur->height);
    //     info.srcFormat = cur->format;
    //     info.srcFormatName = QString(av_get_pix_fmt_name(static_cast<AVPixelFormat>(cur->format)));
    //     info.renderSize =QSize(boundingRect().width(),boundingRect().height());
    //     node->takePendingDxgifromat(info.dxgiFormat);
    //     info.dxgiFormatName =dxgiFormatName(info.dxgiFormat);
    //     info.fps = frame_count;
    //     info.isFromHardWare = true;
    //     info.decodeDevice = "d3d11va";
    //     info.renderDevice = "d3d11va";

    //     emit sendVideoFrameInfo(info);

    //     frame_count = 0;
    //     t0 = now;
    // }

    #if 0
    static int count = 0;
    static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    count++;
    auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - t0 >= 1000) {
        INFO_LOG << "updatePaintNode fps =" << count;
        count = 0;
        t0 = now;
    }
    #endif

    return node;
}




