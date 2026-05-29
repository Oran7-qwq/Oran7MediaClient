#ifndef ORAN7WINDOWAGENT_H
#define ORAN7WINDOWAGENT_H

#include <QtCore/QObject>
#include <QtQml/qqml.h>
#include <QWKQuick/quickwindowagent.h>

class Oran7WindowAgent : public QWK::QuickWindowAgent, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_NAMED_ELEMENT(Oran7WindowAgent)

public:
    explicit Oran7WindowAgent(QObject *parent = nullptr);
    ~Oran7WindowAgent();

    void classBegin() override;
    void componentComplete() override;

signals:

private:
    QQuickWindow *m_window = nullptr;
};

#endif // ORAN7WINDOWAGENT_H
