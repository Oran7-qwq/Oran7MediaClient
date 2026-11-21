#include "Oran7CaptureOutputRouter.h"
#include <QElapsedTimer>
#include <QImage>
#include <QTransform>
#include <algorithm>
#include <array>
#include <cstring>

namespace
{
constexpr double redWeight = 0.2126, greenWeight = 0.7152, blueWeight = 0.0722;

double luma(double red, double green, double blue)
{
    return redWeight * red + greenWeight * green + blueWeight * blue;
}

uint8_t over(uint8_t foreground, uint8_t background, uint8_t alpha, int neutral)
{
    if (alpha == 0)
        return background;
    if (alpha == 255)
        return foreground;
    // 预乘的 YUV 保留视频黑位和色度中点，合成前抵消该偏移。
    const int remainder = (background * (255 - alpha) + neutral * alpha + 127) / 255;
    return uint8_t(std::clamp(foreground + remainder - neutral, 0, 255));
}
}

void Oran7CaptureOutputRouter::ImageOverlay::prepare(const SceneInput& layer,
                                                    const Oran7CaptureComposition::Oran7Blit& blit)
{
    const int turns = layer.composition.enabled ? layer.composition.quarterTurns : 0;
    const bool mirror = layer.composition.enabled && layer.composition.mirrored;
    if (source == layer.auxiliaryFrame && crop == blit.source && size == blit.destination.size() &&
        quarterTurns == turns && mirrored == mirror)
        return;

    source = layer.auxiliaryFrame;
    crop = blit.source;
    size = blit.destination.size();
    quarterTurns = turns;
    mirrored = mirror;
    opaque = visible = false;
    if (source->format != AV_PIX_FMT_BGRA || !source->data[0] || (size.width() & 1) || (size.height() & 1))
        return;

    const QImage original(static_cast<const uchar*>(source->data[0]), source->width, source->height,
                          source->linesize[0], QImage::Format_ARGB32_Premultiplied);
    auto pixels = crop == original.rect() ? original : original.copy(crop);
    if (quarterTurns)
        pixels = pixels.transformed(QTransform().rotate(90 * quarterTurns));
    if (mirrored)
        pixels = pixels.mirrored(true, false);
    pixels = pixels.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (pixels.isNull())
        return;

    const int width = size.width();
    const auto count = std::size_t(width) * size.height();
    y.resize(count);
    uv.resize(count / 2);
    alpha.resize(count);
    chromaAlpha.resize(count / 4);
    opaque = true;
    for (int row = 0; row < size.height(); row += 2)
    {
        const auto* top = reinterpret_cast<const QRgb*>(pixels.constScanLine(row));
        const auto* bottom = reinterpret_cast<const QRgb*>(pixels.constScanLine(row + 1));
        const auto lumaOffset = std::size_t(row) * width;
        const auto chromaOffset = lumaOffset / 2;
        for (int column = 0; column < width; column += 2)
        {
            const QRgb block[] = {top[column], top[column + 1], bottom[column], bottom[column + 1]};
            const auto offset = lumaOffset + column;
            const std::size_t samples[] = {offset, offset + 1, offset + width, offset + width + 1};
            int red = 0, green = 0, blue = 0, coverage = 0;
            for (int index = 0; index < 4; ++index)
            {
                const auto pixel = block[index];
                y[samples[index]] = uint8_t(16 + qRound(luma(qRed(pixel), qGreen(pixel), qBlue(pixel)) * 219 / 255));
                alpha[samples[index]] = uint8_t(qAlpha(pixel));
                opaque &= qAlpha(pixel) == 255;
                red += qRed(pixel);
                green += qGreen(pixel);
                blue += qBlue(pixel);
                coverage += qAlpha(pixel);
            }
            visible |= coverage > 0;
            // NV12 每个 UV 对覆盖四个像素。颜色和 Alpha 使用同一组采样。
            const auto chroma = chromaOffset + column;
            const double brightness = luma(red, green, blue);
            uv[chroma] = uint8_t(128 + qRound((blue - brightness) * 112 / (1020 * (1 - blueWeight))));
            uv[chroma + 1] = uint8_t(128 + qRound((red - brightness) * 112 / (1020 * (1 - redWeight))));
            chromaAlpha[chroma / 2] = uint8_t((coverage + 2) / 4);
        }
    }
}

void Oran7CaptureOutputRouter::ImageOverlay::blend(AVFrame& destination, const QPoint& origin) const
{
    const int width = size.width();
    for (int plane = 0; plane < 2; ++plane)
    {
        const int step = plane + 1;
        const auto& pixels = plane ? uv : y;
        const auto& coverage = plane ? chromaAlpha : alpha;
        const int neutral = plane ? 128 : 16;
        for (int row = 0; row < size.height() / step; ++row)
        {
            auto* dst = destination.data[plane] + (origin.y() / step + row) * destination.linesize[plane] + origin.x();
            const auto* src = pixels.data() + std::size_t(row) * width;
            if (opaque)
            {
                std::memcpy(dst, src, width);
                continue;
            }
            const auto* mask = coverage.data() + std::size_t(row) * width / step;
            for (int column = 0; column < width; ++column)
                dst[column] = over(src[column], dst[column], mask[column / step], neutral);
        }
    }
}

Oran7CaptureFrameConverter::Frame Oran7CaptureOutputRouter::composeScene(
    Route& route, const AVFrame* source, const QSize& canvas, const Oran7CaptureComposition& fallback,
    const std::vector<SceneInput>& layers)
{
    for (auto it = route.images.begin(); it != route.images.end();)
    {
        const bool active = std::any_of(layers.begin(), layers.end(), [&](const SceneInput& layer) {
            return layer.image && !layer.composition.removed && layer.auxiliaryFrame.get() == it->first;
        });
        if (active)
            ++it;
        else
            it = route.images.erase(it);
    }

    auto& converter = *route.converter;
    const int width = canvas.width(), height = canvas.height();
    if (layers.empty())
        return converter.recording(source, width, height, fallback);
    if (layers.size() == 1 && layers.front().primary)
        return converter.recording(source, width, height, layers.front().composition);

    const auto blackCanvas = [&] {
        Oran7CaptureComposition blank;
        blank.enabled = blank.removed = true;
        return converter.recording(source, width, height, blank);
    };
    Oran7CaptureFrameConverter::Frame result;
    for (const auto& layer : layers)
    {
        if (layer.composition.removed)
            continue;
        const auto* input = layer.primary ? source : layer.auxiliaryFrame.get();
        if (!input)
            continue;
        const auto blit = layer.composition.blit(QSize(input->width, input->height), canvas);
        const auto& area = blit.destination;
        if (area.isEmpty())
            continue;
        if (layer.image)
        {
            auto& image = route.images[input];
            image.prepare(layer, blit);
            if (!image.visible)
                continue;
            if (!result)
                result = blackCanvas();
            if (result)
                image.blend(*result, area.topLeft());
            continue;
        }
        auto frame = converter.recording(input, width, height, layer.composition);
        if (!frame)
            continue;
        if (!result || area == QRect(QPoint(), canvas))
        {
            result = std::move(frame);
            continue;
        }

        // 捕获源只覆盖其目的矩形，矩形外保留下方图层。
        for (int plane = 0; plane < 2; ++plane)
            for (int y = area.y() / (plane + 1); y < (area.y() + area.height()) / (plane + 1); ++y)
                std::memcpy(result->data[plane] + y * result->linesize[plane] + area.x(),
                            frame->data[plane] + y * frame->linesize[plane] + area.x(), area.width());
    }
    if (result)
        return result;
    return blackCanvas();
}

Oran7CaptureOutputRouter::Statistics Oran7CaptureOutputRouter::submit(const AVFrame* source, int64_t nowUs)
{
    Statistics stats;
    const auto outputs = m_feed->outputs();
    for (auto it = m_routes.begin(); it != m_routes.end();)
    {
        if (std::none_of(outputs.begin(), outputs.end(), [&](const auto& o) { return o.id == it->first; }))
            it = m_routes.erase(it);
        else
            ++it;
    }
    if (!source)
        return stats;

    struct Oran7DueOutput
    {
        const Oran7RecordingFeed::Oran7Output* output;
        Route* route;
    };
    // Feed 最多同时保留两个订阅。
    std::array<Oran7DueOutput, 2> due{};
    std::size_t dueCount = 0;
    for (const auto& output : outputs)
    {
        if (!output.encoder->isRuning())
            continue;
        auto [it, inserted] = m_routes.try_emplace(output.id);
        auto& route = it->second;
        if (inserted)
            route.nextUs = output.startUs;
        if (nowUs + 1000 >= route.nextUs)
            due[dueCount++] = {&output, &route};
    }

    if (!dueCount)
        return stats;
    const auto composition = m_feed->composition();
    std::vector<SceneInput> scene;
    if (m_feed->scene())
    {
        const auto layers = m_feed->scene()->layers();
        std::map<const Oran7SceneFrameMailbox*, std::shared_ptr<AVFrame>> sampled;
        scene.reserve(layers.size());
        for (const auto& layer : layers)
        {
            std::shared_ptr<AVFrame> auxiliary;
            if (!layer.primary && !layer.composition.removed && layer.frames)
            {
                auto [frame, inserted] = sampled.try_emplace(layer.frames.get());
                if (inserted)
                    frame->second = layer.frames->latest();
                auxiliary = frame->second;
            }
            scene.push_back({layer.composition, std::move(auxiliary), layer.primary, layer.image});
        }
    }
    const bool sharePixels = dueCount == 2 && due[0].output->width == due[1].output->width
                             && due[0].output->height == due[1].output->height;
    Oran7CaptureFrameConverter::Frame secondFrame;
    for (std::size_t index = 0; index < dueCount; ++index)
    {
        const auto& output = *due[index].output;
        auto& route = *due[index].route;
        Oran7CaptureFrameConverter::Frame frame;
        if (sharePixels && index == 1)
        {
            frame = std::move(secondFrame);
        }
        else
        {
            if (!route.converter)
            {
                route.converter = std::make_unique<Oran7CaptureFrameConverter>(m_device, output.fps, m_download);
                route.converter->beginRecording();
            }
            QElapsedTimer conversion;
            conversion.start();
            frame = composeScene(route, source, QSize(output.width, output.height), composition, scene);
            stats.conversionNs += conversion.nsecsElapsed();
            ++stats.conversions;

            // 先复制帧头再交付，编码线程各自修改时间戳和关键帧标记，NV12 像素通过 AVBufferRef 共享。
            if (sharePixels && frame)
                secondFrame.reset(av_frame_clone(frame.get()));
        }
        if (!frame)
            continue;
        auto pts = std::max<int64_t>(0, (nowUs - output.startUs) * 90000 / 1000000);
        if (route.lastPts != AV_NOPTS_VALUE && pts <= route.lastPts)
            pts = route.lastPts + 1;
        route.lastPts = pts;
        frame->pts = pts;
        frame->duration = av_rescale_q(1, {1, output.fps}, {1, 90000});
        if (pts == 0)
            frame->pict_type = AV_PICTURE_TYPE_I;
        route.nextUs = std::max(route.nextUs + 1000000 / output.fps, nowUs);
        if (m_feed->deliver(output.id, frame.release()))
            ++stats.submitted;
    }
    return stats;
}
