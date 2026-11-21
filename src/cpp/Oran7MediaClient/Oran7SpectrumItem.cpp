#include "Oran7SpectrumItem.h"

#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <algorithm>
#include <cmath>

namespace {
constexpr int contourBars = 16;
constexpr int tailBars = 4;
constexpr float baselineHeight = 4.f;
}

Oran7SpectrumItem::Oran7SpectrumItem(QQuickItem* parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setAcceptedMouseButtons(Qt::NoButton);
}

void Oran7SpectrumItem::setVisualizer(Oran7AudioVisualizer* visualizer)
{
    if (m_visualizer == visualizer) return;
    if (m_visualizer) disconnect(m_visualizer, nullptr, this, nullptr);
    m_visualizer = visualizer;
    if (visualizer) {
        connect(visualizer, &Oran7AudioVisualizer::frameReady, this, &Oran7SpectrumItem::update);
        connect(visualizer, &Oran7AudioVisualizer::layoutChanged, this, &Oran7SpectrumItem::update);
        connect(visualizer, &Oran7AudioVisualizer::settingsChanged, this, &Oran7SpectrumItem::update);
    }
    emit visualizerChanged();
    update();
}

void Oran7SpectrumItem::setBarColor(const QColor& color)
{
    if (m_color == color) return;
    m_color = color;
    emit barColorChanged();
    update();
}

void Oran7SpectrumItem::geometryChange(const QRectF& now, const QRectF& old)
{
    QQuickItem::geometryChange(now, old);
    update();
}

float Oran7SpectrumItem::shapeLeftHeight(int index, float height, float sustainedHead, float tailHeight)
{
    const float span = sustainedHead - tailHeight;
    if (index <= 0 || index >= contourBars || span <= 0.f) return height;

    constexpr float profile[contourBars + 1] = {
        1.f, .83f, .66f, .51f, .41f, .33f, .27f, .225f,
        .185f, .15f, .12f, .094f, .071f, .050f, .032f, .016f, 0.f
    };
    const float contour = tailHeight + span * profile[index];
    // 保留本频段的小幅变化，收住会在轮廓上形成缺口或突刺的大落差。
    const float deviation = height - contour;
    const float neighboringDrop = std::min(profile[index - 1] - profile[index], profile[index] - profile[index + 1]);
    const float detailLimit = std::max(.01f, span * std::min(.075f, .59f * neighboringDrop));
    const float detail = deviation * (detailLimit / (detailLimit + std::abs(deviation)));
    // 低频与后段接近时逐渐退出整形，避免跨过强度边界后突降。
    const float t = std::min(1.f, span / std::max(8.f, tailHeight * .60f));
    const float activation = t * t * (3.f - 2.f * t);
    const float blend = .88f * activation * std::min(1.f, float(contourBars - index) / 6.f);
    return height + (contour + detail - height) * blend;
}

float Oran7SpectrumItem::pulseHeightScale(int index, int bars, float pulse)
{
    if (index <= 0 || bars <= 1 || pulse == 0.f) return 1.f;

    // 低频到中段随整体强弱起伏，向高频侧渐弱，第一柱的鼓点保持独立。
    const float position = float(index) / (bars - 1);
    const float t = std::clamp((position - .24f) / .48f, 0.f, 1.f);
    return 1.f + .60f * pulse * (1.f - t * t * (3.f - 2.f * t));
}

QSGNode* Oran7SpectrumItem::updatePaintNode(QSGNode* old, UpdatePaintNodeData*)
{
    // Qt 同步场景图时 GUI 线程停在帧边界，此处只读 GUI 快照。
    if (!m_visualizer || m_visualizer->barCount() <= 0 || width() <= 0 || height() <= 0) {
        delete old;
        return nullptr;
    }
    const auto& config = m_visualizer->config();
    const auto& levels = m_visualizer->levels();
    const float pulse = m_visualizer->musicPulse();
    const int bars = m_visualizer->barCount();
    auto* node = static_cast<QSGGeometryNode*>(old);
    if (!node) {
        node = new QSGGeometryNode;
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }
    auto* geometry = node->geometry();
    constexpr int verticesPerBar = 12;
    if (geometry->vertexCount() != bars * verticesPerBar) geometry->allocate(bars * verticesPerBar);
    auto* vertices = geometry->vertexDataAsColoredPoint2D();
    const float bar = float(std::min(qreal(config.barWidth), width() / bars));
    const float gap = config.stretchToEdges && bars > 1
        ? float((width() - bars * bar) / (bars - 1)) : float(config.barGap);
    const float origin = float(std::max(0.0, (width() - bars * bar - (bars - 1) * gap) / 2));
    const float bottom = float(height());
    const float uncappedHeight = float(m_visualizer->uncappedOverlayHeight());
    const auto barHeightAt = [&](int index) {
        return std::min(uncappedHeight, config.height * Oran7AudioVisualizer::barHeightScale(index));
    };
    const auto rangeAt = [](float barHeight) { return std::max(0.f, barHeight - baselineHeight); };
    float sustainedHead = 0.f;
    float tailHeight = 0.f;
    if (bars >= contourBars + tailBars && !levels.isEmpty()) {
        // 两端都跟随持续高度，短促的鼓点与其他频段的瞬态不牵动整条曲线。
        sustainedHead = rangeAt(barHeightAt(0)) * levels.constFirst().sustained;
        for (int i = contourBars; i < contourBars + tailBars; ++i)
            tailHeight += rangeAt(barHeightAt(i)) * levels.value(i).sustained * pulseHeightScale(i, bars, pulse);
        tailHeight /= tailBars;
    }
    // VertexColorMaterial 要求预乘颜色。
    const auto colorAt = [&](float opacity) {
        return qPremultiply(qRgba(m_color.red(), m_color.green(), m_color.blue(),
                                 qRound(m_color.alpha() * config.opacity * opacity)));
    };
    const QRgb baseline = colorAt(.38f);
    for (int i = 0; i < bars; ++i) {
        const float x = origin + (bar + gap) * i;
        const auto level = levels.value(i);
        const float barHeight = barHeightAt(i);
        const float minimumHeight = std::min({baselineHeight, barHeight, bottom});
        const float baseTop = bottom - minimumHeight;
        const float range = rangeAt(barHeight);
        const float animatedHeight = range * level.height() * pulseHeightScale(i, bars, pulse);
        const float peakHeight = shapeLeftHeight(i, animatedHeight, sustainedHead, tailHeight);
        // 限制只裁掉超过上限的部分，低于上限的柱高保持自己的幅度。
        const float peakTop = baseTop - std::min({range, peakHeight, baseTop});
        const auto vertex = [&](int index, float vx, float vy, QRgb color) {
            vertices[index].set(vx, vy, qRed(color), qGreen(color), qBlue(color), qAlpha(color));
        };
        const auto quad = [&](float from, float to, QRgb topColor, QRgb bottomColor) {
            vertex(0, x, from, topColor);
            vertex(1, x, to, bottomColor);
            vertex(2, x + bar, from, topColor);
            vertex(3, x + bar, from, topColor);
            vertex(4, x, to, bottomColor);
            vertex(5, x + bar, to, bottomColor);
            vertices += 6;
        };
        quad(baseTop, bottom, baseline, baseline);
        quad(peakTop, baseTop, colorAt(.56f + .44f * level.transient), colorAt(.38f + .22f * level.transient));
    }
    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
