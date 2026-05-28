#include "oran7sizegenerator.h"

Oran7SizeGenerator::Oran7SizeGenerator(QObject *parent)
    : QObject{parent}
{

}

Oran7SizeGenerator::~Oran7SizeGenerator()
{

}

QList<qreal> Oran7SizeGenerator::generateFontSize(qreal fontSizeBase)
{
    QList<qreal> fontSizes(10);
    for (int index = 0; index < 10; index++) {
        const auto i = index - 1;
        const auto baseSize = fontSizeBase * std::exp(i / 5.0);
        const auto intSize = (i + 1) > 1 ? std::floor(baseSize) : std::ceil(baseSize);
        // Convert to even
        fontSizes[index] = std::floor(intSize / 2) * 2;
    }
    fontSizes[1] = fontSizeBase;

    return fontSizes;
}

QList<qreal> Oran7SizeGenerator::generateFontLineHeight(qreal fontSizeBase)
{
    QList<qreal> fontLineHeights = generateFontSize(fontSizeBase);
    for (int index = 0; index < 10; index++) {
        auto fontSize = fontLineHeights[index];
        fontLineHeights[index] = (fontSize + 8) / fontSize;
    }

    return fontLineHeights;
}
