#include "GlobalEventFilter.h"
#include <QGuiApplication>
#include <QKeyEvent>
#include <QDebug>
#include <QDateTime>

#include "GlobalHelper.h"

GlobalEventFilter::GlobalEventFilter(QObject *parent)
    : QObject(parent)
{
    if(__GlobelKeyLogINFO__)
    {
        connect(this, &GlobalEventFilter::escapeKeyPressed, this, []()
                { INFO_LOG << "ESC key is pressed."; });
        connect(this, &GlobalEventFilter::escapeKeyReleased, this, []()
                { INFO_LOG << "ESC key is released."; });
        connect(this, &GlobalEventFilter::upKeyPressed, this, []()
                { INFO_LOG << "UP key is pressed."; });
        connect(this, &GlobalEventFilter::downKeyPressed, this, []()
                { INFO_LOG << "DOWN key is pressed."; });
        connect(this, &GlobalEventFilter::leftKeyPressed, this, []()
                { INFO_LOG << "LEFT key is pressed."; });
        connect(this, &GlobalEventFilter::rightKeyPressed, this, []()
                { INFO_LOG << "RIGHT key is pressed."; });

        connect(this, &GlobalEventFilter::upKeyReleased, this, []()
                { INFO_LOG << "UP key is released."; });
        connect(this, &GlobalEventFilter::downKeyReleased, this, []()
                { INFO_LOG << "DOWN key is released."; });
        connect(this, &GlobalEventFilter::leftKeyReleased, this, []()
                { INFO_LOG << "LEFT key is released."; });
        connect(this, &GlobalEventFilter::rightKeyReleased, this, []()
                { INFO_LOG << "RIGHT key is released."; });
    }
}

GlobalEventFilter::~GlobalEventFilter()
{

}

/**
 * @brief GlobalEventFilter::eventFilter
 * @param obj
 * @param event
 * @return  is false: 非拦截，让其它领域也能接收处理, is true:处理完成,事件中止
 */
bool GlobalEventFilter::eventFilter(QObject *obj, QEvent *event)
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
            handleKeyPress(keyEvent);
        } else if (event->type() == QEvent::KeyRelease) {
            handleKeyRelease(keyEvent);
        }
    }

    return QObject::eventFilter(obj, event);
}

void GlobalEventFilter::handleKeyPress(QKeyEvent *keyEvent)
{
    int key = keyEvent->key();

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

void GlobalEventFilter::handleKeyRelease(QKeyEvent *keyEvent)
{
    int key = keyEvent->key();

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

void GlobalEventFilter::checkKeyCombinations()
{
    // 检查 Shift + Delete
    if (m_pressedKeys.value(Qt::Key_Shift, false) &&
        m_pressedKeys.value(Qt::Key_Delete, false))
    {
        //INFO_LOG << "Shift + Delete 组合键被按下";
        emit keyCombinationTriggered("LShift+Delete");
    }

    // 检查 Ctrl + S
    if (m_pressedKeys.value(Qt::Key_Control, false) &&
        m_pressedKeys.value(Qt::Key_S, false))
    {
        // INFO_LOG << "Ctrl + S 组合键被按下";
        emit keyCombinationTriggered("Ctrl+S");
    }

    // 检查 Ctrl + Alt + Delete
    if (m_pressedKeys.value(Qt::Key_Control, false) &&
        m_pressedKeys.value(Qt::Key_Alt, false) &&
        m_pressedKeys.value(Qt::Key_Delete, false))
    {
        // INFO_LOG << "Ctrl + Alt + Delete 组合键被按下";
        emit keyCombinationTriggered("Ctrl+Alt+Delete");
    }

    // 检查 Alt + F4
    if (m_pressedKeys.value(Qt::Key_Alt, false) &&
        m_pressedKeys.value(Qt::Key_F4, false))
    {
        // INFO_LOG << "Alt + F4 组合键被按下";
        emit keyCombinationTriggered("Alt+F4");
    }

    // 检查 Ctrl + Alt + Del (Windows 安全组合)
    if (m_pressedKeys.value(Qt::Key_Control, false) &&
        m_pressedKeys.value(Qt::Key_Alt, false) &&
        m_pressedKeys.value(Qt::Key_Delete, false))
    {
        // INFO_LOG << "Ctrl + Alt + Del 组合键被按下";
        emit keyCombinationTriggered("Ctrl+Alt+Del");
    }
}
