/*
 * HuskarUI
 *
 * Copyright (C) mengps (MenPenS) (MIT License)
 * https://github.com/mengps/HuskarUI
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

#include "Oran7ThemeFunctions.h"
#include "Oran7ColorGenerator.h"
#include "Oran7SizeGenerator.h"
#include "Oran7RadiusGenerator.h"

#include <QtGui/QFontDatabase>

Oran7ThemeFunctions::Oran7ThemeFunctions(QObject *parent)
    : QObject{parent}
{

}

Oran7ThemeFunctions& Oran7ThemeFunctions::instance()
{
    static Oran7ThemeFunctions ins;
    return ins;
}

Oran7ThemeFunctions *Oran7ThemeFunctions::create(QQmlEngine *, QJSEngine *)
{
    return &instance();
}

QList<QColor> Oran7ThemeFunctions::genColor(int preset, bool light, const QColor &background)
{
    return Oran7ColorGenerator::generate(Oran7ColorGenerator::Preset(preset),light,background);
}

QList<QColor> Oran7ThemeFunctions::genColor(const QColor &color, bool light, const QColor &background)
{
    return Oran7ColorGenerator::generate(color,light,background);
}

QList<QString> Oran7ThemeFunctions::genColorString(const QColor &color, bool light, const QColor &background)
{
    QList<QString> result;
    const auto listColor = Oran7ColorGenerator::generate(color,light,background);
    for(const auto &color : listColor)
        result.append(color.name());

    return result;
}

QList<qreal> Oran7ThemeFunctions::genFontSize(qreal fontSizeBase)
{
    return Oran7SizeGenerator::generateFontSize(fontSizeBase);
}

QList<qreal> Oran7ThemeFunctions::genFontLineHeight(qreal fontSizeBase)
{
    return Oran7SizeGenerator::generateFontLineHeight(fontSizeBase);
}

QList<int> Oran7ThemeFunctions::genRadius(int radiusBase)
{
    return Oran7RadiusGenerator::generateRadius(radiusBase);
}

QString Oran7ThemeFunctions::genFontFamily(const QString &fontFamilyBase)
{
    const auto families = fontFamilyBase.split(",");
    const auto fontFamilyDatabase = QFontDatabase::families();
    for(auto family : families)
    {
        auto normalize = family.remove('\'').remove('\"').trimmed();
        if(fontFamilyDatabase.contains(normalize))
        {
            return normalize.trimmed();
        }
    }
    return fontFamilyDatabase.first();
}

QColor Oran7ThemeFunctions::darker(const QColor &color, int factor)
{
    return color.darker(factor);
}

QColor Oran7ThemeFunctions::lighter(const QColor &color, int factor)
{
    return color.lighter(factor);
}

QColor Oran7ThemeFunctions::brightness(const QColor &color, bool light, int lighterFactor, int darkerFactor)
{
    if(!light)
        return darker(color,lighterFactor);
    else
        return lighter(color,lighterFactor);
}

QColor Oran7ThemeFunctions::alpha(const QColor &color, qreal alpha)
{
    return QColor(color.red(),color.green(),color.blue(),alpha * 255);
}

QColor Oran7ThemeFunctions::onBackground(const QColor &color, const QColor &background)
{
    const auto fg = color.toRgb();
    const auto bg = background.toRgb();

    const qreal fgAlpha = fg.alphaF();
    const qreal bgAlpha = bg.alphaF();

    const qreal alpha = fgAlpha + bgAlpha * (1.0 - fgAlpha);

    if (qFuzzyIsNull(alpha)) {
        return QColor::fromRgbF(0, 0, 0, 0);
    }

    const qreal red =
        (fg.redF() * fgAlpha + bg.redF() * bgAlpha * (1.0 - fgAlpha)) / alpha;

    const qreal green =
        (fg.greenF() * fgAlpha + bg.greenF() * bgAlpha * (1.0 - fgAlpha)) / alpha;

    const qreal blue =
        (fg.blueF() * fgAlpha + bg.blueF() * bgAlpha * (1.0 - fgAlpha)) / alpha;

    return QColor::fromRgbF(red, green, blue, alpha);
}

qreal Oran7ThemeFunctions::multiply(qreal num1, qreal num2)
{
    return num1 * num2;
}
