#ifndef GLOBALEVENTFILTER_H
#define GLOBALEVENTFILTER_H

#include <QObject>
#include <QEvent>
#include <QKeyEvent>
#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QSet>
#include <QString>
#include <QTimer>

class GlobalEventFilter : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit GlobalEventFilter(QObject *parent = nullptr);
    ~GlobalEventFilter();

    bool eventFilter(QObject *obj, QEvent *event) override;
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void escapeKeyPressed();
    void escapeKeyReleased();
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
    // 发送组合键信号；已注册为全局热键的组合由 WM_HOTKEY 触发，此处跳过避免重复
    void emitCombo(const QString &comboName);
    void handleKeyPress(int key);
    void handleKeyRelease(int key);

    // ---- 全局热键（RegisterHotKey，主窗口失焦也触发；仅注册的组合走全局，其它走 Qt GUI）----
    void registerGlobalHotkeys();
    QSet<QString> m_globalHotkeys;        // 已注册为全局热键的组合名
    QHash<int, QString> m_hotkeyIdToName; // WM_HOTKEY 的 id → 组合名

    // WM_HOTKEY 在原生消息泵里只记 pending，由该定时器在事件循环安全阶段发出信号，
    // 避免在原生消息处理中途 emit 触发 QML/Client 重逻辑导致崩溃
    QTimer m_hotkeyDispatchTimer;
    QString m_pendingHotkey;

    bool __GlobelKeyLogINFO__ = true;
    bool __GlobelKeyLogINFO__Detail__ = false;
};

#endif // GLOBALEVENTFILTER_H