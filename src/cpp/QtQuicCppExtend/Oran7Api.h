#ifndef ORAN7API_H
#define ORAN7API_H

#include <QObject>
#include <QtQml/qqml.h>
#include <QtGui/QWindow>
#include <QVariantList>

class QScreen;

class Oran7Api : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Oran7Api)
    Q_PROPERTY(QVariantList screenWorkAreas READ screenWorkAreas NOTIFY screenWorkAreasChanged)
    Q_PROPERTY(QVariantList screenGeometries READ screenGeometries NOTIFY screenWorkAreasChanged)

public:
    ~Oran7Api();

    static Oran7Api *instance();
    static Oran7Api *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE void setWindowStaysOnTopHint(QWindow *window, bool hint);
    Q_INVOKABLE void setWindowState(QWindow *window, int state);

    Q_INVOKABLE void setAlwaysOnTop(QWindow *window, bool alwaysOnTop);

    Q_INVOKABLE int getWindowState(QWindow *window) const;
    Q_INVOKABLE void setWindowMaskRect(QWindow *window, const QRectF& rect);
    QVariantList screenWorkAreas() const;
    QVariantList screenGeometries() const;

signals:
    void screenWorkAreasChanged();

private:
    explicit Oran7Api(QObject *parent = nullptr);
    void observeScreen(QScreen* screen);
};

#endif // ORAN7API_H
