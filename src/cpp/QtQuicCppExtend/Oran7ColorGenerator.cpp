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

#include "Oran7ColorGenerator.h"
#include <QtCore/QHash>
#include <QtCore/qmath.h>

static const auto g_hueStep = 2; // 色相阶梯
static const auto g_saturationStep = 0.16; // 饱和度阶梯，浅色部分
static const auto g_saturationStep2 = 0.05; // 饱和度阶梯，深色部分
static const auto g_brightnessStep1 = 0.05; // 亮度阶梯，浅色部分
static const auto g_brightnessStep2 = 0.15; // 亮度阶梯，深色部分
static const auto g_lightColorCount = 5; // 浅色数量，主色上
static const auto g_darkColorCount = 4; // 深色数量，主色下

[[maybe_unused]] static auto qHash(Oran7ColorGenerator::Preset preset)
{
    return static_cast<std::underlying_type<Oran7ColorGenerator::Preset>::type>(preset);
}

/**
 * @brief mix
 * @param rgb1
 * @param rgb2
 * @param amount : 0 - 100 /rate
 * @return
 */
static QColor mix(const QColor &rgb1,const QColor &rgb2,int amount)
{
    qreal rate = qreal(amount) / 100.0;
    const QColor rgb = QColor::fromRgbF(
        (rgb2.redF() - rgb1.redF()) * rate + rgb1.redF(),
        (rgb2.greenF() - rgb1.greenF()) * rate + rgb1.greenF(),
        (rgb2.blueF() - rgb1.blueF()) * rate + rgb1.blueF()
    );
    return rgb;
}

/**
 * @brief
 *  0°       红色
 *  60°     黄色
 *  120°   绿色
 *  180°   青色
 *  240°   蓝色
 *  300°   紫色
 *  360°   回到红色
 * @param hsv
 * @param i 色阶偏移量
 * @param light 是否获取亮色
 * @return
 */
static qreal getHue(const QColor &hsv, int  i, bool light = false)
{
    // 处理灰度颜色（hsvHue() 返回 -1）
    if (hsv.hsvHue() < 0)
        return 0.0;

    qreal hue;
    if(std::round(hsv.hsvHue()) >= 60 && std::round(hsv.hsvHue()) <= 240)
        hue = light ? hsv.hsvHue() - g_hueStep * i : hsv.hsvHue() + g_hueStep * i;
    else
        hue = light ? hsv.hsvHue() + g_hueStep * i :hsv.hsvHue() - g_hueStep * i;

    if(hue < 0)
        hue += 360;
    else if(hue >= 360)
        hue -= 360;

    // fromHsvF 期望 0-1 的归一化值
    return hue / 360.0;
}

/**
 * @brief getSaturation
 * @param hsv
 * @param i
 * @param light
 * @return
 */
static qreal getSaturation(const QColor &hsv, int i ,bool light = false)
{
    // grey color don't change saturation
    if (hsv.hsvHue() == -1 || hsv.hsvSaturation() == 0) {
        return hsv.hsvSaturationF();
    }

    qreal saturation;
    if(light)
        saturation = hsv.hsvSaturationF() - g_saturationStep * i;
    else if(i == g_darkColorCount)
        saturation = hsv.hsvSaturationF() + g_saturationStep;
    else
        saturation = hsv.hsvSaturationF() + g_saturationStep2 * i;

    // 边界值修正
    if (saturation > 1) {
        saturation = 1;
    }
    // 第一格的 s 限制在 0.06-0.1 之间
    if (light && i == g_lightColorCount && saturation > 0.1) {
        saturation = 0.1;
    }
    if (saturation < 0.06) {
        saturation = 0.06;
    }
    return saturation;
}

static qreal getValue(const QColor &hsv, int i, bool light = false)
{
    qreal value;
    if (light) {
        value = hsv.valueF() + g_brightnessStep1 * i;
    } else {
        value = hsv.valueF() - g_brightnessStep2 * i;
    }
    if (value > 1) {
        value = 1;
    }

    return value;
}

Oran7ColorGenerator::Oran7ColorGenerator(QObject *parent)
    : QObject{parent}
{}

Oran7ColorGenerator::~Oran7ColorGenerator()
{

}

QColor Oran7ColorGenerator::reverseColor(const QColor &color)
{
    return QColor(255 - color.red(), 255 - color.green(), 255 - color.blue(), color.alpha());
}

QColor Oran7ColorGenerator::presetToColor(const QString &color)
{
    using PresetTableType = QHash<QString,QColor>;
    static PresetTableType g_presetTable{
        { QString("#Preset_Red"),      QColor(0xF5222D) },
        { QString("#Preset_Volcano"),  QColor(0xFA541C) },
        { QString("#Preset_Orange"),   QColor(0xFA8C16) },
        { QString("#Preset_Gold"),     QColor(0xFAAD14) },
        { QString("#Preset_Yellow"),   QColor(0xFADB14) },
        { QString("#Preset_Lime"),     QColor(0xA0D911) },
        { QString("#Preset_Green"),    QColor(0x52C41A) },
        { QString("#Preset_Cyan"),     QColor(0x13C2C2) },
        { QString("#Preset_Blue"),     QColor(0x1677FF) },
        { QString("#Preset_Geekblue"), QColor(0x2F54EB) },
        { QString("#Preset_Purple"),   QColor(0x722ED1) },
        { QString("#Preset_Magenta"),  QColor(0xEB2F96) },
        { QString("#Preset_Grey"),     QColor(0x666666) }
    };

    if(g_presetTable.contains(color))
        return g_presetTable[color];
    else
        return QColor(QColor::Invalid);
}

QColor Oran7ColorGenerator::presetToColor(Preset color)
{
    using PresetTableType = QHash<Oran7ColorGenerator::Preset,QColor>;
    static PresetTableType g_presetTable{
        {Oran7ColorGenerator::Preset::Preset_Red,QColor(0xF5222D)},
        {Oran7ColorGenerator::Preset::Preset_Volcano,QColor(0xFA541C)},
        {Oran7ColorGenerator::Preset::Preset_Orange,QColor(0xFA8C16)},
        {Oran7ColorGenerator::Preset::Preset_Gold,QColor(0xFAAD14)},
        {Oran7ColorGenerator::Preset::Preset_Yellow,QColor(0xFADB14)},
        {Oran7ColorGenerator::Preset::Preset_Lime,QColor(0xA0D911)},
        {Oran7ColorGenerator::Preset::Preset_Green,QColor(0x52C41A)},
        {Oran7ColorGenerator::Preset::Preset_Cyan,QColor(0x13C2C2)},
        {Oran7ColorGenerator::Preset::Preset_Blue,QColor(0x1677FF)},
        {Oran7ColorGenerator::Preset::Preset_Geekblue,QColor(0x2F54EB)},
        {Oran7ColorGenerator::Preset::Preset_Purple,QColor(0x722ED1)},
        {Oran7ColorGenerator::Preset::Preset_Magenta,QColor(0xEB2F96)},
        {Oran7ColorGenerator::Preset::Preset_Grey,QColor(0x666666)}
    };

    if(g_presetTable.contains(color))
        return g_presetTable[color];
    else
        return QColor(QColor::Invalid);
}

QList<QColor> Oran7ColorGenerator::generate(Preset color, bool light, const QColor &background)
{
    return generate(presetToColor(color),light,background);
}

QList<QColor> Oran7ColorGenerator::generate(const QColor &color,bool light,const QColor &background)
{
    QList<QColor> patterns;

    if (!color.isValid()) {
        return patterns;
    }

    patterns.reserve(10);

    const auto hsv = color.toHsv();

    // light colors: 1 - 5
    for (int i = g_lightColorCount; i > 0; --i) {
        const QColor generatedColor = QColor::fromHsvF(
            getHue(hsv, i, true),
            getSaturation(hsv, i, true),
            std::max(getValue(hsv, i, true), 0.0)
            );

        patterns.append(generatedColor);
    }

    // base color: 6
    patterns.append(color);

    // dark colors: 7 - 10
    for (int i = 1; i <= g_darkColorCount; ++i) {
        const QColor generatedColor = QColor::fromHsvF(
            getHue(hsv, i, false),
            getSaturation(hsv, i, false),
            std::max(getValue(hsv, i, false), 0.0)
            );

        patterns.append(generatedColor);
    }

    if (light) {
        return patterns;
    }

    static const std::list<std::tuple<int, int>> g_darkColorMap = {
        std::make_tuple(7, 15),
        std::make_tuple(6, 25),
        std::make_tuple(5, 30),
        std::make_tuple(5, 45),
        std::make_tuple(5, 65),
        std::make_tuple(5, 85),
        std::make_tuple(4, 90),
        std::make_tuple(3, 95),
        std::make_tuple(2, 97),
        std::make_tuple(1, 98)
    };

    QList<QColor> darkPatterns;
    darkPatterns.reserve(10);

    const QColor bg = background.isValid()
                          ? background
                          : QColor(0x141414);

    for (const auto &item : g_darkColorMap) {
        const auto [index, amount] = item;
        if (index < 0 || index >= patterns.size()) {
            continue;
        }
        darkPatterns.append(mix(bg, patterns.at(index), amount));
    }

    return darkPatterns;
}
