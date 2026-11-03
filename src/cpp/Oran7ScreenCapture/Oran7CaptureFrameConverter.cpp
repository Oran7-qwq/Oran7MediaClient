#include "Oran7CaptureFrameConverter.h"
#include "../Oran7VideoGPURender/Oran7CaptureTextureSlot.h"
#include "Oran7GlobalHelper.h"
#include <QSize>
#include <QThread>
#include <wrl/client.h>
#include <cstring>
extern "C"
{
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
using Microsoft::WRL::ComPtr;
namespace
{
constexpr int kPool = 3;
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
    FrameTransfer m_download = &av_hwframe_transfer_data;
    ComPtr<ID3D11Texture2D> m_encodeNv12Tex, m_encodeReadback;
    ComPtr<ID3D11VideoProcessorEnumerator> m_encodeVpEnum;
    ComPtr<ID3D11VideoProcessor> m_encodeVp;
    QSize m_encodeSourceSize, m_encodeTargetSize;
    bool m_gpuConversionUnavailable = false;
    struct Oran7SwsDeleter
    {
        void operator()(SwsContext* context) const { sws_freeContext(context); }
    };
    std::unique_ptr<SwsContext, Oran7SwsDeleter> m_sws;
    QThread* m_owner = QThread::currentThread(); // 只作线程断言，不跨线程解引用。
    ~Oran7Resources() { Q_ASSERT(m_owner == QThread::currentThread()); }
    bool ensurePreviewPool(int w, int h);
    bool ensurePreviewProcessor(int w, int h);
    bool convertPreviewWithProcessor(int idx, ID3D11Texture2D* srcTex, int w, int h, int slice);
    bool copyPreviewTexture(int idx, ID3D11Texture2D* srcTex, const D3D11_TEXTURE2D_DESC& sdesc, int slice);
    AVFrame* convertRecordingFrame(const AVFrame* frame, int width, int height);
    AVFrame* convertRecordingWithProcessor(const AVFrame* frame, int width, int height);
};

Oran7CaptureFrameConverter::Oran7CaptureFrameConverter(ID3D11Device* device, int fps, FrameTransfer download)
    : m_impl(std::make_unique<Oran7Resources>())
{
    m_impl->m_fps = fps;
    if (download)
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
    m_impl->m_gpuConversionUnavailable = false;
}
Oran7CaptureFrameConverter::Frame Oran7CaptureFrameConverter::recording(const AVFrame* source, int width, int height)
{
    Q_ASSERT(m_impl->m_owner == QThread::currentThread());
    return Frame(m_impl->convertRecordingFrame(source, width, height));
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

AVFrame* Oran7CaptureFrameConverter::Oran7Resources::convertRecordingFrame(const AVFrame* hwFrame, int dstW, int dstH)
{
    // hwFrame 来自 ddagrab → D3D11 硬件帧
    // av_hwframe_transfer_data 正确处理 data[1] 子资源索引
    if (!hwFrame || hwFrame->format != AV_PIX_FMT_D3D11)
    {
        WARNING_LOG << "downloadD3d11: invalid hwFrame";
        return nullptr;
    }

    if (!m_gpuConversionUnavailable)
    {
        if (auto* converted = convertRecordingWithProcessor(hwFrame, dstW, dstH))
            return converted;
        m_gpuConversionUnavailable = true;
        WARNING_LOG << "[CaptureDiag] GPU NV12 conversion unavailable, using CPU fallback";
    }

    Frame bgra(av_frame_alloc());
    if (!bgra)
        return nullptr;

    int ret = m_download(bgra.get(), hwFrame, 0);
    if (ret < 0)
    {
        WARNING_LOG << "av_hwframe_transfer_data failed:" << ret;
        return nullptr;
    }
    av_frame_copy_props(bgra.get(), hwFrame);

    // 校验下载后的软件像素格式；ddagrab output_fmt=bgra + allow_fallback
    // 正常情况下是 BGRA，个别 GPU/驱动可能降级为其他 8bit 格式，这里按实际格式转换
    const AVPixelFormat srcFmt = AVPixelFormat(bgra->format);
    if (srcFmt != AV_PIX_FMT_BGRA)
    {
        const char* name = av_get_pix_fmt_name(srcFmt);
        WARNING_LOG << "downloadD3d11: downloaded format:" << (name ? name : "?")
                    << " (expected BGRA), using it directly";
    }

    int srcW = bgra->width;
    int srcH = bgra->height;
    int outW = (dstW > 0) ? dstW : srcW;
    int outH = (dstH > 0) ? dstH : srcH;

    // sws_getCachedContext 可能释放旧实例，先交出所有权，再接管返回值。
    m_sws.reset(sws_getCachedContext(m_sws.release(), srcW, srcH, srcFmt, outW, outH, AV_PIX_FMT_NV12, SWS_BICUBIC,
                                     nullptr, nullptr, nullptr));
    if (!m_sws)
    {
        return nullptr;
    }
    const auto* coefficients = sws_getCoefficients(SWS_CS_ITU709);
    const bool sourceRgb = srcFmt == AV_PIX_FMT_BGRA || srcFmt == AV_PIX_FMT_RGBA;
    const int sourceRange = sourceRgb || hwFrame->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
    sws_setColorspaceDetails(m_sws.get(), coefficients, sourceRange, coefficients, 0, 0, 1 << 16, 1 << 16);

    Frame dst(av_frame_alloc());
    if (!dst)
    {
        return nullptr;
    }
    dst->format = AV_PIX_FMT_NV12;
    dst->width = outW;
    dst->height = outH;
    if (av_frame_get_buffer(dst.get(), 32) < 0)
    {
        return nullptr;
    }
    int rows = sws_scale(m_sws.get(), bgra->data, bgra->linesize, 0, srcH, dst->data, dst->linesize);
    if (rows != outH)
    {
        WARNING_LOG << "BGRA→NV12 incomplete:" << rows << "/" << outH;
        return nullptr;
    }
    dst->pts = hwFrame->pts;
    dst->color_range = AVCOL_RANGE_MPEG;
    dst->colorspace = AVCOL_SPC_BT709;
    dst->color_primaries = AVCOL_PRI_BT709;
    dst->color_trc = AVCOL_TRC_BT709;
    return dst.release();
}

AVFrame* Oran7CaptureFrameConverter::Oran7Resources::convertRecordingWithProcessor(const AVFrame* frame, int width,
                                                                                   int height)
{
    if (!frame || !frame->data[0] || !m_capVideoDev || !m_capVideoCtx || !m_capCtx || width <= 0 || height <= 0 ||
        (width & 1) || (height & 1))
        return nullptr;
    auto* texture = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
    const QSize source(frame->width, frame->height), target(width, height);
    if (!m_encodeVp || source != m_encodeSourceSize || target != m_encodeTargetSize)
    {
        m_encodeVp.Reset();
        m_encodeVpEnum.Reset();
        m_encodeNv12Tex.Reset();
        m_encodeReadback.Reset();
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC content{};
        content.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        content.InputWidth = source.width();
        content.InputHeight = source.height();
        content.OutputWidth = width;
        content.OutputHeight = height;
        content.InputFrameRate = {UINT(m_fps), 1};
        content.OutputFrameRate = content.InputFrameRate;
        content.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        if (FAILED(m_capVideoDev->CreateVideoProcessorEnumerator(&content, &m_encodeVpEnum)) ||
            FAILED(m_capVideoDev->CreateVideoProcessor(m_encodeVpEnum.Get(), 0, &m_encodeVp)))
            return nullptr;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
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
        m_encodeSourceSize = source;
        m_encodeTargetSize = target;
    }
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc{};
    inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputDesc.Texture2D.ArraySlice = UINT(reinterpret_cast<intptr_t>(frame->data[1]));
    ComPtr<ID3D11VideoProcessorInputView> input;
    if (FAILED(m_capVideoDev->CreateVideoProcessorInputView(texture, m_encodeVpEnum.Get(), &inputDesc, &input)))
        return nullptr;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc{};
    outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11VideoProcessorOutputView> output;
    if (FAILED(m_capVideoDev->CreateVideoProcessorOutputView(m_encodeNv12Tex.Get(), m_encodeVpEnum.Get(), &outputDesc,
                                                             &output)))
        return nullptr;
    RECT sourceRect{0, 0, source.width(), source.height()}, targetRect{0, 0, width, height};
    m_capVideoCtx->VideoProcessorSetStreamFrameFormat(m_encodeVp.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    m_capVideoCtx->VideoProcessorSetStreamSourceRect(m_encodeVp.Get(), 0, TRUE, &sourceRect);
    m_capVideoCtx->VideoProcessorSetStreamDestRect(m_encodeVp.Get(), 0, TRUE, &targetRect);
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
    if (FAILED(m_capVideoCtx->VideoProcessorBlt(m_encodeVp.Get(), output.Get(), 0, 1, &stream)))
        return nullptr;
    m_capCtx->CopyResource(m_encodeReadback.Get(), m_encodeNv12Tex.Get());
    Frame result(av_frame_alloc());
    if (!result)
        return nullptr;
    result->format = AV_PIX_FMT_NV12;
    result->width = width;
    result->height = height;
    if (av_frame_get_buffer(result.get(), 32) < 0)
    {
        return nullptr;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_capCtx->Map(m_encodeReadback.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        return nullptr;
    }
    const auto* data = static_cast<const uint8_t*>(mapped.pData);
    for (int y = 0; y < height; ++y)
        std::memcpy(result->data[0] + y * result->linesize[0], data + size_t(y) * mapped.RowPitch, width);
    data += size_t(height) * mapped.RowPitch;
    for (int y = 0; y < height / 2; ++y)
        std::memcpy(result->data[1] + y * result->linesize[1], data + size_t(y) * mapped.RowPitch, width);
    m_capCtx->Unmap(m_encodeReadback.Get(), 0);
    result->pts = frame->pts;
    result->color_range = AVCOL_RANGE_MPEG;
    result->colorspace = AVCOL_SPC_BT709;
    result->color_primaries = AVCOL_PRI_BT709;
    result->color_trc = AVCOL_TRC_BT709;
    return result.release();
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
