#ifndef GLOBALEVENTFILTER_H
#define GLOBALEVENTFILTER_H

#include <QObject>
#include <QEvent>
#include <QKeyEvent>

class GlobalEventFilter : public QObject
{
    Q_OBJECT
public:
    explicit GlobalEventFilter(QObject *parent = nullptr);
    ~GlobalEventFilter();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void escapeKeyPressed();
    void globalKeyEvent(int key, bool pressed);

private:
    bool m_isEscapeHandled;
    qint64 m_lastEscapeTime; // 用于去重
};

#endif // GLOBALEVENTFILTER_H