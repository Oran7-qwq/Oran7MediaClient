#pragma once

#include "Oran7AudioVisualizer.h"
#include <QColor>
#include <QQuickItem>

class Oran7SpectrumItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(Oran7AudioVisualizer* visualizer READ visualizer WRITE setVisualizer NOTIFY visualizerChanged)
    Q_PROPERTY(QColor barColor READ barColor WRITE setBarColor NOTIFY barColorChanged)

public:
    explicit Oran7SpectrumItem(QQuickItem* parent = nullptr);
    Oran7AudioVisualizer* visualizer() const { return m_visualizer; }
    void setVisualizer(Oran7AudioVisualizer* visualizer);
    QColor barColor() const { return m_color; }
    void setBarColor(const QColor& color);

signals:
    void visualizerChanged();
    void barColorChanged();

protected:
    static float shapeLeftHeight(int index, float height, float sustainedHead, float tailHeight);
    static float pulseHeightScale(int index, int bars, float pulse);
    QSGNode* updatePaintNode(QSGNode* node, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& now, const QRectF& old) override;

private:
    QPointer<Oran7AudioVisualizer> m_visualizer;
    QColor m_color = Qt::white;
};
