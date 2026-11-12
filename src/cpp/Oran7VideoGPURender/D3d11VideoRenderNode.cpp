#include "D3d11VideoRenderNode.h"
#include <QFile>
#include <cstring>
#include <QSGRendererInterface>
#include <dxgi1_2.h>
#include <qfloat16.h>
#include "Oran7HdrWindowOutput.h"
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
bool importTexture(std::unique_ptr<QRhiTexture>& wrapper, ID3D11Texture2D* texture)
{
    // Qt 6.7.3 的失败分支仍可能拥有 native 指针；额外引用防止失败销毁借用纹理。
    texture->AddRef();
    const bool created = wrapper && wrapper->createFrom({quint64(texture), 0});
    if (created || !wrapper || !wrapper->nativeTexture().object)
        texture->Release();
    if (!created)
        wrapper.reset();
    return created;
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
    m_releaseSerial = m_mailbox->releaseSerial();
    m_releaseConnection = QObject::connect(window, &QQuickWindow::afterFrameEnd, window, [this] {
        const auto serial = m_mailbox->releaseSerial();
        if (serial == m_releaseSerial) return;
        // 停止后页面即使隐藏、不再 prepare，也不能长期占住整段视频的 CPU/GPU 缓冲。
        // 必须等本帧命令已提交；在 afterRendering 释放会破坏仍未执行的多 pass 命令。
        doReleaseResources();
        m_blackFrame = true;
        m_sourceSize = {};
        m_releaseSerial = serial;
    }, Qt::DirectConnection);
}
D3D11VideoRenderNode::~D3D11VideoRenderNode()
{
    QObject::disconnect(m_frameConnection);
    QObject::disconnect(m_releaseConnection);
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
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
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

bool D3D11VideoRenderNode::uploadCpuFrameToRgba(AVFrame* cur, bool rgbFallback)
{
    if (!cur || !m_dev || !m_ctx || cur->width <= 0 || cur->height <= 0)
        return false;
    const int w = cur->width, h = cur->height;
    const auto source = AVPixelFormat(cur->format);
    const auto* desc = av_pix_fmt_desc_get(source);
    if (!desc)
        return false;
    const bool planar = !(desc->flags & AV_PIX_FMT_FLAG_RGB) && !rgbFallback;
    const bool highPrecision = m_color.bitDepth > 8 || m_color.transfer != 0;
    const auto destination = planar ? (highPrecision ? AV_PIX_FMT_P016LE : AV_PIX_FMT_NV12)
                                    : (highPrecision ? AV_PIX_FMT_RGBA64LE : AV_PIX_FMT_BGRA);
    m_swsCtx = sws_getCachedContext(m_swsCtx, w, h, source, w, h, destination, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx)
        return false;
    // swscale 只整理像素/矩阵和范围，不承担 PQ/HLG 色调映射，避免提前截断高光。
    int matrix = m_color.kr == 0.2627f ? SWS_CS_BT2020 : m_color.kr == 0.299f ? SWS_CS_ITU601 : SWS_CS_ITU709;
    const int* table = sws_getCoefficients(matrix);
    sws_setColorspaceDetails(m_swsCtx, table, m_color.fullRange, table, planar ? m_color.fullRange : 1, 0, 1 << 16,
                             1 << 16);
    if (!m_cpuFrame || m_cpuFrame->width != w || m_cpuFrame->height != h || m_cpuFrame->format != destination)
    {
        m_cpuFrame.reset(av_frame_alloc());
        if (!m_cpuFrame)
            return false;
        m_cpuFrame->format = destination;
        m_cpuFrame->width = w;
        m_cpuFrame->height = h;
        if (av_frame_get_buffer(m_cpuFrame.get(), 32) < 0)
            return false;
    }
    if (!m_cpuFrame->data[0] ||
        sws_scale(m_swsCtx, cur->data, cur->linesize, 0, h, m_cpuFrame->data, m_cpuFrame->linesize) != h)
        return false;
    m_yuv = planar;
    m_yuv16 = planar && highPrecision;
    if (planar)
    {
        const auto format = highPrecision ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
        if (!ensureRgbaTarget(w, h, format))
            return false;
        D3D11_TEXTURE2D_DESC old{};
        if (m_chromaTex)
            m_chromaTex->GetDesc(&old);
        const int cw = (w + 1) / 2, ch = (h + 1) / 2;
        const auto uvFormat = highPrecision ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
        if (!m_chromaTex || old.Width != UINT(cw) || old.Height != UINT(ch) || old.Format != uvFormat)
        {
            m_chromaTex.Reset();
            D3D11_TEXTURE2D_DESC uv{};
            uv.Width = cw;
            uv.Height = ch;
            uv.ArraySize = 1;
            uv.MipLevels = 1;
            uv.Format = uvFormat;
            uv.SampleDesc.Count = 1;
            uv.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(m_dev->CreateTexture2D(&uv, nullptr, &m_chromaTex)))
                return false;
            m_importDirty = true;
        }
        m_ctx->UpdateSubresource(m_rgbaTex.Get(), 0, nullptr, m_cpuFrame->data[0], m_cpuFrame->linesize[0], 0);
        m_ctx->UpdateSubresource(m_chromaTex.Get(), 0, nullptr, m_cpuFrame->data[1], m_cpuFrame->linesize[1], 0);
    }
    else
    {
        m_chromaTex.Reset();
        if (!ensureRgbaTarget(w, h, highPrecision ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM))
            return false;
        if (highPrecision)
        {
            // 软解高位深 RGB 保留到 FP16；仅作为软解/不支持平面视图时的回退。
            std::vector<qfloat16> pixels(size_t(w) * h * 4);
            for (int y = 0; y < h; ++y)
            {
                const auto* row = reinterpret_cast<const uint16_t*>(m_cpuFrame->data[0] + y * m_cpuFrame->linesize[0]);
                for (int x = 0; x < w * 4; ++x)
                    pixels[size_t(y) * w * 4 + x] = qfloat16(float(row[x]) / 65535.0f);
            }
            m_ctx->UpdateSubresource(m_rgbaTex.Get(), 0, nullptr, pixels.data(), w * 8, 0);
        }
        else
            m_ctx->UpdateSubresource(m_rgbaTex.Get(), 0, nullptr, m_cpuFrame->data[0], m_cpuFrame->linesize[0], 0);
    }
    m_sourceSize = QSize(w, h);
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
    const bool planar =
        desc.Format == DXGI_FORMAT_NV12 || desc.Format == DXGI_FORMAT_P010 || desc.Format == DXGI_FORMAT_P016;
    const bool rgb = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
                     desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM || desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (!planar && !rgb)
        return false;
    const auto format = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ? DXGI_FORMAT_B8G8R8A8_UNORM : desc.Format;
    // 编码尺寸可能带对齐边缘，采样时再按可见尺寸裁剪，不能把色度平面当 BGRA 复制。
    if (!ensureRgbaTarget(int(desc.Width), int(desc.Height), format))
        return false;
    if (m_yuv != planar || m_chromaTex)
        m_importDirty = true;
    m_yuv = planar;
    m_yuv16 = planar && desc.Format != DXGI_FORMAT_NV12;
    m_chromaTex.Reset();
    const D3D11_BOX box{0, 0, 0, desc.Width, desc.Height, 1};
    m_ctx->CopySubresourceRegion(m_rgbaTex.Get(), 0, 0, 0, 0, source, D3D11CalcSubresource(0, slice, desc.MipLevels),
                                 &box);
    m_sourceSize = QSize(frame->width, frame->height);
    m_currentDxgiFormat = desc.Format;
    if (!frame->hw_frames_ctx)
    {
        if (desc.Format == DXGI_FORMAT_P010 || desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
            m_color.bitDepth = 10;
        else if (desc.Format == DXGI_FORMAT_P016 || desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
            m_color.bitDepth = 16;
    }
    const bool uploaded = true;
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
    AVPixelFormat storage = AVPixelFormat(frame->format);
    if (hardware && frame->hw_frames_ctx)
        storage = AVPixelFormat(reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data)->sw_format);
    const auto previousColor = m_color;
    m_color = Oran7VideoColorSpace::fromFrame(frame, storage);
    bool uploaded = hardware ? uploadHardwareFrame(frame) : uploadCpuFrameToRgba(frame);
    if (uploaded)
        uploaded = ensureImportedTexture();
    if (!uploaded && !m_sharedTextureBusy)
    {
        // 平面视图/硬件导入不受支持时，保留原位深下载，不能回退为 8-bit HDR 直出。
        Oran7VideoFrame software(av_frame_alloc());
        AVFrame* fallback = frame;
        if (hardware)
        {
            if (!software || !frame->hw_frames_ctx || av_hwframe_transfer_data(software.get(), frame, 0) < 0 ||
                av_frame_copy_props(software.get(), frame) < 0)
                fallback = nullptr;
            else
                fallback = software.get();
        }
        uploaded = fallback && uploadCpuFrameToRgba(fallback, true) && ensureImportedTexture();
    }
    if (!uploaded)
    {
        m_color = previousColor;
        if (!m_uploadFailureLogged && !m_sharedTextureBusy)
        {
            m_uploadFailureLogged = true;
            WARNING_LOG << "QRhi video frame upload skipped; format=" << frame->format;
        }
        return;
    }
    m_blackFrame = false;
    // 同一轮清屏之后已成功接收新帧，帧末不能把新图像当作旧资源释放。
    m_releaseSerial = m_mailbox->releaseSerial();
    if (!hardware)
    {
        D3D11_TEXTURE2D_DESC desc{};
        m_rgbaTex->GetDesc(&desc);
        m_currentDxgiFormat = desc.Format;
    }
    // 元数据仅在转换成功后整体发布，GUI 不读取节点/GPU 的可变字段。
    Oran7VideoInfo info;
    info.srcSize = QSize(frame->width, frame->height);
    info.srcFormat = frame->format;
    info.srcFormatName = QString::fromLatin1(av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format)));
    info.renderSize = m_rect.size().toSize();
    info.dxgiFormat = m_currentDxgiFormat;
    info.dxgiFormatName = dxgiFormatName(info.dxgiFormat);
    info.bitDepth = m_color.bitDepth;
    info.colorTransfer = m_color.transferName;
    info.colorPrimaries = m_color.primariesName;
    info.colorMatrix = m_color.matrixName;
    info.colorRange = m_color.fullRange ? "full" : "limited";
    info.hdr = m_color.transfer != 0;
    const bool hdrDisplay =
        !m_window->property("_oran7_hdr_display").isValid() || m_window->property("_oran7_hdr_display").toBool();
    info.outputColorMode = Oran7HdrWindowOutput::isActive(m_window) && hdrDisplay ? "scRGB HDR (FP16)"
                           : info.hdr                                             ? "HDR -> SDR tone mapping"
                                                                                  : "SDR";
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
    m_rhiChroma.reset();
    D3D11_TEXTURE2D_DESC desc{};
    m_rgbaTex->GetDesc(&desc);
    auto format = QRhiTexture::BGRA8;
    if (m_yuv)
        format = m_yuv16 ? QRhiTexture::R16 : QRhiTexture::R8;
    else if (desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
        format = QRhiTexture::RGB10A2;
    else if (desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
        format = QRhiTexture::RGBA16F;
    m_rhiTex.reset(m_rhi->newTexture(format, QSize(int(desc.Width), int(desc.Height)), 1));
    if (!importTexture(m_rhiTex, m_rgbaTex.Get()))
    {
        m_rhiTex.reset();
        return false;
    }
    if (m_yuv)
    {
        const QSize size((int(desc.Width) + 1) / 2, (int(desc.Height) + 1) / 2);
        m_rhiChroma.reset(m_rhi->newTexture(m_yuv16 ? QRhiTexture::RG16 : QRhiTexture::RG8, size, 1));
        if (!importTexture(m_rhiChroma, m_chromaTex ? m_chromaTex.Get() : m_rgbaTex.Get()))
        {
            m_rhiChroma.reset();
            return false;
        }
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
                                                   m_sampler.get()),
         QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                   m_rhiChroma ? m_rhiChroma.get() : m_rhiTex.get(), m_sampler.get())});
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
    if (m_blackFrame && !m_rgbaTex) clearBlack();
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
    uniform.mode[0] = m_yuv ? 1 : 0;
    uniform.mode[1] = float(m_color.transfer);
    uniform.mode[2] = m_color.bt2020 ? 1 : 0;
    // 8-bit 离屏目标无法承载扩展值，单独做 SDR 映射，不能影响主 HDR pass。
    bool floatTarget = false;
    if (renderTarget()->resourceType() == QRhiResource::TextureRenderTarget)
    {
        const auto description = static_cast<QRhiTextureRenderTarget*>(renderTarget())->description();
        const auto* texture = description.cbeginColorAttachments() != description.cendColorAttachments()
                                  ? description.colorAttachmentAt(0)->texture()
                                  : nullptr;
        floatTarget = texture && texture->format() == QRhiTexture::RGBA16F;
    }
    else
        floatTarget = Oran7HdrWindowOutput::isActive(m_window);
    const bool hdrDisplay =
        !m_window->property("_oran7_hdr_display").isValid() || m_window->property("_oran7_hdr_display").toBool();
    uniform.mode[3] = floatTarget && Oran7HdrWindowOutput::isActive(m_window) && hdrDisplay ? 1 : 0;
    const float unit = m_yuv16 ? 256.0f / 65535.0f : 1.0f / 255.0f;
    uniform.range[0] = m_color.fullRange ? 0 : 16 * unit;
    uniform.range[1] = m_color.fullRange ? 1 : 1 / (219 * unit);
    uniform.range[2] = 128 * unit;
    uniform.range[3] = m_color.fullRange ? 1 : 1 / (224 * unit);
    uniform.coefficients[0] = m_color.kr;
    uniform.coefficients[1] = m_color.kb;
    uniform.coefficients[2] = m_color.peakNits;
    uniform.coefficients[3] = qBound(80.0f, m_window->property("_oran7_sdr_white_nits").toFloat(), 1000.0f);
    D3D11_TEXTURE2D_DESC textureDesc{};
    m_rgbaTex->GetDesc(&textureDesc);
    if (!m_sourceSize.isEmpty())
    {
        uniform.uvScale[0] = float(m_sourceSize.width()) / textureDesc.Width;
        uniform.uvScale[1] = float(m_sourceSize.height()) / textureDesc.Height;
    }
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
    m_cpuFrame.reset();
    m_yuv = false;
    m_yuv16 = false;
    m_color = {};
    m_rhiChroma.reset();
    m_chromaTex.Reset();
    m_rgbaTex.Reset();
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
