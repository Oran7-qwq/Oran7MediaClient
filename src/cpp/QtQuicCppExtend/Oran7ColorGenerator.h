#ifndef ORAN7COLORGENERATOR_H
#define ORAN7COLORGENERATOR_H

#include <QtCore/QObject>
#include <QtGui/QColor>
#include <QtQml/qqml.h>

class Oran7ColorGenerator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Oran7ColorGenerator)
    QML_SINGLETON
public:
    explicit Oran7ColorGenerator(QObject *parent = nullptr);
    ~Oran7ColorGenerator();

    enum class Preset
    {
        Preset_Red = 1,
        Preset_Volcano,
        Preset_Orange,
        Preset_Gold,
        Preset_Yellow,
        Preset_Lime,
        Preset_Green,
        Preset_Cyan,
        Preset_Blue,
        Preset_Geekblue,
        Preset_Purple,
        Preset_Magenta,
        Preset_Grey
    };
    Q_ENUM(Preset)

    Q_INVOKABLE static QColor reverseColor(const QColor &color);
    Q_INVOKABLE static QColor presetToColor(const QString& color);
    Q_INVOKABLE static QColor presetToColor(Oran7ColorGenerator::Preset color);
    Q_INVOKABLE static QList<QColor> generate(Oran7ColorGenerator::Preset color, bool light = true, const QColor &background = QColor(QColor::Invalid));
    Q_INVOKABLE static QList<QColor> generate(const QColor &color, bool light = true, const QColor &background = QColor(QColor::Invalid));
};

#endif // ORAN7COLORGENERATOR_H
