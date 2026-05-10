#include "Oran7Api.h"
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Oran7Api::~Oran7Api()
{
}

Oran7Api *Oran7Api::instance()
{
    static Oran7Api *ins = new Oran7Api;
    return ins;
}

Oran7Api *Oran7Api::create(QQmlEngine *, QJSEngine *)
{
    return instance();
}

void Oran7Api::setWindowStaysOnTopHint(QWindow *window, bool hint)
{
    if (window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (hint) {
            ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        } else {
            ::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
#else
        window->setFlag(Qt::WindowStaysOnTopHint, hint);
#endif
    }
}

void Oran7Api::setWindowState(QWindow *window, int state)
{
    if (window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        switch (state) {
        case Qt::WindowMinimized:
            ::ShowWindow(hwnd, SW_MINIMIZE);
            break;
        case Qt::WindowMaximized:
            ::ShowWindow(hwnd, SW_MAXIMIZE);
            break;
        case Qt::WindowFullScreen:
            // 全屏需要先移除边框
            ::SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            ::SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
            break;
        default:
            window->setWindowState(Qt::WindowState(state));
            break;
        }
#else
        window->setWindowState(Qt::WindowState(state));
#endif
    }
}

void Oran7Api::setAlwaysOnTop(QWindow *window, bool alwaysOnTop)
{
    setWindowStaysOnTopHint(window, alwaysOnTop);
}

int Oran7Api::getWindowState(QWindow *window) const
{
    if (!window) {
        return Qt::WindowNoState;
    }

    return static_cast<int>(window->windowState());
}

Oran7Api::Oran7Api(QObject *parent)
    : QObject(parent)
{
}
