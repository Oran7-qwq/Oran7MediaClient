#ifndef ORAN7SIZEGENERATOR_H
#define ORAN7SIZEGENERATOR_H

#include <QObject>
#include <QtGui/QColor>
#include <QtQml/qqml.h>

class Oran7SizeGenerator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Oran7SizeGenerator)
public:
    explicit Oran7SizeGenerator(QObject *parent = nullptr);
    ~Oran7SizeGenerator();

    Q_INVOKABLE static QList<qreal> generateFontSize(qreal fontSizeBase);
    Q_INVOKABLE static QList<qreal> generateFontLineHeight(qreal fontSizeBase);
signals:
};

#endif // ORAN7SIZEGENERATOR_H
