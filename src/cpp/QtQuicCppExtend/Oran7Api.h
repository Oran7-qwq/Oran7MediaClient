#ifndef ORAN7API_H
#define ORAN7API_H

#include <QObject>
#include <QtQml/qqml.h>
#include <QtGui/QWindow>

class Oran7Api : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Oran7Api)

public:
    ~Oran7Api();

    static Oran7Api *instance();
    static Oran7Api *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE void setWindowStaysOnTopHint(QWindow *window, bool hint);
    Q_INVOKABLE void setWindowState(QWindow *window, int state);

    Q_INVOKABLE void setAlwaysOnTop(QWindow *window, bool alwaysOnTop);

    Q_INVOKABLE int getWindowState(QWindow *window) const;

private:
    explicit Oran7Api(QObject *parent = nullptr);
};

#endif // ORAN7API_H
