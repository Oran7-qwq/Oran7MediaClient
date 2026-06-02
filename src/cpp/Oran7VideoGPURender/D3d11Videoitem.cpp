// d3d11videoitem.cpp
#include "D3d11Videoitem.h"

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
    uint64_t seq = m_submitSeq.fetch_add(1, std::memory_order_relaxed) + 1;

    if (m_debugLog.load(std::memory_order_relaxed)) {
        static int submitCount = 0;
        static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
        submitCount++;
        auto now = QDateTime::currentMSecsSinceEpoch();
        if (now - t0 >= 1000) {
            INFO_LOG << "submitFrame fps =" << submitCount << " seq =" << seq;
            submitCount = 0;
            t0 = now;
        }
    }

    // 入队，队列容量 2（实时预览宁可丢旧帧也不积压）
    {
        QMutexLocker locker(&m_frameMutex);
        m_frameQueue.enqueue(frameRef);
        if (m_frameQueue.size() > 2) {
            AVFrame *drop = m_frameQueue.dequeue();
            av_frame_free(&drop);
        }
    }

    // 双驱动：item update 触发 sync → updatePaintNode 兜底显示
    //          window update 触发 beforeRendering → 采集场景低延迟渲染
    auto request = [this] {
        update();
        if (auto *w = window())
            w->update();
    };

    if (QThread::currentThread() == thread()) {
        request();
    } else {
        QMetaObject::invokeMethod(this, request, Qt::QueuedConnection);
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
    if (!m_renderNode)
        return;

    AVFrame *cur = takeLatestQueuedFrame();
    if (!cur)
        return;

    //---------------- 帧间隔抖动检测 ----------------//
    static qint64 lastMs = 0;
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (lastMs != 0) {
        qint64 dt = nowMs - lastMs;
        if (dt > 22 && m_debugLog.load(std::memory_order_relaxed)) {
            INFO_LOG << "render jitter dt =" << dt
                     << "submitSeq =" << m_submitSeq.load(std::memory_order_acquire);
        }
    }
    lastMs = nowMs;

    m_renderNode->setFrame(cur);
    // 通知场景图节点内容已变更，使 ShaderEffectSource 等采集组件能感知到更新
    m_renderNode->markDirty(QSGNode::DirtyMaterial);

    //==============  video info (每秒) ================//
    static int frame_count = 0;
    static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    frame_count++;
    auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - t0 >= 1000) {
        Oran7VideoInfo info;
        info.srcSize = QSize(cur->width, cur->height);
        info.srcFormat = cur->format;
        info.srcFormatName = QString(av_get_pix_fmt_name(static_cast<AVPixelFormat>(cur->format)));
        info.renderSize = QSize(boundingRect().width(), boundingRect().height());
        m_renderNode->takePendingDxgifromat(info.dxgiFormat);
        info.dxgiFormatName = dxgiFormatName(info.dxgiFormat);
        info.fps = frame_count;
        info.isFromHardWare = true;
        info.decodeDevice = "d3d11va";
        info.renderDevice = "d3d11va";

        emit sendVideoFrameInfo(info);

        if (m_debugLog.load(std::memory_order_relaxed)) {
            INFO_LOG << "beforeRendering setFrame fps =" << frame_count
                     << "submitSeq =" << m_submitSeq.load(std::memory_order_acquire);
        }
        frame_count = 0;
        t0 = now;
    }
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

    static QMetaObject::Connection beforeConn1;
    if (beforeConn1)
        disconnect(beforeConn1);
    beforeConn1 = connect(w, &QQuickWindow::beforeRendering,
                         this, &D3D11VideoItem::onBeforeRendering,
                         Qt::DirectConnection);

    static QMetaObject::Connection beforeConn2;
    if (beforeConn2)
        disconnect(beforeConn2);
    beforeConn2 = connect(w, &QQuickWindow::frameSwapped, this, [this](){
        static int count = 0;
        static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
        count++;
        auto now = QDateTime::currentMSecsSinceEpoch();
        if (now - t0 >= 1000 && m_debugLog.load(std::memory_order_relaxed)) {
            INFO_LOG << "frameSwapped fps =" << count;
            count = 0;
            t0 = now;
        }
    });


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

    // ===== 色彩空间设置（修复饱和度偏低的问题）=====
    // 输入流色彩空间：BT.709 + Limited Range (16-235)，网络直播流标准
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE inputStreamColor{};
    inputStreamColor.Usage = 0;
    inputStreamColor.RGB_Range = 0;
    inputStreamColor.YCbCr_Matrix = 1;                    // 1 = BT.709（1080p标准）
    inputStreamColor.YCbCr_xvYCC = 0;
    inputStreamColor.Nominal_Range = 1;                    // 1 = Limited Range (16-235)
    m_videoCtx->VideoProcessorSetStreamColorSpace(m_vp.Get(), 0, &inputStreamColor);

    // 输出色彩空间：BT.709 + Full Range (0-255)，匹配显示器 sRGB
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE outputColorSpace{};
    outputColorSpace.Usage = 0;
    outputColorSpace.RGB_Range = 0;                       // 0 = Full Range (0-255)
    outputColorSpace.YCbCr_Matrix = 1;                    // BT.709
    outputColorSpace.YCbCr_xvYCC = 0;
    outputColorSpace.Nominal_Range = 0;                    // 0 = Full Range (0-255)
    m_videoCtx->VideoProcessorSetOutputColorSpace(m_vp.Get(), &outputColorSpace);

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inView.Get();

    hr = m_videoCtx->VideoProcessorBlt(m_vp.Get(), outView.Get(), 0, 1, &stream);
    if (FAILED(hr)) { WARNING_LOG << "VideoProcessorBlt failed hr="<<hr; return false; }
    return SUCCEEDED(hr);
}

QSGNode* D3D11VideoItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<D3D11VideoRenderNode*>(oldNode);
    if (!node) {
        node = new D3D11VideoRenderNode(window());
    }

    m_renderNode = node;

    node->setRect(boundingRect());
    node->setContentRect(m_contentRect);

    if (m_needClearBlack.exchange(false, std::memory_order_acq_rel))
        node->setNeedBlack(true);

    // 兜底：播放器场景 beforeRendering 可能未触发，这里直接取帧渲染
    AVFrame *cur = takeLatestQueuedFrame();
    if (cur) {
        node->setFrame(cur);
        // 通知场景图节点内容已变更，使 ShaderEffectSource 等采集组件能感知到更新
        node->markDirty(QSGNode::DirtyMaterial);
    }

    int w = 0, h = 0;
    if (node->takePendingSourceSizeChanged(w, h))
        emit sourceSizeChanged(w, h);

    return node;
}

AVFrame* D3D11VideoItem::takeLatestQueuedFrame()
{
    AVFrame *cur = nullptr;

    QMutexLocker locker(&m_frameMutex);
    while (!m_frameQueue.isEmpty()) {
        AVFrame *f = m_frameQueue.dequeue();
        if (cur)
            av_frame_free(&cur);   // 丢旧帧
        cur = f;                    // 留最新
    }

    return cur;
}




