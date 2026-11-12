#include "Oran7VideoColorSpace.h"
#include <algorithm>
#include <cmath>
extern "C"
{
#include <libavutil/pixdesc.h>
#include <libavutil/mastering_display_metadata.h>
}

Oran7VideoColorSpace Oran7VideoColorSpace::fromFrame(const AVFrame* frame, AVPixelFormat format)
{
    Oran7VideoColorSpace result;
    if (const auto* descriptor = av_pix_fmt_desc_get(format);
        descriptor && descriptor->nb_components > 0 && !(descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL))
        result.bitDepth = descriptor->comp[0].depth;
    result.transfer = frame->color_trc == AVCOL_TRC_SMPTE2084 ? 1 : frame->color_trc == AVCOL_TRC_ARIB_STD_B67 ? 2 : 0;
    result.bt2020 = frame->color_primaries == AVCOL_PRI_BT2020 ||
                    (frame->color_primaries == AVCOL_PRI_UNSPECIFIED && frame->colorspace == AVCOL_SPC_BT2020_NCL);
    result.fullRange = frame->color_range == AVCOL_RANGE_JPEG;
    auto matrix = frame->colorspace;
    if (matrix == AVCOL_SPC_UNSPECIFIED)
        matrix = result.bt2020 ? AVCOL_SPC_BT2020_NCL : frame->height <= 576 ? AVCOL_SPC_SMPTE170M : AVCOL_SPC_BT709;
    if (matrix == AVCOL_SPC_BT2020_NCL || matrix == AVCOL_SPC_BT2020_CL)
    {
        result.kr = 0.2627f;
        result.kb = 0.0593f;
    }
    else if (matrix == AVCOL_SPC_BT470BG || matrix == AVCOL_SPC_SMPTE170M)
    {
        result.kr = 0.299f;
        result.kb = 0.114f;
    }
    result.transferName = QString::fromLatin1(av_color_transfer_name(frame->color_trc));
    result.primariesName = QString::fromLatin1(av_color_primaries_name(frame->color_primaries));
    result.matrixName = QString::fromLatin1(av_color_space_name(matrix));
    // MaxCLL 比母版屏幕峰值更接近内容峰值；缺少元数据时采用明确的 1000 nit 回退。
    if (auto* data = av_frame_get_side_data(frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
        data && data->size >= sizeof(AVMasteringDisplayMetadata))
    {
        const auto* metadata = reinterpret_cast<const AVMasteringDisplayMetadata*>(data->data);
        const auto peak = av_q2d(metadata->max_luminance);
        if (metadata->has_luminance && std::isfinite(peak) && peak >= 100 && peak <= 10000)
            result.peakNits = float(peak);
    }
    if (auto* data = av_frame_get_side_data(frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
        data && data->size >= sizeof(AVContentLightMetadata))
    {
        const auto* metadata = reinterpret_cast<const AVContentLightMetadata*>(data->data);
        if (metadata->MaxCLL >= 100 && metadata->MaxCLL <= 10000)
            result.peakNits = float(metadata->MaxCLL);
    }
    return result;
}
