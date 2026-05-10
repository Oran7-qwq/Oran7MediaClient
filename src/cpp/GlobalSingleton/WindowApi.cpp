#include "WindowApi.h"
#include <QDebug>
#include <QGuiApplication>
#include <QClipboard>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

WindowApi::~WindowApi()
{
}

WindowApi *WindowApi::instance()
{
    static WindowApi *ins = new WindowApi;
    return ins;
}

WindowApi *WindowApi::create(QQmlEngine *, QJSEngine *)
{
    return instance();
}

void WindowApi::setWindowMinimized(QWindow *window)
{
    if (window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        ShowWindow(hwnd, SW_MINIMIZE);
#else
        window->setWindowState(Qt::WindowMinimized);
#endif
    }
}

void WindowApi::setWindowMaximized(QWindow *window)
{
    if (window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        ShowWindow(hwnd, SW_MAXIMIZE);
#else
        window->setWindowState(Qt::WindowMaximized);
#endif
    }
}

void WindowApi::setWindowNormal(QWindow *window)
{
    if (window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        ShowWindow(hwnd, SW_RESTORE);
#else
        window->setWindowState(Qt::WindowNoState);
#endif
    }
}

void WindowApi::setWindowState(QWindow *window, int state)
{
    if (window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        switch (state) {
        case Qt::WindowMinimized:
            ShowWindow(hwnd, SW_MINIMIZE);
            break;
        case Qt::WindowMaximized:
            ShowWindow(hwnd, SW_MAXIMIZE);
            break;
        case Qt::WindowNoState:
            ShowWindow(hwnd, SW_RESTORE);
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

void WindowApi::setWindowStaysOnTopHint(QWindow *window, bool hint)
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

WindowApi::WindowApi(QObject *parent)
    : QObject{parent}
{
}