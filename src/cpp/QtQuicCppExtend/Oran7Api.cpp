#include "Oran7Api.h"
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QDebug>
#include <QQmlEngine>
#include <QRegion>
#include <QThread>
#include <cmath>

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
    auto* api = instance();
    // 跨引擎复用的应用级实例不交给某一个 QML 引擎回收。
    QQmlEngine::setObjectOwnership(api, QQmlEngine::CppOwnership);
    return api;
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

void Oran7Api::setWindowMaskRect(QWindow* window, const QRectF& rect)
{
    if (!window || window->thread() != QThread::currentThread()
        || !std::isfinite(rect.x()) || !std::isfinite(rect.y())
        || !std::isfinite(rect.width()) || !std::isfinite(rect.height())) return;
    const auto bounds = rect.toAlignedRect().intersected(QRect(QPoint(), window->size()));
    // 空 QRegion 表示取消遮罩；无有效范围时保留一个位于窗口外的空命中区域。
    const QRegion mask(bounds.isEmpty() ? QRect(-2, -2, 1, 1) : bounds);
    if (window->mask() != mask) window->setMask(mask);
}
