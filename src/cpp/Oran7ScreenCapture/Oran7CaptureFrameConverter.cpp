#include "Oran7CaptureFrameConverter.h"
#include "../Oran7VideoGPURender/Oran7CaptureTextureSlot.h"
#include "Oran7GlobalHelper.h"
#include <QSize>
#include <QThread>
#include <wrl/client.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
extern "C"
{
#include <libavcodec/defs.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
using Microsoft::WRL::ComPtr;
namespace
{
constexpr int kPool = 3;

void fillBlack(AVFrame* frame)
{
    for (int plane = 0; plane < 2; ++plane)
        for (int y = 0; y < frame->height / (plane + 1); ++y)
            std::memset(frame->data[plane] + y * frame->linesize[plane], plane ? 128 : 16, frame->width);
}

// 每个 UV 采样是两个相邻字节，旋转时将 UV 对视为一个像素。
void copyRecordingRegion(AVFrame* frame, const QRect& area, const uint8_t* const source[2],
                         const int stride[2], const Oran7CaptureComposition& composition)
{
    if (area != QRect(0, 0, frame->width, frame->height))
        fillBlack(frame);
    for (int plane = 0; plane < 2; ++plane)
    {
        const int step = plane + 1;
        const QSize size(area.width() / step, area.height() / step);
        auto* target = frame->data[plane] + area.y() / step * frame->linesize[plane] + area.x();
        if (!composition.transformsPixels())
        {
            for (int y = 0; y < size.height(); ++y)
                std::memcpy(target + y * frame->linesize[plane], source[plane] + y * stride[plane], area.width());
            continue;
        }

        const auto input = composition.inputSize(size);
        const auto offset = [&](int x, int y) -> std::ptrdiff_t {
            const auto point = composition.sourceRect(QRect(x, y, 1, 1), input).topLeft();
            return std::ptrdiff_t(point.y()) * stride[plane] + point.x() * step;
        };
        const auto start = offset(0, 0);
        const auto column = offset(1, 0) - start;
        const auto row = offset(0, 1) - start;
        for (int y = 0; y < size.height(); ++y)
        {
            auto* dst = target + y * frame->linesize[plane];
            const auto* src = source[plane] + start + y * row;
            if (plane == 0)
                for (int x = 0; x < size.width(); ++x)
                    dst[x] = src[x * column];
            else
                for (int x = 0; x < size.width(); ++x)
                    std::memcpy(dst + x * 2, src + x * column, 2);
        }
    }
}

AVFrame* finishOutput(Oran7CaptureFrameConverter::Frame frame, const AVFrame* source)
{
    frame->pts = source->pts;
    frame->color_range = AVCOL_RANGE_MPEG;
    frame->colorspace = AVCOL_SPC_BT709;
    frame->color_primaries = AVCOL_PRI_BT709;
    frame->color_trc = AVCOL_TRC_BT709;
    return frame.release();
}
}
struct Oran7CaptureFrameConverter::Oran7Resources
{
    ComPtr<ID3D11Device> m_capDev;
    ComPtr<ID3D11DeviceContext> m_capCtx;
    ComPtr<ID3D11VideoDevice> m_capVideoDev;
    ComPtr<ID3D11VideoContext> m_capVideoCtx;
    ComPtr<ID3D11VideoProcessorEnumerator> m_vpEnum;
    ComPtr<ID3D11VideoProcessor> m_vp;
    QSize m_vpSize;
    std::shared_ptr<Oran7CaptureTextureSlot> m_sharedSlots[kPool];
    int m_poolW = 0, m_poolH = 0, m_index = 0;
    int m_fps;
    FrameTransfer m_download = nullptr;
    ComPtr<ID3D11Texture2D> m_bgraReadback;
    QSize m_bgraReadbackSize;
    ComPtr<ID3D11Texture2D> m_encodeNv12Tex, m_encodeReadback;
    ComPtr<ID3D11VideoProcessorOutputView> m_encodeOutputView;
    ComPtr<ID3D11VideoProcessorEnumerator> m_encodeVpEnum;
    ComPtr<ID3D11VideoProcessor> m_encodeVp;
    QSize m_encodeSourceSize, m_encodeTargetSize;
    bool m_gpuConversionUnavailable[2] = {};
    struct Oran7SwsDeleter
    {
        void operator()(SwsContext* context) const { sws_freeContext(context); }
    };
    std::unique_ptr<SwsContext, Oran7SwsDeleter> m_sws;
    struct Oran7PoolDeleter
    {
        void operator()(AVBufferPool* pool) const { av_buffer_pool_uninit(&pool); }
    };
    std::unique_ptr<AVBufferPool, Oran7PoolDeleter> m_recordingPool;
    QSize m_recordingPoolSize;
    Frame m_softwareRegion;
    Frame allocateRecordingFrame(int width, int height);
    int downloadFrame(AVFrame* result, const AVFrame* input);
    QThread* m_owner = QThread::currentThread(); // 只作线程断言，不跨线程解引用。
    ~Oran7Resources() { Q_ASSERT(m_owner == QThread::currentThread()); }
    bool ensurePreviewPool(int w, int h);
    bool ensurePreviewProcessor(int w, int h);
    bool convertPreviewWithProcessor(int idx, ID3D11Texture2D* srcTex, int w, int h, int slice);
    bool copyPreviewTexture(int idx, ID3D11Texture2D* srcTex, const D3D11_TEXTURE2D_DESC& sdesc, int slice);
    AVFrame* convertRecordingFrame(const AVFrame* frame, int width, int height, const Oran7CaptureComposition&);
    AVFrame* convertRecordingWithProcessor(const AVFrame* frame, int width, int height,
                                           const Oran7CaptureComposition::Oran7Blit&, const Oran7CaptureComposition&);
};

Oran7CaptureFrameConverter::Frame Oran7CaptureFrameConverter::Oran7Resources::allocateRecordingFrame(int width,
                                                                                                     int height)
{
    if (!m_recordingPool || m_recordingPoolSize != QSize(width, height))
    {
        const int bytes = av_image_get_buffer_size(AV_PIX_FMT_NV12, width, height, 32);
        if (bytes < 0)
            return {};
        m_recordingPool.reset(av_buffer_pool_init(size_t(bytes) + AV_INPUT_BUFFER_PADDING_SIZE, nullptr));
        if (!m_recordingPool)
            return {};
        m_recordingPoolSize = QSize(width, height);
    }
    Frame result(av_frame_alloc());
    if (!result)
        return {};
    result->buf[0] = av_buffer_pool_get(m_recordingPool.get());
    if (!result->buf[0])
        return {};
    result->format = AV_PIX_FMT_NV12;
    result->width = width;
    result->height = height;
    if (av_image_fill_arrays(result->data, result->linesize, result->buf[0]->data, AV_PIX_FMT_NV12, width, height, 32) <
        0)
        return {};
    return result;
}

Oran7CaptureFrameConverter::Oran7CaptureFrameConverter(ID3D11Device* device, int fps, FrameTransfer download)
    : m_impl(std::make_unique<Oran7Resources>())
{
    m_impl->m_fps = fps;
    m_impl->m_download = download;
    m_impl->m_capDev = device;
    if (device)
    {
        device->GetImmediateContext(&m_impl->m_capCtx);
        device->QueryInterface(IID_PPV_ARGS(&m_impl->m_capVideoDev));
        m_impl->m_capCtx.As(&m_impl->m_capVideoCtx);
    }
}
Oran7CaptureFrameConverter::~Oran7CaptureFrameConverter() = default;
void Oran7CaptureFrameConverter::beginRecording()
{
    Q_ASSERT(m_impl->m_owner == QThread::currentThread());
    endRecording();
    m_impl->m_gpuConversionUnavailable[0] = false;
    m_impl->m_gpuConversionUnavailable[1] = false;
}
void Oran7CaptureFrameConverter::endRecording()
{
    Q_ASSERT(m_impl->m_owner == QThread::currentThread());
    m_impl->m_recordingPool.reset();
    m_impl->m_recordingPoolSize = {};
    m_impl->m_softwareRegion.reset();
}
Oran7CaptureFrameConverter::Frame Oran7CaptureFrameConverter::recording(const AVFrame* source, int width, int height,
                                                                        const Oran7CaptureComposition& composition)
{
    Q_ASSERT(m_impl->m_owner == QThread::currentThread());
    return Frame(m_impl->convertRecordingFrame(source, width, height, composition));
}
Oran7CaptureFrameConverter::Oran7PreviewResult Oran7CaptureFrameConverter::preview(const AVFrame* source)
{
    Q_ASSERT(m_impl->m_owner == QThread::currentThread());
    Oran7PreviewResult result;
    if (!source || source->format != AV_PIX_FMT_D3D11 || !source->data[0] || !m_impl->m_capDev)
    {
        result.error = QStringLiteral("预览转换输入帧或设备无效");
        return result;
    }
    auto* texture = reinterpret_cast<ID3D11Texture2D*>(source->data[0]);
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    const int slice = int(reinterpret_cast<intptr_t>(source->data[1]));
    if (slice < 0 || UINT(slice) >= desc.ArraySize || source->width <= 0 || source->height <= 0 ||
        UINT(source->width) > desc.Width || UINT(source->height) > desc.Height)
    {
        result.error = QStringLiteral("预览转换尺寸或子资源无效");
        return result;
    }
    if (!m_impl->ensurePreviewPool(int(desc.Width), int(desc.Height)))
    {
        result.error = QStringLiteral("ensurePreviewPool failed");
        return result;
    }
    // CPU 租约防止未消费帧被覆盖，零超时 mutex 保证跨设备 GPU 顺序。
    for (int attempt = 0; attempt < kPool; ++attempt)
    {
        m_impl->m_index = (m_impl->m_index + 1) % kPool;
        const int index = m_impl->m_index;
        const auto& slot = m_impl->m_sharedSlots[index];
        if (slot.use_count() != 1)
            continue;
        const HRESULT acquired = slot->mutex->AcquireSync(0, 0);
        if (acquired == WAIT_TIMEOUT)
            continue;
        if (acquired != S_OK)
        {
            result.error =
                QStringLiteral("capture shared mutex acquire failed: 0x") + QString::number(quint32(acquired), 16);
            result.invalidated = true;
            return result;
        }
        const bool copied = m_impl->copyPreviewTexture(index, texture, desc, slice);
        m_impl->m_capCtx->Flush();
        const HRESULT released = slot->mutex->ReleaseSync(0);
        if (released != S_OK)
        {
            result.error =
                QStringLiteral("capture shared mutex release failed: 0x") + QString::number(quint32(released), 16);
            result.invalidated = true;
            return result;
        }
        if (copied)
            result.frame.reset(oran7MakeCaptureFrame(slot, source->width, source->height));
        break;
    }
    return result;
}

bool Oran7CaptureFrameConverter::Oran7Resources::ensurePreviewPool(int w, int h)
{
    if (m_poolW == w && m_poolH == h && m_sharedSlots[0])
        return true;

    // 尺寸/会话变化创建新槽位，旧槽位随最后一个 AVFrame 租约释放。
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = UINT(w);
    desc.Height = UINT(h);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    std::shared_ptr<Oran7CaptureTextureSlot> newSlots[kPool];
    for (auto& slot : newSlots)
    {
        slot = std::make_shared<Oran7CaptureTextureSlot>();
        if (FAILED(m_capDev->CreateTexture2D(&desc, nullptr, &slot->texture)) || FAILED(slot->texture.As(&slot->mutex)))
            return false;
    }
    for (int i = 0; i < kPool; ++i)
        m_sharedSlots[i] = std::move(newSlots[i]);
    m_poolW = w;
    m_poolH = h;

    return true;
}

int Oran7CaptureFrameConverter::Oran7Resources::downloadFrame(AVFrame* result, const AVFrame* input)
{
    if (m_download)
        return m_download(result, input, 0);
    if (input->hw_frames_ctx)
        return av_hwframe_transfer_data(result, input, 0);
    // 窗口采集的 Surface 不属于 FFmpeg 帧池，回读其实际可见区域。
    auto* source = reinterpret_cast<ID3D11Texture2D*>(input->data[0]);
    if (!source || !m_capDev || !m_capCtx)
        return AVERROR(EINVAL);
    D3D11_TEXTURE2D_DESC sourceDesc{};
    source->GetDesc(&sourceDesc);
    ComPtr<ID3D11Device> sourceDevice;
    source->GetDevice(sourceDevice.GetAddressOf());
    const auto slice = UINT(reinterpret_cast<uintptr_t>(input->data[1]));
    if (sourceDevice.Get() != m_capDev.Get() || sourceDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM ||
        sourceDesc.SampleDesc.Count != 1 || slice >= sourceDesc.ArraySize ||
        UINT(input->width) > sourceDesc.Width || UINT(input->height) > sourceDesc.Height)
        return AVERROR(EINVAL);

    const QSize size(input->width, input->height);
    if (!m_bgraReadback || size != m_bgraReadbackSize)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = size.width();
        desc.Height = size.height();
        desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
        desc.Format = sourceDesc.Format;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(m_capDev->CreateTexture2D(&desc, nullptr, m_bgraReadback.ReleaseAndGetAddressOf())))
            return AVERROR(EIO);
        m_bgraReadbackSize = size;
    }
    result->format = AV_PIX_FMT_BGRA;
    result->width = size.width();
    result->height = size.height();
    const int allocated = av_frame_get_buffer(result, 32);
    if (allocated < 0)
        return allocated;
    const D3D11_BOX region{0, 0, 0, UINT(size.width()), UINT(size.height()), 1};
    m_capCtx->CopySubresourceRegion(m_bgraReadback.Get(), 0, 0, 0, 0, source,
                                    D3D11CalcSubresource(0, slice, sourceDesc.MipLevels), &region);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_capCtx->Map(m_bgraReadback.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return AVERROR(EIO);
    const auto* pixels = static_cast<const uint8_t*>(mapped.pData);
    for (int y = 0; y < size.height(); ++y)
        std::memcpy(result->data[0] + size_t(y) * result->linesize[0], pixels + size_t(y) * mapped.RowPitch, size.width() * 4);
    m_capCtx->Unmap(m_bgraReadback.Get(), 0);
    return 0;
}

AVFrame* Oran7CaptureFrameConverter::Oran7Resources::convertRecordingFrame(const AVFrame* hwFrame, int dstW, int dstH,
                                                                          const Oran7CaptureComposition& composition)
{
    // hwFrame 来自 ddagrab → D3D11 硬件帧
    // av_hwframe_transfer_data 正确处理 data[1] 子资源索引
    if (!hwFrame || (hwFrame->format != AV_PIX_FMT_D3D11 && hwFrame->format != AV_PIX_FMT_NV12 &&
                     hwFrame->format != AV_PIX_FMT_BGRA) ||
        hwFrame->width <= 0 || hwFrame->height <= 0)
    {
        WARNING_LOG << "downloadD3d11: invalid hwFrame";
        return nullptr;
    }

    const int outW = dstW > 0 ? dstW : hwFrame->width;
    const int outH = dstH > 0 ? dstH : hwFrame->height;
    const QRect canvas(0, 0, outW, outH);
    const auto blit = composition.blit(QSize(hwFrame->width, hwFrame->height), canvas.size());
    if (blit.destination.isEmpty())
    {
        auto black = allocateRecordingFrame(outW, outH);
        if (!black)
            return nullptr;
        fillBlack(black.get());
        return finishOutput(std::move(black), hwFrame);
    }
    auto& gpuUnavailable = m_gpuConversionUnavailable[composition.swapsAxes()];
    const bool hardware = hwFrame->format == AV_PIX_FMT_D3D11;
    if (hardware && !gpuUnavailable)
    {
        if (auto* converted = convertRecordingWithProcessor(hwFrame, outW, outH, blit, composition))
            return converted;
        gpuUnavailable = true;
        WARNING_LOG << "[CaptureDiag] GPU NV12 conversion unavailable, using CPU fallback";
    }

    Frame bgra(hardware ? av_frame_alloc() : av_frame_clone(hwFrame));
    if (!bgra)
        return nullptr;

    if (hardware)
    {
        const int ret = downloadFrame(bgra.get(), hwFrame);
        if (ret < 0)
        {
            WARNING_LOG << "capture readback failed:" << ret;
            return nullptr;
        }
        av_frame_copy_props(bgra.get(), hwFrame);
    }

    // 校验下载后的软件像素格式；ddagrab output_fmt=bgra + allow_fallback
    // 正常情况下是 BGRA，个别 GPU/驱动可能降级为其他 8bit 格式，这里按实际格式转换
    const AVPixelFormat srcFmt = AVPixelFormat(bgra->format);
    if (hardware && srcFmt != AV_PIX_FMT_BGRA)
    {
        const char* name = av_get_pix_fmt_name(srcFmt);
        WARNING_LOG << "downloadD3d11: downloaded format:" << (name ? name : "?")
                    << " (expected BGRA), using it directly";
    }

    const auto cpuBlit = composition.blit(QSize(bgra->width, bgra->height), canvas.size());
    const auto& src = cpuBlit.source;
    const auto& area = cpuBlit.destination;
    if (area.isEmpty())
    {
        auto black = allocateRecordingFrame(outW, outH);
        if (!black)
            return nullptr;
        fillBlack(black.get());
        return finishOutput(std::move(black), hwFrame);
    }
    if (src != QRect(0, 0, bgra->width, bgra->height))
    {
        bgra->crop_left = src.x();
        bgra->crop_top = src.y();
        bgra->crop_right = bgra->width - src.x() - src.width();
        bgra->crop_bottom = bgra->height - src.y() - src.height();
        if (av_frame_apply_cropping(bgra.get(), AV_FRAME_CROP_UNALIGNED) < 0)
            return nullptr;
    }

    // sws_getCachedContext 可能释放旧实例，先交出所有权，再接管返回值。
    const auto regionSize = composition.inputSize(area.size());
    m_sws.reset(sws_getCachedContext(m_sws.release(), bgra->width, bgra->height, srcFmt,
                                     regionSize.width(), regionSize.height(), AV_PIX_FMT_NV12, SWS_BICUBIC,
                                     nullptr, nullptr, nullptr));
    if (!m_sws)
    {
        return nullptr;
    }
    const auto* coefficients = sws_getCoefficients(SWS_CS_ITU709);
    const bool sourceRgb = srcFmt == AV_PIX_FMT_BGRA || srcFmt == AV_PIX_FMT_RGBA;
    const int sourceRange = sourceRgb || hwFrame->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
    sws_setColorspaceDetails(m_sws.get(), coefficients, sourceRange, coefficients, 0, 0, 1 << 16, 1 << 16);

    Frame dst = allocateRecordingFrame(outW, outH);
    if (!dst)
    {
        return nullptr;
    }
    AVFrame* writeTarget = dst.get();
    if (area != canvas || composition.transformsPixels())
    {
        // 让 swscale 写入独立的对齐缓冲，避免 SIMD 写过子矩形污染黑边。
        if (!m_softwareRegion || QSize(m_softwareRegion->width, m_softwareRegion->height) != regionSize)
        {
            Frame region(av_frame_alloc());
            if (!region)
                return nullptr;
            region->width = regionSize.width();
            region->height = regionSize.height();
            region->format = AV_PIX_FMT_NV12;
            if (av_frame_get_buffer(region.get(), 32) < 0)
                return nullptr;
            m_softwareRegion = std::move(region);
        }
        writeTarget = m_softwareRegion.get();
    }
    int rows = sws_scale(m_sws.get(), bgra->data, bgra->linesize, 0, bgra->height,
                         writeTarget->data, writeTarget->linesize);
    if (rows != regionSize.height())
    {
        WARNING_LOG << "BGRA→NV12 incomplete:" << rows << "/" << regionSize.height();
        return nullptr;
    }
    if (writeTarget != dst.get())
    {
        const uint8_t* planes[] = {writeTarget->data[0], writeTarget->data[1]};
        copyRecordingRegion(dst.get(), area, planes, writeTarget->linesize, composition);
    }
    return finishOutput(std::move(dst), hwFrame);
}

AVFrame* Oran7CaptureFrameConverter::Oran7Resources::convertRecordingWithProcessor(const AVFrame* frame, int width,
                                                                                   int height,
                                                                                   const Oran7CaptureComposition::Oran7Blit& blit,
                                                                                   const Oran7CaptureComposition& composition)
{
    if (!frame || !frame->data[0] || !m_capVideoDev || !m_capVideoCtx || !m_capCtx || width <= 0 || height <= 0 ||
        (width & 1) || (height & 1))
        return nullptr;
    auto* texture = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
    const QSize source(frame->width, frame->height);
    const auto target = composition.inputSize(QSize(width, height));
    if (!m_encodeVp || source != m_encodeSourceSize || target != m_encodeTargetSize)
    {
        m_encodeVp.Reset();
        m_encodeOutputView.Reset();
        m_encodeVpEnum.Reset();
        m_encodeNv12Tex.Reset();
        m_encodeReadback.Reset();
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC content{};
        content.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        content.InputWidth = source.width();
        content.InputHeight = source.height();
        content.OutputWidth = target.width();
        content.OutputHeight = target.height();
        content.InputFrameRate = {UINT(m_fps), 1};
        content.OutputFrameRate = content.InputFrameRate;
        content.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        if (FAILED(m_capVideoDev->CreateVideoProcessorEnumerator(&content, &m_encodeVpEnum)) ||
            FAILED(m_capVideoDev->CreateVideoProcessor(m_encodeVpEnum.Get(), 0, &m_encodeVp)))
            return nullptr;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = target.width();
        desc.Height = target.height();
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_NV12;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(m_capDev->CreateTexture2D(&desc, nullptr, &m_encodeNv12Tex)))
            return nullptr;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(m_capDev->CreateTexture2D(&desc, nullptr, &m_encodeReadback)))
            return nullptr;
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc{};
        outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        if (FAILED(m_capVideoDev->CreateVideoProcessorOutputView(m_encodeNv12Tex.Get(), m_encodeVpEnum.Get(),
                                                                 &outputDesc, &m_encodeOutputView)))
            return nullptr;
        m_encodeSourceSize = source;
        m_encodeTargetSize = target;
    }
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc{};
    inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputDesc.Texture2D.ArraySlice = UINT(reinterpret_cast<intptr_t>(frame->data[1]));
    ComPtr<ID3D11VideoProcessorInputView> input;
    if (FAILED(m_capVideoDev->CreateVideoProcessorInputView(texture, m_encodeVpEnum.Get(), &inputDesc, &input)))
        return nullptr;
    const auto native = [](const QRect& rect) {
        return RECT{rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height()};
    };
    const auto sourceRect = native(blit.source);
    // 仍走一次 VideoProcessor + NV12 回读，在写入最终帧时应用旋转和镜像。
    const auto region = composition.transformsPixels()
        ? QRect(QPoint(), composition.inputSize(blit.destination.size())) : blit.destination;
    const auto destinationRect = native(region);
    const auto targetRect = native(QRect(QPoint(), target));
    m_capVideoCtx->VideoProcessorSetStreamFrameFormat(m_encodeVp.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    m_capVideoCtx->VideoProcessorSetStreamSourceRect(m_encodeVp.Get(), 0, TRUE, &sourceRect);
    m_capVideoCtx->VideoProcessorSetStreamDestRect(m_encodeVp.Get(), 0, TRUE, &destinationRect);
    m_capVideoCtx->VideoProcessorSetOutputTargetRect(m_encodeVp.Get(), TRUE, &targetRect);
    m_capVideoCtx->VideoProcessorSetStreamAutoProcessingMode(m_encodeVp.Get(), 0, FALSE);
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE inputColor{}, outputColor{};
    inputColor.RGB_Range = 0;
    inputColor.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
    outputColor.YCbCr_Matrix = 1;
    outputColor.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
    m_capVideoCtx->VideoProcessorSetStreamColorSpace(m_encodeVp.Get(), 0, &inputColor);
    m_capVideoCtx->VideoProcessorSetOutputColorSpace(m_encodeVp.Get(), &outputColor);
    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = input.Get();
    if (FAILED(m_capVideoCtx->VideoProcessorBlt(m_encodeVp.Get(), m_encodeOutputView.Get(), 0, 1, &stream)))
        return nullptr;
    m_capCtx->CopyResource(m_encodeReadback.Get(), m_encodeNv12Tex.Get());
    Frame result = allocateRecordingFrame(width, height);
    if (!result)
        return nullptr;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_capCtx->Map(m_encodeReadback.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        return nullptr;
    }
    const auto* data = static_cast<const uint8_t*>(mapped.pData);
    const uint8_t* planes[] = {data + size_t(region.y()) * mapped.RowPitch + region.x(),
        data + size_t(target.height() + region.y() / 2) * mapped.RowPitch + region.x()};
    const int stride[] = {int(mapped.RowPitch), int(mapped.RowPitch)};
    copyRecordingRegion(result.get(), blit.destination, planes, stride, composition);
    m_capCtx->Unmap(m_encodeReadback.Get(), 0);
    return finishOutput(std::move(result), frame);
}

bool Oran7CaptureFrameConverter::Oran7Resources::ensurePreviewProcessor(int w, int h)
{
    if (!m_capVideoDev || !m_capVideoCtx)
        return false;

    if (m_vp && m_vpEnum && m_vpSize == QSize(w, h))
        return true;

    m_vp.Reset();
    m_vpEnum.Reset();
    m_vpSize = QSize(w, h);

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC c{};
    c.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    c.InputWidth = (UINT)w;
    c.InputHeight = (UINT)h;
    c.OutputWidth = (UINT)w;
    c.OutputHeight = (UINT)h;
    c.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr = m_capVideoDev->CreateVideoProcessorEnumerator(&c, &m_vpEnum);
    if (FAILED(hr) || !m_vpEnum)
        return false;

    hr = m_capVideoDev->CreateVideoProcessor(m_vpEnum.Get(), 0, &m_vp);
    return SUCCEEDED(hr) && m_vp;
}

bool Oran7CaptureFrameConverter::Oran7Resources::convertPreviewWithProcessor(int idx, ID3D11Texture2D* srcTex, int w,
                                                                             int h, int slice)
{
    if (!srcTex)
        return false;
    if (idx < 0 || idx >= kPool)
        return false;
    if (!m_sharedSlots[idx])
        return false;

    if (!ensurePreviewProcessor(w, h))
        return false;
    if (!m_vpEnum || !m_vp)
        return false;

    // 1) Input view: srcTex (NV12/P010), may be array texture; slice indicates which slice
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
    inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inDesc.Texture2D.ArraySlice = (UINT)slice;

    ComPtr<ID3D11VideoProcessorInputView> inView;
    HRESULT hr = m_capVideoDev->CreateVideoProcessorInputView(srcTex, m_vpEnum.Get(), &inDesc, &inView);
    if (FAILED(hr) || !inView)
        return false;

    // 2) Output view: sharedTex[idx] (RGBA8)
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{};
    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outDesc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11VideoProcessorOutputView> outView;
    hr = m_capVideoDev->CreateVideoProcessorOutputView(m_sharedSlots[idx]->texture.Get(), m_vpEnum.Get(), &outDesc,
                                                       &outView);
    if (FAILED(hr) || !outView)
        return false;

    RECT srcRect{0, 0, w, h};
    RECT dstRect{0, 0, w, h};

    m_capVideoCtx->VideoProcessorSetStreamSourceRect(m_vp.Get(), 0, TRUE, &srcRect);
    m_capVideoCtx->VideoProcessorSetStreamDestRect(m_vp.Get(), 0, TRUE, &dstRect);

    // 可选：色彩空间/范围设置（先不碰，默认通常可用）
    // m_capVideoCtx->VideoProcessorSetOutputColorSpace1(...)

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inView.Get();

    hr = m_capVideoCtx->VideoProcessorBlt(m_vp.Get(), outView.Get(), 0, 1, &stream);
    return SUCCEEDED(hr);
}

bool Oran7CaptureFrameConverter::Oran7Resources::copyPreviewTexture(int idx, ID3D11Texture2D* srcTex,
                                                                    const D3D11_TEXTURE2D_DESC& sdesc, int slice)
{
    if (!srcTex)
    {
        WARNING_LOG << "copyPreviewTexture srcTex is nullptr.";
        return false;
    }
    if (idx < 0 || idx >= kPool)
    {
        WARNING_LOG << "copyPreviewTexture idx is ERROR,idx:" << idx;
        return false;
    }
    if (!m_capCtx)
    {
        WARNING_LOG << "copyPreviewTexture m_capCtx is nullptr.";
        return false;
    }
    if (!m_sharedSlots[idx])
    {
        WARNING_LOG << "copyPreviewTexture slot is nullptr, idx:" << idx;
        return false;
    }

    // BGRA -> BGRA  --- NOW
    if (sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
    {
        // ArraySlice 是 FFmpeg 帧身份的一部分，不能复制整个 array 到单槽纹理。
        m_capCtx->CopySubresourceRegion(m_sharedSlots[idx]->texture.Get(), 0, 0, 0, 0, srcTex,
                                        D3D11CalcSubresource(0, UINT(slice), sdesc.MipLevels), nullptr);
        return true;
    }

    // RGBA 与目标 BGRA 字节序不同，交由 VideoProcessor 转换。
    if (sdesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || sdesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
        return convertPreviewWithProcessor(idx, srcTex, int(sdesc.Width), int(sdesc.Height), slice);

    // NV12/P010 -> RGBA: 用 VideoProcessor
    if (sdesc.Format == DXGI_FORMAT_NV12 || sdesc.Format == DXGI_FORMAT_P010)
    {
        return convertPreviewWithProcessor(idx, srcTex, (int)sdesc.Width, (int)sdesc.Height, slice);
    }

    WARNING_LOG << "copyPreviewTexture Other formt not support:" << quint32(sdesc.Format);
    return false;
}
