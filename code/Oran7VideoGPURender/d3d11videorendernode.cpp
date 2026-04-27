#include "D3D11VideoRenderNode.h"
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QSGRendererInterface>

#include "globalhelper.h"

D3D11VideoRenderNode::D3D11VideoRenderNode(QQuickWindow *w)
    : m_window(w)
{

}

void D3D11VideoRenderNode::setFrame(AVFrame *f)
{
    AVFrame *old = m_latestFrame.exchange(f, std::memory_order_acq_rel);
    if (old)
        av_frame_free(&old);
}

void D3D11VideoRenderNode::setRect(const QRectF &rect)
{
    m_rect = rect;
    m_verticesDirty = true;   //quad 顶点要跟着尺寸变
}

void D3D11VideoRenderNode::setNeedBlack(bool on)
{
    m_needBlack = on;
}

bool D3D11VideoRenderNode::takePendingSourceSizeChanged(int &w, int &h)
{
    if (!m_hasPendingSizeChanged)
        return false;

    w = m_pendingW;
    h = m_pendingH;
    m_hasPendingSizeChanged = false;
    return true;
}

bool D3D11VideoRenderNode::takePendingDxgifromat(DXGI_FORMAT &fmt)
{
    if(!m_hasPendingDXGIFormatChanged)
        return false;

    fmt = m_pendingDxgiFmt;
    m_hasPendingDXGIFormatChanged = false;

    return true;
}


bool D3D11VideoRenderNode::initD3D11Resources()
{
    if (m_dev)
        return true;
    if (!m_window)
        return false;

    QSGRendererInterface *ri = m_window->rendererInterface();
    if (!ri)
        return false;

    auto *dev = static_cast<ID3D11Device *>(
        ri->getResource(m_window, QSGRendererInterface::DeviceResource));
    if (!dev)
        return false;

    m_dev = dev;
    m_dev->GetImmediateContext(&m_ctx);
    m_dev.As(&m_videoDev);
    m_ctx.As(&m_videoCtx);

    return m_ctx && m_videoDev && m_videoCtx;
}

bool D3D11VideoRenderNode::ensureRgbaTarget(int w, int h, DXGI_FORMAT fmt)
{
    if (!m_dev || w <= 0 || h <= 0)
        return false;

    bool recreated = false;

    if (m_rgbaTex) {
        D3D11_TEXTURE2D_DESC oldDesc = {};
        m_rgbaTex->GetDesc(&oldDesc);
        // INFO_LOG << "rgbaTex fmt=" << int( oldDesc.Format)
        //          << " size=" <<  oldDesc.Width << "x" <<  oldDesc.Height;
        if (int(oldDesc.Width) ==    w &&
            int(oldDesc.Height) == h &&
            oldDesc.Format == fmt) {
            return true;
        }
        m_rgbaTex.Reset();
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = UINT(w);
    desc.Height = UINT(h);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = fmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    HRESULT hr = m_dev->CreateTexture2D(&desc, nullptr, m_rgbaTex.GetAddressOf());
    if (FAILED(hr) || !m_rgbaTex) {
        WARNING_LOG << "CreateTexture2D failed hr="
                    << QString::number(quint32(hr), 16).toStdString();
        return false;
    }

    m_importDirty = true; // 重新导入 QRhiTexture
    return true;
}


bool D3D11VideoRenderNode::ensureVideoProcessor(int srcW, int srcH)
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

bool D3D11VideoRenderNode::blitNv12ToRgba(ID3D11Texture2D *srcTex, int srcW, int srcH, int slice)
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

void D3D11VideoRenderNode::clearBlack()
{
    if (!m_dev || !m_ctx)
        return;

    if (!m_rgbaTex) {
        const int w = int(m_rect.width());
        const int h = int(m_rect.height());
        if (w <= 0 || h <= 0)
            return;
        if (!ensureRgbaTarget(w, h, DXGI_FORMAT_B8G8R8A8_UNORM))
            return;
    }

    ComPtr<ID3D11RenderTargetView> rtv;
    D3D11_RENDER_TARGET_VIEW_DESC desc = {};
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = 0;

    HRESULT hr = m_dev->CreateRenderTargetView(
        m_rgbaTex.Get(), &desc, rtv.GetAddressOf());
    if (FAILED(hr) || !rtv)
        return;

    const float color[4] = { 0.f, 0.f, 0.f, 0.f };//black
    m_ctx->ClearRenderTargetView(rtv.Get(), color);
    m_ctx->Flush();

    INFO_LOG<<"New Render black;";

    m_importDirty = true;
    m_srbDirty = true;
}


void D3D11VideoRenderNode::prepare()
{
    AVFrame *cur = m_latestFrame.exchange(nullptr, std::memory_order_acq_rel);

    const bool needBlack = m_needBlack.exchange(false, std::memory_order_acq_rel);

    if (!m_dev) {
        if (!initD3D11Resources()) {
            if (cur) av_frame_free(&cur);
            return;
        }
    }

    if (needBlack) {
        if (cur) {
            av_frame_free(&cur);
            cur = nullptr;
        }
        clearBlack();
        ensureImportedTexture();
        return;
    }

#if 0
    static int count = 0;
    static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    count++;
    auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - t0 >= 1000) {
        INFO_LOG << "prepare fps =" << count;
        count = 0;
        t0 = now;
    }
#endif

    if (!cur){
#if 0
        static int count = 0;
        static qint64 t0 = QDateTime::currentMSecsSinceEpoch();
        count++;
        auto now = QDateTime::currentMSecsSinceEpoch();
        if (now - t0 >= 1000) {
            INFO_LOG << "drop fps =" << count;
            count = 0;
            t0 = now;
        }
#endif
        return;
    }

    auto *srcTex = reinterpret_cast<ID3D11Texture2D *>(cur->data[0]);
    if (!srcTex) {
        av_frame_free(&cur);
        return;
    }

    D3D11_TEXTURE2D_DESC sdesc{};
    srcTex->GetDesc(&sdesc);

    const int srcW = int(sdesc.Width);
    const int srcH = int(sdesc.Height);

    if (srcW != m_lastSrcW || srcH != m_lastSrcH) {
        m_lastSrcW = srcW;
        m_lastSrcH = srcH;
        m_pendingW = srcW;
        m_pendingH = srcH;
        m_hasPendingSizeChanged = true;
    }
    if(sdesc.Format != m_pendingDxgiFmt)
    {
        m_pendingDxgiFmt = sdesc.Format;
        m_hasPendingDXGIFormatChanged = true;
        INFO_LOG<<"DXGI_FORMAT_NAME:"<<dxgiFormatName(sdesc.Format);
    }

    ensureRgbaTarget(srcW, srcH, DXGI_FORMAT_B8G8R8A8_UNORM);

    if (sdesc.Format == DXGI_FORMAT_NV12 || sdesc.Format == DXGI_FORMAT_P010) {
        if (!m_vp || !m_vpEnum) {
            if (!ensureVideoProcessor(srcW, srcH)) {
                av_frame_free(&cur);
                return;
            }
        }
        const int slice = int(reinterpret_cast<intptr_t>(cur->data[1]));
        blitNv12ToRgba(srcTex, srcW, srcH, slice);
    } else if (sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
               sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        m_ctx->CopyResource(m_rgbaTex.Get(), srcTex);
    }

    av_frame_free(&cur);
}

void D3D11VideoRenderNode::render(const RenderState *)
{
    if (!renderTarget() || !commandBuffer())
        return;

    if (!ensureImportedTexture())
        return;
    // if (!createDebugGreenTexture())
    //     return;

    createSampler();
    createBuffers();

    if (!m_sampler || !m_vbuf || !m_ubuf || !m_rhiTex)
        return;

    uploadStaticVerticesIfNeeded();
    rebuildSrbAndPipelineIfNeeded();
    updateUniforms();

    if (!m_ps || !m_srb)
        return;

    QRhiCommandBuffer *cb = commandBuffer();
    QRhiRenderTarget *rt = renderTarget();

    cb->setGraphicsPipeline(m_ps.get());
    cb->setViewport(QRhiViewport(0, 0,
                                 rt->pixelSize().width(),
                                 rt->pixelSize().height()));
    cb->setShaderResources(m_srb.get());

    const QRhiCommandBuffer::VertexInput vb(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vb);
    cb->draw(4);
}

// void D3D11VideoRenderNode::prepare()
// {
//     AVFrame* cur = m_latestFrame.exchange(nullptr, std::memory_order_acq_rel);
//     if (!cur)
//         return;

//     auto* srcTex = reinterpret_cast<ID3D11Texture2D*>(cur->data[0]);
//     if (!srcTex) {
//         av_frame_free(&cur);
//         return;
//     }

//     // 如果纹理还没 wrap，或者源 texture 改变了，创建 QRhiTexture
//     if (!m_rhiTex || m_lastImportedTex != srcTex) {
//         QRhiTexture::NativeTexture nt{};
//         nt.object = quint64(srcTex);

//         const QSize size(cur->width, cur->height);
//         m_rhiTex.reset(m_rhi->newTexture(QRhiTexture::BGRA8, size, 1, QRhiTexture::Flags{}));
//         if (!m_rhiTex->createFrom(nt)) {
//             m_rhiTex.reset();
//             m_lastImportedTex = nullptr;
//             av_frame_free(&cur);
//             return;
//         }

//         m_lastImportedTex = srcTex;
//         m_srbDirty = true;
//     }

//     av_frame_free(&cur);
// }

// void D3D11VideoRenderNode::render(const RenderState*)
// {
//     if (!m_rhiTex || !m_ps || !m_srb)
//         return;

//     QRhiCommandBuffer* cb = commandBuffer();
//     cb->setGraphicsPipeline(m_ps.get());
//     cb->setViewport(QRhiViewport(0, 0,
//                                  renderTarget()->pixelSize().width(),
//                                  renderTarget()->pixelSize().height()));
//     cb->setShaderResources(m_srb.get());

//     const QRhiCommandBuffer::VertexInput vb(m_vbuf.get(), 0);
//     cb->setVertexInput(0, 1, &vb);
//     cb->draw(4);
// }

void D3D11VideoRenderNode::releaseResources()
{
    doReleaseResources();
}

void D3D11VideoRenderNode::doReleaseResources()
{
    //释放待处理帧，避免悬挂 AVFrame / 纹理引用
    AVFrame *frame = m_latestFrame.exchange(nullptr, std::memory_order_acq_rel);
    if (frame) {
        av_frame_free(&frame);
    }

    //先释放 QRhi 资源
    //    顺序上先 pipeline / srb / buffers / sampler / texture
    m_ps.reset();
    m_srb.reset();
    m_vbuf.reset();
    m_ubuf.reset();
    m_sampler.reset();
    m_rhiTex.reset();

    //释放 D3D11 资源
    m_vp.Reset();
    m_vpEnum.Reset();
    m_rgbaTex.Reset();
    m_videoCtx.Reset();
    m_videoDev.Reset();
    m_ctx.Reset();
    m_dev.Reset();

    //清空非 owning 指针 / 状态，确保后续可重建
    m_rhi = nullptr;
    m_lastImportedTex = nullptr;

    m_rhiInited = false;
    m_pipelineDirty = true;
    m_verticesDirty = true;
    m_importDirty = true;
    m_srbDirty = true;

    m_targetFmt = DXGI_FORMAT_UNKNOWN;
    m_lastDxgiFormat = DXGI_FORMAT_UNKNOWN;
    m_bgraSize = QSize();

    m_lastSrcW = 0;
    m_lastSrcH = 0;

    m_hasPendingSizeChanged = false;
    m_pendingW = 0;
    m_pendingH = 0;

    m_needBlack.store(false, std::memory_order_release);
}


QSGRenderNode::RenderingFlags D3D11VideoRenderNode::flags() const
{
    return QSGRenderNode::BoundedRectRendering
           | QSGRenderNode::NoExternalRendering;
}

QSGRenderNode::StateFlags D3D11VideoRenderNode::changedStates() const
{
    return QSGRenderNode::ViewportState | QSGRenderNode::ScissorState;
}

QRectF D3D11VideoRenderNode::rect() const
{
    return m_rect;
}

bool D3D11VideoRenderNode::ensureImportedTexture()
{
    if (!m_rhi) {
        auto *ri = m_window->rendererInterface();
        m_rhi = static_cast<QRhi *>(
            ri->getResource(m_window, QSGRendererInterface::RhiResource));
        //m_rhi = m_window ? m_window->rhi() : nullptr;
        if (!m_rhi)
            return false;
    }

    if (!m_rgbaTex)
        return false;

    D3D11_TEXTURE2D_DESC desc{};
    m_rgbaTex->GetDesc(&desc);

    const QSize size(int(desc.Width), int(desc.Height));
    const DXGI_FORMAT dxgi = desc.Format;

    QRhiTexture::Format fmt = QRhiTexture::UnknownFormat;
    switch (dxgi) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        fmt = QRhiTexture::RGBA8;
        break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        fmt = QRhiTexture::BGRA8;
        break;
    default:
        return false;
    }

    const bool needReimport =
        m_importDirty ||
        !m_rhiTex ||
        m_lastImportedTex != m_rgbaTex.Get() ||
        m_lastDxgiFormat != dxgi ||
        (m_rhiTex && m_rhiTex->pixelSize() != size);

    if (!needReimport)
        return true;

    m_rhiTex.reset(
        m_rhi->newTexture(fmt, size, 1, QRhiTexture::Flags{}));
    if (!m_rhiTex)
        return false;

    QRhiTexture::NativeTexture nt{};
    nt.object = quint64(m_rgbaTex.Get());

    if (!m_rhiTex->createFrom(nt)) {
        m_rhiTex.reset();
        WARNING_LOG<<"Failed to m_rhiTex->createFrom(nt).";
        return false;
    }
    INFO_LOG<<"m_rhiTex->nativeTexture().layout:"<<m_rhiTex->nativeTexture().layout;
    INFO_LOG<<"m_rhiTex->nativeTexture().object:"<<m_rhiTex->nativeTexture().object;
    INFO_LOG<<"m_rhiTex->pixelSize():"<<m_rhiTex->pixelSize();
    m_srbDirty = true;

    m_lastImportedTex = m_rgbaTex.Get();
    m_lastDxgiFormat = dxgi;
    m_importDirty = false;

    rebuildSrbAndPipelineIfNeeded(); // 纹理对象换了，SRB 也要重绑
    return true;
}

void D3D11VideoRenderNode::createSampler()
{
    if (m_sampler)
        return;

    m_sampler.reset(
        m_rhi->newSampler(QRhiSampler::Linear,
                          QRhiSampler::Linear,
                          QRhiSampler::None,
                          QRhiSampler::ClampToEdge,
                          QRhiSampler::ClampToEdge));

    if (!m_sampler->create()) {
        m_sampler.reset();
        WARNING_LOG << "create sampler failed";
    }
    m_srbDirty = true;
}

void D3D11VideoRenderNode::createBuffers()
{
    if (!m_vbuf) {
        m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable,
                                      QRhiBuffer::VertexBuffer,
                                      sizeof(Vertex) * 4));
        if (!m_vbuf->create()) {
            m_vbuf.reset();
            WARNING_LOG << "create vertex buffer failed";
            return;
        }
        m_verticesDirty = true;
    }

    if (!m_ubuf) {
        m_ubuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                      QRhiBuffer::UniformBuffer,
                                      sizeof(UbufData)));
        if (!m_ubuf->create()) {
            m_ubuf.reset();
            WARNING_LOG << "create uniform buffer failed";
            return;
        }
    }
}


void D3D11VideoRenderNode::uploadStaticVerticesIfNeeded()
{
    if (!m_verticesDirty || !m_vbuf)
        return;

    Vertex quad[4] = {
        { 0.0f,                 0.0f,                  0.0f, 0.0f },
        { 0.0f,                 float(m_rect.height()), 0.0f, 1.0f },
        { float(m_rect.width()), 0.0f,                  1.0f, 0.0f },
        { float(m_rect.width()), float(m_rect.height()), 1.0f, 1.0f }
    };

    // Vertex quad[4] = {
    //     { -1.0f, -1.0f, 0.0f, 1.0f },
    //     { -1.0f,  1.0f, 0.0f, 0.0f },
    //     {  1.0f, -1.0f, 1.0f, 1.0f },
    //     {  1.0f,  1.0f, 1.0f, 0.0f }
    // };


    QRhiResourceUpdateBatch *rub = m_rhi->nextResourceUpdateBatch();
    rub->uploadStaticBuffer(m_vbuf.get(), 0, sizeof(quad), quad);
    commandBuffer()->resourceUpdate(rub);

    m_verticesDirty = false;
}

void D3D11VideoRenderNode::createPipeline()
{
    if (!m_rhi || !renderTarget() || !m_srb)
        return;

    m_ps.reset(m_rhi->newGraphicsPipeline());

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(Vertex) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, offsetof(Vertex, x) },
        { 0, 1, QRhiVertexInputAttribute::Float2, offsetof(Vertex, u) }
    });

    const QShader vs = loadShader(":/shaders/video.vert.qsb");
    const QShader fs = loadShader(":/shaders/video.frag.qsb");
    if (!vs.isValid() || !fs.isValid()) {
        WARNING_LOG << "Shader load failed.";
        m_ps.reset();
        return;
    }

    m_ps->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_ps->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs }
    });
    m_ps->setVertexInputLayout(inputLayout);
    m_ps->setShaderResourceBindings(m_srb.get());
    m_ps->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_ps->setTargetBlends({ blend });

    if (!m_ps->create()) {
        WARNING_LOG << "create graphics pipeline failed";
        m_ps.reset();
        return;
    }

    m_pipelineDirty = false;
}


void D3D11VideoRenderNode::updateUniforms()
{
    if (!m_ubuf)
        return;

    UbufData ubuf;
    ubuf.mvp = (*projectionMatrix()) * (*matrix());
    ubuf.opacity = float(inheritedOpacity());

    QRhiResourceUpdateBatch *rub = m_rhi->nextResourceUpdateBatch();
    rub->updateDynamicBuffer(m_ubuf.get(), 0, sizeof(UbufData), &ubuf);
    commandBuffer()->resourceUpdate(rub);
}

void D3D11VideoRenderNode::rebuildSrbAndPipelineIfNeeded()
{
    if (!m_rhi || !m_rhiTex || !m_sampler || !m_ubuf)
        return;

    if (m_srbDirty || !m_srb) {
        m_srb.reset(m_rhi->newShaderResourceBindings());
        m_srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                m_ubuf.get()),
            QRhiShaderResourceBinding::sampledTexture(
                1,
                QRhiShaderResourceBinding::FragmentStage,
                m_rhiTex.get(),
                m_sampler.get())
        });

        if (!m_srb->create()) {
            WARNING_LOG << "create SRB failed";
            m_srb.reset();
            return;
        }

        m_srbDirty = false;
        m_pipelineDirty = true; // 如果 pipeline 当前还没绑定这个 srb，就重建
    }

    if (m_pipelineDirty || !m_ps) {
        createPipeline();
    }
}

bool D3D11VideoRenderNode::createDebugGreenTexture()
{
    if (!m_rhi) {
        m_rhi = m_window ? m_window->rhi() : nullptr;
        if (!m_rhi)
            return false;
    }

    const QSize size(2, 2);

    m_rhiTex.reset(m_rhi->newTexture(QRhiTexture::RGBA8, size, 1));
    if (!m_rhiTex || !m_rhiTex->create())
        return false;

    QImage img(size, QImage::Format_RGBA8888);
    img.fill(QColor(0, 255, 0, 255));

    QRhiResourceUpdateBatch *rub = m_rhi->nextResourceUpdateBatch();
    rub->uploadTexture(m_rhiTex.get(), img);
    commandBuffer()->resourceUpdate(rub);

    m_srbDirty = true;
    m_pipelineDirty = true;
    return true;
}


