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
    void enterKeyPressed();
    void globalKeyEvent(int key, bool pressed);
    void keyCombinationTriggered(const QString &comboName);
    void upKeyPressed();
    void downKeyPressed();
    void leftKeyPressed();
    void rightKeyPressed();
    void upKeyReleased();
    void downKeyReleased();
    void leftKeyReleased();
    void rightKeyReleased();

private:
    QHash<int, bool> m_pressedKeys;

    void checkKeyCombinations();
    void handleKeyPress(QKeyEvent *keyEvent);
    void handleKeyRelease(QKeyEvent *keyEvent);

    bool __GlobelKeyLogINFO__ = false;
};

#endif // GLOBALEVENTFILTER_H
