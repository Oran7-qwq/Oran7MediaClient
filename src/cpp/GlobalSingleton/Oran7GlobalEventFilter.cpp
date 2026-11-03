#include "Oran7GlobalEventFilter.h"
#include <QGuiApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QDebug>
#include <QDateTime>

#include "Oran7GlobalHelper.h"

Oran7GlobalEventFilter::Oran7GlobalEventFilter(QObject *parent)
    : QObject(parent)
{
    // 注册原生事件过滤器（WM_HOTKEY 等系统消息从这里进来）
    QCoreApplication::instance()->installNativeEventFilter(this);

    // 注册全局热键（RegisterHotKey）：仅注册过的组合在窗口失焦时也会触发，其它键仍走 Qt GUI
    registerGlobalHotkeys();

    // 全局热键信号延迟派发：原生消息泵里只记 pending，由定时器在事件循环安全阶段 emit，
    // 避免在原生消息处理中途触发 QML/Oran7MediaClient 等重逻辑导致崩溃
    m_hotkeyDispatchTimer.setInterval(30);
    m_hotkeyDispatchTimer.setSingleShot(true);
    connect(&m_hotkeyDispatchTimer, &QTimer::timeout, this, [this]() {
        if (!m_pendingHotkey.isEmpty()) {
            const QString name = m_pendingHotkey;
            m_pendingHotkey.clear();
            emit keyCombinationTriggered(name);
        }
    });

    if(__GlobelKeyLogINFO__)
    {
        connect(this, &Oran7GlobalEventFilter::escapeKeyPressed, this, []()
                { INFO_LOG << "ESC key is pressed."; });
        connect(this, &Oran7GlobalEventFilter::escapeKeyReleased, this, []()
                { INFO_LOG << "ESC key is released."; });
        connect(this, &Oran7GlobalEventFilter::upKeyPressed, this, []()
                { INFO_LOG << "UP key is pressed."; });
        connect(this, &Oran7GlobalEventFilter::downKeyPressed, this, []()
                { INFO_LOG << "DOWN key is pressed."; });
        connect(this, &Oran7GlobalEventFilter::leftKeyPressed, this, []()
                { INFO_LOG << "LEFT key is pressed."; });
        connect(this, &Oran7GlobalEventFilter::rightKeyPressed, this, []()
                { INFO_LOG << "RIGHT key is pressed."; });

        connect(this, &Oran7GlobalEventFilter::upKeyReleased, this, []()
                { INFO_LOG << "UP key is released."; });
        connect(this, &Oran7GlobalEventFilter::downKeyReleased, this, []()
                { INFO_LOG << "DOWN key is released."; });
        connect(this, &Oran7GlobalEventFilter::leftKeyReleased, this, []()
                { INFO_LOG << "LEFT key is released."; });
        connect(this, &Oran7GlobalEventFilter::rightKeyReleased, this, []()
                { INFO_LOG << "RIGHT key is released."; });


        connect(this, &Oran7GlobalEventFilter::keyCombinationTriggered, this, [](QString CombinationS)
                { INFO_LOG << CombinationS+" key is pressed."; });

    }
}

Oran7GlobalEventFilter::~Oran7GlobalEventFilter()
{
#ifdef Q_OS_WIN
    for (int id : m_hotkeyIdToName.keys())
        UnregisterHotKey(nullptr, id);
#endif
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

// ============ 全局热键（RegisterHotKey） ============
// 仅在此注册的组合会走全局（窗口失焦也触发）；其它按键/组合都走正常 Qt GUI 事件。
// 需要拓展新的全局组合时，在 addHotkey(...) 追加一行即可。
void Oran7GlobalEventFilter::registerGlobalHotkeys()
{
#ifdef Q_OS_WIN
    int nextId = 1;
    auto addHotkey = [&](const QString &name, quint32 mods, quint32 vk) {
        if (RegisterHotKey(nullptr, nextId, mods, vk)) {
            m_globalHotkeys.insert(name);
            m_hotkeyIdToName.insert(nextId, name);
            if (__GlobelKeyLogINFO__)
                INFO_LOG << "GlobalHotkey registered: " << name << " (id=" << nextId << ")";
        } else {
            if (__GlobelKeyLogINFO__)
                INFO_LOG << "GlobalHotkey FAILED: " << name << " err=" << int(GetLastError());
        }
        ++nextId;
    };

    // 上/下一首均为全局热键，窗口失焦时也能触发。
    // MOD_NOREPEAT 避免长按连发 WM_HOTKEY。
    addHotkey(QStringLiteral("Ctrl+Right"), MOD_CONTROL | MOD_NOREPEAT, VK_RIGHT);
    addHotkey(QStringLiteral("Ctrl+Left"), MOD_CONTROL | MOD_NOREPEAT, VK_LEFT);

    // ---- 以后想加其它全局组合，照下面格式追加即可，例如：----
    // addHotkey("Ctrl+Shift+R", MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'R');
    // addHotkey("Alt+F4", MOD_ALT | MOD_NOREPEAT, VK_F4);
#else
    Q_UNUSED(this)
#endif
}

bool Oran7GlobalEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if ((eventType == QByteArrayLiteral("windows_generic_MSG")
         || eventType == QByteArrayLiteral("windows_dispatcher_MSG")) && message) {
        MSG *msg = static_cast<MSG *>(message);
        // 全局热键按下：Windows 发 WM_HOTKEY 到本线程消息队列
        if (msg->message == WM_HOTKEY) {
            const int id = int(msg->wParam);
            const QString name = m_hotkeyIdToName.value(id);
            if (!name.isEmpty()) {
                // 原生消息泵里不做任何 Qt/QML 调用，也不依赖 result 指针：
                // 只记录 pending，由 m_hotkeyDispatchTimer 在事件循环安全阶段 emit。
                m_pendingHotkey = name;
                m_hotkeyDispatchTimer.start();
                if (result)
                    *result = 0;
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType) Q_UNUSED(message) Q_UNUSED(result)
#endif
    return false;
}

/**
 * @brief Oran7GlobalEventFilter::eventFilter
 * @param obj
 * @param event
 * @return  is false: 非拦截，让其它领域也能接收处理, is true:处理完成,事件中止
 */
bool Oran7GlobalEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        // 忽略长按产生的自动重复事件
        if (keyEvent->isAutoRepeat()) {
            if(__GlobelKeyLogINFO__Detail__)
                INFO_LOG << "eventFilter: Ignoring auto-repeat event for key=" << keyEvent->key()
                        << ", type=" << (event->type() == QEvent::KeyPress ? "Press" : "Release");
            return QObject::eventFilter(obj, event); // 返回 false 让其他处理器也能处理
        }

        if (event->type() == QEvent::KeyPress) {
            handleKeyPress(keyEvent->key());
        } else if (event->type() == QEvent::KeyRelease) {
            handleKeyRelease(keyEvent->key());
        }
    }

    return QObject::eventFilter(obj, event);
}

void Oran7GlobalEventFilter::handleKeyPress(int key)
{
    // 检查是否已经按下（防止重复处理）
    if (m_pressedKeys.value(key, false)) {
        if(__GlobelKeyLogINFO__Detail__)
            INFO_LOG << "handleKeyPress: Key " << key << " already pressed, ignoring";
        return;
    }

    // 记录按键为按下状态
    m_pressedKeys[key] = true;

    if(__GlobelKeyLogINFO__Detail__)
        INFO_LOG << "handleKeyPress: Key " << key << " pressed (non-repeat)";

    // 发送全局键盘事件信号
    emit globalKeyEvent(key, true);

    // 检查组合键
    checkKeyCombinations();

    // 处理单键事件
    switch (key)
    {
    case Qt::Key_Escape:
        emit escapeKeyPressed();
        break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        emit enterKeyPressed();
        break;
    case Qt::Key_Up:
        emit upKeyPressed();
        break;
    case Qt::Key_Down:
        emit downKeyPressed();
        break;
    case Qt::Key_Left:
        emit leftKeyPressed();
        break;
    case Qt::Key_Right:
        emit rightKeyPressed();
        break;
    default:
        break;
    }
}

void Oran7GlobalEventFilter::handleKeyRelease(int key)
{
    if(__GlobelKeyLogINFO__Detail__)
        INFO_LOG << "handleKeyRelease: key=" << key << ", m_pressedKeys[" << key << "]=" << m_pressedKeys.value(key, false);

    // 如果按键已经不在按下状态，忽略这个释放事件
    if (!m_pressedKeys.value(key, false)) {
        if(__GlobelKeyLogINFO__Detail__)
            INFO_LOG << "handleKeyRelease: Key " << key << " already released, ignoring";
        return;
    }

    if(__GlobelKeyLogINFO__Detail__)
        INFO_LOG << "handleKeyRelease: Key " << key << " released (non-repeat)";

    // 发送全局键盘事件信号
    emit globalKeyEvent(key, false);

    // 处理方向键释放事件
    switch (key)
    {
    case Qt::Key_Escape:
        emit escapeKeyReleased();
        break;
    case Qt::Key_Up:
        emit upKeyReleased();
        break;
    case Qt::Key_Down:
        emit downKeyReleased();
        break;
    case Qt::Key_Left:
        emit leftKeyReleased();
        break;
    case Qt::Key_Right:
        emit rightKeyReleased();
        break;
    default:
        break;
    }

    // 清除按键状态
    m_pressedKeys[key] = false;

    if(__GlobelKeyLogINFO__Detail__)
        INFO_LOG << "handleKeyRelease: Cleared pressed state for key=" << key;
}

void Oran7GlobalEventFilter::emitCombo(const QString &comboName)
{
    // 已注册为全局热键的组合由 WM_HOTKEY 触发，Qt 路径这里跳过，避免重复
    if (m_globalHotkeys.contains(comboName))
        return;
    emit keyCombinationTriggered(comboName);
}

void Oran7GlobalEventFilter::checkKeyCombinations()
{
    // 检查 Shift + Delete
    if (m_pressedKeys.value(Qt::Key_Shift, false) &&
        m_pressedKeys.value(Qt::Key_Delete, false))
    {
        emitCombo(QStringLiteral("LShift+Delete"));
    }

    // 检查 Ctrl + S
    if (m_pressedKeys.value(Qt::Key_Control, false) &&
        m_pressedKeys.value(Qt::Key_S, false))
    {
        emitCombo(QStringLiteral("Ctrl+S"));
    }

    // Ctrl + Right（已注册为全局热键时在 emitCombo 里被跳过，走 WM_HOTKEY）
    if(m_pressedKeys.value(Qt::Key_Right,false)&&
       m_pressedKeys.value(Qt::Key_Control,false))
    {
        emitCombo(QStringLiteral("Ctrl+Right"));
    }

    // 注册失败时保留窗口内的上一首快捷键；注册成功则由 emitCombo 去重。
    if (m_pressedKeys.value(Qt::Key_Left, false) &&
        m_pressedKeys.value(Qt::Key_Control, false))
    {
        emitCombo(QStringLiteral("Ctrl+Left"));
    }

    // 检查 Ctrl + Alt + Delete
    if (m_pressedKeys.value(Qt::Key_Control, false) &&
        m_pressedKeys.value(Qt::Key_Alt, false) &&
        m_pressedKeys.value(Qt::Key_Delete, false))
    {
        emitCombo(QStringLiteral("Ctrl+Alt+Delete"));
    }

    // 检查 Alt + F4
    if (m_pressedKeys.value(Qt::Key_Alt, false) &&
        m_pressedKeys.value(Qt::Key_F4, false))
    {
        emitCombo(QStringLiteral("Alt+F4"));
    }

    // 检查 Ctrl + Alt + Del (Windows 安全组合)
    if (m_pressedKeys.value(Qt::Key_Control, false) &&
        m_pressedKeys.value(Qt::Key_Alt, false) &&
        m_pressedKeys.value(Qt::Key_Delete, false))
    {
        emitCombo(QStringLiteral("Ctrl+Alt+Del"));
    }
}
