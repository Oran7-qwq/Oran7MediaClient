#ifndef ORAN7RADIUSGENERATOR_H
#define ORAN7RADIUSGENERATOR_H

#include <QObject>
#include <QtGui/QColor>
#include <QtQml/qqml.h>

class Oran7RadiusGenerator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Oran7RadiusGenerator)
public:
    explicit Oran7RadiusGenerator(QObject *parent = nullptr);
    ~Oran7RadiusGenerator();

    Q_INVOKABLE static QList<int> generateRadius(int radiusBase);

signals:
};

#endif // ORAN7RADIUSGENERATOR_H
