#include "D3d11VideoRenderNode.h"
#include <QFile>
#include <cstring>
#include <QSGRendererInterface>
#include <dxgi1_2.h>
extern "C"
{
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/pixdesc.h>
}

namespace
{
QShader loadShader(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader{};
}
// FFmpeg 同设备帧遵守其递归上下文锁；跨设备捕获帧遵守 keyed mutex。
struct Oran7DecodeContextGuard
{
    AVD3D11VADeviceContext* context = nullptr;
    explicit Oran7DecodeContextGuard(AVFrame* frame)
    {
        if (frame->hw_frames_ctx)
        {
            auto* frames = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
            context = reinterpret_cast<AVD3D11VADeviceContext*>(frames->device_ctx->hwctx);
            if (context && context->lock)
                context->lock(context->lock_ctx);
        }
    }
    ~Oran7DecodeContextGuard()
    {
        if (context && context->unlock)
            context->unlock(context->lock_ctx);
    }
};
struct Oran7KeyedReadGuard
{
    ComPtr<IDXGIKeyedMutex> mutex;
    bool acquired = false;
    ~Oran7KeyedReadGuard()
    {
        if (acquired)
            mutex->ReleaseSync(0);
    }
};
} // namespace
D3D11VideoRenderNode::D3D11VideoRenderNode(QQuickWindow* window, std::shared_ptr<Oran7VideoFrameMailbox> mailbox)
    : m_window(window), m_mailbox(std::move(mailbox))
{
    // 这里只推进 Qt 帧编号，不消费媒体帧。离屏/主画面各 pass 共用本次准备结果。
    m_frameConnection = QObject::connect(
        window, &QQuickWindow::beforeRendering, window, [this] { ++m_frameSerial; }, Qt::DirectConnection);
}
D3D11VideoRenderNode::~D3D11VideoRenderNode()
{
    QObject::disconnect(m_frameConnection);
    doReleaseResources();
}
void D3D11VideoRenderNode::setRect(const QRectF& rect)
{
    if (m_rect != rect)
    {
        m_rect = rect;
        m_verticesDirty = true;
    }
}
void D3D11VideoRenderNode::setContentRect(const QRectF& rect)
{
    if (m_contentRect != rect)
    {
        m_contentRect = rect;
        m_verticesDirty = true;
    }
}
bool D3D11VideoRenderNode::initD3D11Resources()
{
    if (m_dev)
        return true;
    if (!m_window)
        return false;

    QSGRendererInterface* ri = m_window->rendererInterface();
    if (!ri)
        return false;

    auto* dev = static_cast<ID3D11Device*>(ri->getResource(m_window, QSGRendererInterface::DeviceResource));
    if (!dev)
        return false;

    m_dev = dev;
    m_dev->GetImmediateContext(&m_ctx);
    m_dev.As(&m_videoDev);
    m_ctx.As(&m_videoCtx);

    return bool(m_ctx);
}

bool D3D11VideoRenderNode::ensureRgbaTarget(int w, int h, DXGI_FORMAT fmt)
{
    if (!m_dev || w <= 0 || h <= 0)
        return false;

    if (m_rgbaTex)
    {
        D3D11_TEXTURE2D_DESC oldDesc = {};
        m_rgbaTex->GetDesc(&oldDesc);
        // INFO_LOG << "rgbaTex fmt=" << int( oldDesc.Format)
        //          << " size=" <<  oldDesc.Width << "x" <<  oldDesc.Height;
        if (int(oldDesc.Width) == w && int(oldDesc.Height) == h && oldDesc.Format == fmt)
        {
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
    if (FAILED(hr) || !m_rgbaTex)
    {
        WARNING_LOG << "CreateTexture2D failed hr=" << QString::number(quint32(hr), 16).toStdString();
        return false;
    }

    m_importDirty = true; // 重新导入 QRhiTexture
    return true;
}

bool D3D11VideoRenderNode::ensureVideoProcessor(int srcW, int srcH)
{
    if (!m_videoDev || !m_videoCtx)
        return false;
    if (m_vp && m_processorSize == QSize(srcW, srcH))
        return true;
    m_vp.Reset();
    m_vpEnum.Reset();
    m_processorSize = QSize(srcW, srcH);

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC c{};
    c.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    c.InputWidth = srcW;
    c.InputHeight = srcH;
    c.OutputWidth = srcW;
    c.OutputHeight = srcH;
    c.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr = m_videoDev->CreateVideoProcessorEnumerator(&c, &m_vpEnum);
    if (FAILED(hr))
        return false;

    hr = m_videoDev->CreateVideoProcessor(m_vpEnum.Get(), 0, &m_vp);
    return SUCCEEDED(hr);
}

bool D3D11VideoRenderNode::blitNv12ToRgba(ID3D11Texture2D* srcTex, int srcW, int srcH, int slice)
{
    if (!srcTex || !m_vp || !m_vpEnum || !m_videoCtx)
        return false;

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
    inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inDesc.Texture2D.ArraySlice = slice;

    ComPtr<ID3D11VideoProcessorInputView> inView;
    HRESULT hr = m_videoDev->CreateVideoProcessorInputView(srcTex, m_vpEnum.Get(), &inDesc, &inView);
    if (FAILED(hr))
        return false;

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{};
    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outDesc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11VideoProcessorOutputView> outView;
    hr = m_videoDev->CreateVideoProcessorOutputView(m_rgbaTex.Get(), m_vpEnum.Get(), &outDesc, &outView);
    if (FAILED(hr))
    {
        WARNING_LOG << "CreateVPOutputView failed hr=" << hr;
        return false;
    }

    RECT srcRect{0, 0, srcW, srcH};
    RECT dstRect{0, 0, srcW, srcH};

    m_videoCtx->VideoProcessorSetStreamSourceRect(m_vp.Get(), 0, TRUE, &srcRect);
    m_videoCtx->VideoProcessorSetStreamDestRect(m_vp.Get(), 0, TRUE, &dstRect);

    // ===== 色彩空间设置（修复饱和度偏低的问题）=====
    // 输入流色彩空间：BT.709 + Limited Range (16-235)，这是网络直播流的标准
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE inputStreamColor{};
    inputStreamColor.Usage = 0;         // 播放（非录制）
    inputStreamColor.RGB_Range = 0;     // 不适用（输入是YUV）
    inputStreamColor.YCbCr_Matrix = 1;  // 1 = BT.709（1080p标准）
    inputStreamColor.YCbCr_xvYCC = 0;   // 不使用 xvYCC 扩展
    inputStreamColor.Nominal_Range = 1; // 1 = Limited Range (16-235)
    m_videoCtx->VideoProcessorSetStreamColorSpace(m_vp.Get(), 0, &inputStreamColor);

    // 输出色彩空间：BT.709 + Full Range (0-255)，匹配显示器 sRGB
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE outputColorSpace{};
    outputColorSpace.Usage = 0;
    outputColorSpace.RGB_Range = 0;    // 0 = Full Range (0-255)
    outputColorSpace.YCbCr_Matrix = 1; // BT.709
    outputColorSpace.YCbCr_xvYCC = 0;
    outputColorSpace.Nominal_Range = 0; // 0 = Full Range (0-255)
    m_videoCtx->VideoProcessorSetOutputColorSpace(m_vp.Get(), &outputColorSpace);

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inView.Get();

    hr = m_videoCtx->VideoProcessorBlt(m_vp.Get(), outView.Get(), 0, 1, &stream);
    if (FAILED(hr))
    {
        WARNING_LOG << "VideoProcessorBlt failed hr=" << hr;
        return false;
    }
    return SUCCEEDED(hr);
}

bool D3D11VideoRenderNode::uploadCpuFrameToRgba(AVFrame* cur)
{
    if (!cur || !m_dev || !m_ctx)
        return false;

    const int w = cur->width;
    const int h = cur->height;
    if (w <= 0 || h <= 0)
        return false;

    // 确保 RGBA 目标纹理存在且尺寸匹配
    if (!ensureRgbaTarget(w, h, DXGI_FORMAT_B8G8R8A8_UNORM))
        return false;

    const AVPixelFormat srcFmt = static_cast<AVPixelFormat>(cur->format);

    // sws 上下文缓存：仅在源尺寸或格式变化时重建
    if (!m_swsCtx || m_lastSrcCpuFmt != srcFmt || m_swsSrcW != w || m_swsSrcH != h)
    {
        if (m_swsCtx)
        {
            sws_freeContext(m_swsCtx);
            m_swsCtx = nullptr;
        }

        m_swsCtx = sws_getContext(w, h, srcFmt, w, h, AV_PIX_FMT_BGRA, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        if (!m_swsCtx)
        {
            static bool logged = false;
            if (!logged)
            {
                logged = true;
                WARNING_LOG << "uploadCpuFrameToRgba: sws_getContext failed, srcFmt=" << av_get_pix_fmt_name(srcFmt);
            }
            return false;
        }
        m_lastSrcCpuFmt = srcFmt;
        m_swsSrcW = w;
        m_swsSrcH = h;
        INFO_LOG << "uploadCpuFrameToRgba: created sws ctx, fmt=" << av_get_pix_fmt_name(srcFmt) << " size=" << w << "x"
                 << h;
    }

    // 分配临时 BGRA AVFrame 作为 sws 输出
    AVFrame* bgraFrame = av_frame_alloc();
    if (!bgraFrame)
        return false;

    bgraFrame->format = AV_PIX_FMT_BGRA;
    bgraFrame->width = w;
    bgraFrame->height = h;
    if (av_frame_get_buffer(bgraFrame, 0) < 0)
    {
        av_frame_free(&bgraFrame);
        return false;
    }

    sws_scale(m_swsCtx, cur->data, cur->linesize, 0, h, bgraFrame->data, bgraFrame->linesize);

    // 上载到 D3D11 RGBA 纹理
    // linesize[0] 可能与 w*4 不一致（av_frame_get_buffer 的默认对齐），需要做 pitch 适配
    const int expectedStride = w * 4;
    if (bgraFrame->linesize[0] == expectedStride)
    {
        D3D11_BOX box{};
        box.right = UINT(w);
        box.bottom = UINT(h);
        box.back = 1;
        m_ctx->UpdateSubresource(m_rgbaTex.Get(), 0, &box, bgraFrame->data[0], UINT(bgraFrame->linesize[0]), 0);
    }
    else
    {
        // pitch 不匹配：逐行拷贝到连续缓冲区后再上载
        std::vector<uint8_t> packed(w * h * 4);
        const uint8_t* src = bgraFrame->data[0];
        for (int y = 0; y < h; ++y)
        {
            memcpy(packed.data() + y * expectedStride, src + y * bgraFrame->linesize[0], expectedStride);
        }
        D3D11_BOX box{};
        box.right = UINT(w);
        box.bottom = UINT(h);
        box.back = 1;
        m_ctx->UpdateSubresource(m_rgbaTex.Get(), 0, &box, packed.data(), UINT(expectedStride), 0);
    }

    av_frame_free(&bgraFrame);
    return true;
}

void D3D11VideoRenderNode::clearBlack()
{
    // 清屏是渲染状态，不覆盖仍可能被 Qt 离屏 pass 引用的上一帧纹理。
    m_blackFrame = true;
    if (!m_rgbaTex)
    {
        if (!ensureRgbaTarget(1, 1, DXGI_FORMAT_B8G8R8A8_UNORM))
            return;
        const uint32_t black = 0xff000000;
        m_ctx->UpdateSubresource(m_rgbaTex.Get(), 0, nullptr, &black, sizeof(black), 0);
    }
}

bool D3D11VideoRenderNode::uploadHardwareFrame(AVFrame* frame)
{
    auto* source = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
    if (!source)
        return false;
    Oran7DecodeContextGuard decodeGuard(frame);
    ComPtr<ID3D11Device> sourceDevice;
    source->GetDevice(&sourceDevice);
    ComPtr<ID3D11Texture2D> opened;
    Oran7KeyedReadGuard readGuard;
    if (sourceDevice.Get() != m_dev.Get())
    {
        ComPtr<IDXGIResource> shared;
        HANDLE handle = nullptr;
        if (FAILED(source->QueryInterface(IID_PPV_ARGS(&shared))) || FAILED(shared->GetSharedHandle(&handle)) ||
            !handle || FAILED(m_dev->OpenSharedResource(handle, IID_PPV_ARGS(&opened))))
            return false;
        source = opened.Get();
    }
    // 普通解码纹理没有 keyed mutex；采集共享纹理必须成功获得读权限才可复制。
    if (SUCCEEDED(source->QueryInterface(IID_PPV_ARGS(&readGuard.mutex))))
    {
        const auto result = readGuard.mutex->AcquireSync(0, 0);
        readGuard.acquired = result == S_OK;
        m_sharedTextureBusy = result == WAIT_TIMEOUT;
        if (!readGuard.acquired)
            return false;
    }
    else if (opened)
    {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    const auto slice = UINT(reinterpret_cast<uintptr_t>(frame->data[1]));
    if (frame->width <= 0 || frame->height <= 0 || UINT(frame->width) > desc.Width ||
        UINT(frame->height) > desc.Height || slice >= desc.ArraySize)
        return false;
    if (!ensureRgbaTarget(frame->width, frame->height, DXGI_FORMAT_B8G8R8A8_UNORM))
        return false;
    bool uploaded = false;
    if (desc.Format == DXGI_FORMAT_NV12 || desc.Format == DXGI_FORMAT_P010)
    {
        uploaded = ensureVideoProcessor(int(desc.Width), int(desc.Height)) &&
                   blitNv12ToRgba(source, frame->width, frame->height, int(slice));
    }
    else if (desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
    {
        const D3D11_BOX box{0, 0, 0, UINT(frame->width), UINT(frame->height), 1};
        m_ctx->CopySubresourceRegion(m_rgbaTex.Get(), 0, 0, 0, 0, source,
                                     D3D11CalcSubresource(0, slice, desc.MipLevels), &box);
        uploaded = true;
    }
    if (uploaded)
        m_currentDxgiFormat = desc.Format;
    return uploaded;
}

void D3D11VideoRenderNode::consumeFrame()
{
    if (m_consumedSerial == m_frameSerial)
        return;
    m_consumedSerial = m_frameSerial;
    auto packet = m_mailbox->take();
    if (packet.clear)
        clearBlack();
    if (!packet.frame)
        return;
    auto* frame = packet.frame.get();
    const bool hardware = frame->format == AV_PIX_FMT_D3D11;
    m_sharedTextureBusy = false;
    const bool uploaded = hardware ? uploadHardwareFrame(frame) : uploadCpuFrameToRgba(frame);
    if (!uploaded)
    {
        if (!m_uploadFailureLogged && !m_sharedTextureBusy)
        {
            m_uploadFailureLogged = true;
            WARNING_LOG << "QRhi video frame upload skipped; format=" << frame->format;
        }
        return;
    }
    m_blackFrame = false;
    if (!hardware)
        m_currentDxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    // 元数据仅在转换成功后整体发布，GUI 不读取节点/GPU 的可变字段。
    Oran7VideoInfo info;
    info.srcSize = QSize(frame->width, frame->height);
    info.srcFormat = frame->format;
    info.srcFormatName = QString::fromLatin1(av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format)));
    info.renderSize = m_rect.size().toSize();
    info.dxgiFormat = m_currentDxgiFormat;
    info.dxgiFormatName = dxgiFormatName(info.dxgiFormat);
    info.fps = packet.fps;
    info.isFromHardWare = hardware;
    info.decodeDevice = hardware ? QStringLiteral("d3d11va") : QStringLiteral("cpu");
    info.renderDevice = QStringLiteral("d3d11rhi");
    m_mailbox->publish(packet.epoch, info);
}

bool D3D11VideoRenderNode::ensureImportedTexture()
{
    if (!m_rgbaTex || !m_rhi)
        return false;
    if (m_rhiTex && !m_importDirty)
        return true;
    // 只在本 Qt 帧的首个 prepare() 消费帧/更换纹理；后续 pass 不销毁已提交资源。
    m_passes.clear();
    m_activePass = nullptr;
    m_rhiTex.reset();
    D3D11_TEXTURE2D_DESC desc{};
    m_rgbaTex->GetDesc(&desc);
    m_rhiTex.reset(m_rhi->newTexture(QRhiTexture::BGRA8, QSize(int(desc.Width), int(desc.Height)), 1));
    QRhiTexture::NativeTexture native{};
    native.object = quint64(m_rgbaTex.Get());
    if (!m_rhiTex || !m_rhiTex->createFrom(native))
    {
        m_rhiTex.reset();
        return false;
    }
    m_importDirty = false;
    return true;
}

D3D11VideoRenderNode::Oran7RenderPass* D3D11VideoRenderNode::preparePass()
{
    auto* target = renderTarget();
    auto* descriptor = target->renderPassDescriptor();
    for (auto& pass : m_passes)
    {
        if (pass->target == target && pass->samples == target->sampleCount() &&
            pass->descriptor->isCompatible(descriptor))
            return pass.get();
    }
    auto pass = std::make_unique<Oran7RenderPass>();
    pass->target = target;
    pass->samples = target->sampleCount();
    pass->descriptor.reset(descriptor->newCompatibleRenderPassDescriptor());
    pass->uniform.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(UbufData)));
    if (!pass->uniform->create())
        return nullptr;
    pass->bindings.reset(m_rhi->newShaderResourceBindings());
    pass->bindings->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(
             0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, pass->uniform.get()),
         QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_rhiTex.get(),
                                                   m_sampler.get())});
    if (!pass->bindings->create())
        return nullptr;
    const auto vertexShader = loadShader(":/shaders/video.vert.qsb");
    const auto fragmentShader = loadShader(":/shaders/video.frag.qsb");
    if (!vertexShader.isValid() || !fragmentShader.isValid())
        return nullptr;
    pass->pipeline.reset(m_rhi->newGraphicsPipeline());
    auto* pipeline = pass->pipeline.get();
    QRhiVertexInputLayout layout;
    layout.setBindings({{sizeof(Vertex)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, offsetof(Vertex, x)},
                          {0, 1, QRhiVertexInputAttribute::Float2, offsetof(Vertex, u)}});
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pipeline->setShaderStages({{QRhiShaderStage::Vertex, vertexShader}, {QRhiShaderStage::Fragment, fragmentShader}});
    pipeline->setVertexInputLayout(layout);
    pipeline->setShaderResourceBindings(pass->bindings.get());
    pipeline->setRenderPassDescriptor(pass->descriptor.get());
    pipeline->setSampleCount(pass->samples);
    pipeline->setTargetBlends({blend});
    pipeline->setFlags(QRhiGraphicsPipeline::UsesScissor | QRhiGraphicsPipeline::UsesStencilRef);
    // 支持矩形与圆角/stencil 裁剪，引用值在 render() 使用 Qt 提供的当前状态。
    pipeline->setStencilTest(true);
    QRhiGraphicsPipeline::StencilOpState stencil;
    stencil.compareOp = QRhiGraphicsPipeline::Equal;
    pipeline->setStencilFront(stencil);
    pipeline->setStencilBack(stencil);
    pipeline->setStencilWriteMask(0);
    if (!pipeline->create())
        return nullptr;
    auto* result = pass.get();
    m_passes.push_back(std::move(pass));
    return result;
}

void D3D11VideoRenderNode::prepare()
{
    m_activePass = nullptr;
    if (!commandBuffer() || !renderTarget())
        return;
    auto* rhi = m_window ? m_window->rhi() : nullptr;
    if (!rhi || rhi->backend() != QRhi::D3D11)
        return;
    if (m_rhi && m_rhi != rhi)
        doReleaseResources();
    m_rhi = rhi;
    if (!initD3D11Resources())
        return;
    // 在首个 pass 之前限制历史目标缓存，避免页面/离屏纹理反复重建后无限累积。
    if (m_consumedSerial != m_frameSerial && m_passes.size() > 16)
        m_passes.clear();
    consumeFrame();
    if (!ensureImportedTexture())
        return;
    if (!m_sampler)
    {
        m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if (!m_sampler->create())
        {
            m_sampler.reset();
            return;
        }
    }
    if (!m_vbuf)
    {
        m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, sizeof(Vertex) * 4));
        if (!m_vbuf->create())
        {
            m_vbuf.reset();
            return;
        }
        m_verticesDirty = true;
    }
    // 每个目标拥有独立 uniform，主窗口与 ShaderEffectSource 的矩阵不会相互覆盖。
    m_activePass = preparePass();
    if (!m_activePass)
        return;
    auto* updates = m_rhi->nextResourceUpdateBatch();
    if (m_verticesDirty)
    {
        const auto& r = m_contentRect.isEmpty() ? m_rect : m_contentRect;
        const Vertex vertices[4] = {{float(r.left()), float(r.top()), 0, 0},
                                    {float(r.left()), float(r.bottom()), 0, 1},
                                    {float(r.right()), float(r.top()), 1, 0},
                                    {float(r.right()), float(r.bottom()), 1, 1}};
        updates->updateDynamicBuffer(m_vbuf.get(), 0, sizeof(vertices), vertices);
        m_verticesDirty = false;
    }
    UbufData uniform;
    const QMatrix4x4 transform = *projectionMatrix() * *matrix();
    // QMatrix4x4 还带内部标志，不能直接作为 std140 的 mat4 内存布局。
    std::memcpy(uniform.mvp, transform.constData(), sizeof(uniform.mvp));
    uniform.opacity = float(inheritedOpacity());
    uniform.black = m_blackFrame ? 1.0f : 0.0f;
    updates->updateDynamicBuffer(m_activePass->uniform.get(), 0, sizeof(uniform), &uniform);
    commandBuffer()->resourceUpdate(updates);
}

void D3D11VideoRenderNode::render(const RenderState* state)
{
    if (!m_activePass || !commandBuffer() || !renderTarget())
        return;
    auto* cb = commandBuffer();
    const auto size = renderTarget()->pixelSize();
    cb->setGraphicsPipeline(m_activePass->pipeline.get());
    cb->setViewport(QRhiViewport(0, 0, float(size.width()), float(size.height())));
    const QRect scissor = state->scissorEnabled() ? state->scissorRect() : QRect(QPoint(0, 0), size);
    cb->setScissor(QRhiScissor(scissor.x(), scissor.y(), scissor.width(), scissor.height()));
    cb->setStencilRef(state->stencilEnabled() ? state->stencilValue() : 0);
    cb->setShaderResources(m_activePass->bindings.get());
    const QRhiCommandBuffer::VertexInput input(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &input);
    cb->draw(4);
}
void D3D11VideoRenderNode::releaseResources()
{
    doReleaseResources();
}
void D3D11VideoRenderNode::doReleaseResources()
{
    m_activePass = nullptr;
    m_passes.clear();
    m_vbuf.reset();
    m_sampler.reset();
    m_rhiTex.reset();
    sws_freeContext(m_swsCtx);
    m_swsCtx = nullptr;
    m_lastSrcCpuFmt = AV_PIX_FMT_NONE;
    m_swsSrcW = m_swsSrcH = 0;
    m_vp.Reset();
    m_vpEnum.Reset();
    m_processorSize = {};
    m_rgbaTex.Reset();
    m_videoCtx.Reset();
    m_videoDev.Reset();
    m_ctx.Reset();
    m_dev.Reset();
    m_rhi = nullptr;
    m_verticesDirty = true;
    m_importDirty = true;
    m_consumedSerial = 0;
}
QSGRenderNode::RenderingFlags D3D11VideoRenderNode::flags() const
{
    return BoundedRectRendering | NoExternalRendering;
}
QSGRenderNode::StateFlags D3D11VideoRenderNode::changedStates() const
{
    return ViewportState | ScissorState | StencilState;
}
