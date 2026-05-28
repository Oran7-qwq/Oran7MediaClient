#ifndef WINDOWAPI_H
#define WINDOWAPI_H

#include <QObject>
#include <QWindow>
#include <QtQml/qqml.h>

class WindowApi : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(WindowApi)

public:
    ~WindowApi();

    static WindowApi& instance();
    static WindowApi *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE void setWindowMinimized(QWindow *window);
    Q_INVOKABLE void setWindowMaximized(QWindow *window);
    Q_INVOKABLE void setWindowNormal(QWindow *window);
    Q_INVOKABLE void setWindowState(QWindow *window, int state);
    Q_INVOKABLE void setWindowStaysOnTopHint(QWindow *window, bool hint);

private:
    explicit WindowApi(QObject *parent = nullptr);
};

#endif // WINDOWAPI_H
