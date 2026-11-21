#pragma once

#include <QRect>
#include <QRectF>
#include <QSize>
#include <algorithm>
#include <cmath>
#include <utility>

// 中心点相对编码画布。适应模式使用相对等比适应尺寸的缩放倍数。
struct Oran7CaptureComposition
{
    bool enabled = false;
    bool fit = true;
    double centerX = 0.5, centerY = 0.5;
    double width = 1, height = 1, scale = 1;
    QRectF crop{0, 0, 1, 1}; // 旋转和镜像后的归一化坐标。
    int quarterTurns = 0; // 顺时针。
    bool mirrored = false;
    bool removed = false;

    bool swapsAxes() const { return enabled && (quarterTurns & 1); }
    bool transformsPixels() const { return enabled && (quarterTurns || mirrored); }
    QSize inputSize(const QSize& oriented) const { return swapsAxes() ? oriented.transposed() : oriented; }

    // 把旋转、镜像后的像素矩形映回原始输入。QRect 的 right/bottom 为包含端点。
    QRect sourceRect(const QRect& oriented, const QSize& input) const
    {
        const QSize domain = inputSize(input);
        const int x = mirrored ? domain.width() - oriented.x() - oriented.width() : oriented.x();
        const int y = oriented.y(), w = oriented.width(), h = oriented.height();
        switch (quarterTurns)
        {
        case 1: return {y, input.height() - x - w, h, w};
        case 2: return {input.width() - x - w, input.height() - y - h, w, h};
        case 3: return {input.width() - y - h, x, h, w};
        default: return {x, y, w, h};
        }
    }

    struct Oran7Blit
    {
        QRect source;
        QRect destination;
    };

    Oran7Blit blit(const QSize& input, const QSize& output) const
    {
        if (input.isEmpty() || output.isEmpty())
            return {};
        const QRect canvas(QPoint(), output);
        if (!enabled)
            return {QRect(QPoint(), input), canvas};
        if (removed)
            return {};
        const QSize oriented = inputSize(input);
        const double aspect = oriented.width() * crop.width() / (oriented.height() * crop.height());
        const double w = fit ? std::min(double(output.width()), output.height() * aspect) * scale : output.width() * width;
        const double h = fit ? w / aspect : output.height() * height;
        const auto even = [](double value) { return int(std::round(value / 2)) * 2; };
        const int left = even(centerX * output.width() - w / 2);
        const int top = even(centerY * output.height() - h / 2);
        const int right = even(centerX * output.width() + w / 2);
        const int bottom = even(centerY * output.height() + h / 2);
        const QRect layer(left, top, right - left, bottom - top);
        const auto visible = canvas.intersected(layer);
        if (visible.isEmpty())
            return {};

        // NV12 的目标边界对齐到两个像素，按同一个缩放反算源裁剪。
        const auto cropAxis = [&](int offset, int length, int extent, int inputLength, double origin, double span)
        {
            const int start = std::clamp(even((origin + double(offset) / extent * span) * inputLength),
                                         0, std::max(0, inputLength - 2));
            const int end = std::clamp(even((origin + double(offset + length) / extent * span) * inputLength),
                                       std::min(start + 2, inputLength), inputLength);
            return std::pair{start, end - start};
        };
        const auto [sx, sw] = cropAxis(visible.x() - left, visible.width(), layer.width(), oriented.width(),
                                       crop.x(), crop.width());
        const auto [sy, sh] = cropAxis(visible.y() - top, visible.height(), layer.height(), oriented.height(),
                                       crop.y(), crop.height());
        return {sourceRect(QRect(sx, sy, sw, sh), input), visible};
    }
};
