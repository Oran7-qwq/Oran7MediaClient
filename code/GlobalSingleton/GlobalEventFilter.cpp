#include "GlobalEventFilter.h"
#include <QGuiApplication>
#include <QKeyEvent>
#include <QDebug>
#include <QDateTime>

#include "globalhelper.h"

GlobalEventFilter::GlobalEventFilter(QObject *parent)
    : QObject(parent), m_isEscapeHandled(false), m_lastEscapeTime(0)
{
    connect(this, &GlobalEventFilter::escapeKeyPressed, this, []()
            { INFO_LOG << "ESC key is pressed."; });
}

GlobalEventFilter::~GlobalEventFilter()
{
}

bool GlobalEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    // 只处理键盘事件
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        // 发送全局键盘事件信号
        emit globalKeyEvent(keyEvent->key(), true);

        // 特别处理 ESC 键
        if (keyEvent->key() == Qt::Key_Escape)
        {
            // 去重逻辑：避免同一个按键事件多次触发
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            if (currentTime - m_lastEscapeTime < 100)
            { // 100ms内的重复事件忽略
                return false;
            }
            m_lastEscapeTime = currentTime;

            emit escapeKeyPressed();
            return false; // 不拦截，让其他控件也能处理
        }
    }
    // else if (event->type() == QEvent::KeyRelease) {
    //     QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    //     // 发送全局键盘事件信号
    //     emit globalKeyEvent(keyEvent->key(), false);
    // }

    return QObject::eventFilter(obj, event);
}
