#ifndef ORAN7THEMEFUNCTIONS_H
#define ORAN7THEMEFUNCTIONS_H

#include <QtCore/QObject>
#include <QtGui/QColor>
#include <QtQml/qqml.h>

class Oran7ThemeFunctions : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Oran7ThemeFunctions)
public:
    static Oran7ThemeFunctions& instance();
    static Oran7ThemeFunctions *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE static QList<QColor> genColor(int preset, bool light = true, const QColor &background = QColor(QColor::Invalid));
    Q_INVOKABLE static QList<QColor> genColor(const QColor &color, bool light = true, const QColor &background = QColor(QColor::Invalid));
    Q_INVOKABLE static QList<QString> genColorString(const QColor &color, bool light = true, const QColor &background = QColor(QColor::Invalid));
    Q_INVOKABLE static QList<qreal> genFontSize(qreal fontSizeBase);
    Q_INVOKABLE static QList<qreal> genFontLineHeight(qreal fontSizeBase);
    Q_INVOKABLE static QList<int> genRadius(int radiusBase);
    Q_INVOKABLE static QString genFontFamily(const QString &fontFamilyBase);
    Q_INVOKABLE static QColor darker(const QColor &color, int factor = 140);
    Q_INVOKABLE static QColor lighter(const QColor &color, int factor = 140);
    Q_INVOKABLE static QColor brightness(const QColor &color, bool light = true, int lighterFactor = 140, int darkerFactor = 140);
    Q_INVOKABLE static QColor alpha(const QColor &color, qreal alpha = 0.5);
    Q_INVOKABLE static QColor onBackground(const QColor &color, const QColor &background);
    Q_INVOKABLE static qreal multiply(qreal num1, qreal num2);
private:
    Oran7ThemeFunctions(QObject *parent = nullptr);
    ~Oran7ThemeFunctions() = default;
};

#endif // ORAN7THEMEFUNCTIONS_H
