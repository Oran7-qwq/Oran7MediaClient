#include "Oran7WindowAgent.h"

#ifdef WIN32
#include <windows.h>
#endif

Oran7WindowAgent::Oran7WindowAgent(QObject *parent)
    : QWK::QuickWindowAgent{parent}
{

}

Oran7WindowAgent::~Oran7WindowAgent()
{

}

void Oran7WindowAgent::classBegin()
{
    auto p = parent();
    Q_ASSERT_X(p, "Oran7WindowAgent", "parent() return nullptr!");
    if (p) {
        if (p->objectName() == QLatin1StringView("__Oran7Window__")) {
            m_window = qobject_cast<QQuickWindow *>(p);
            setup(m_window);
        }
        if (p->objectName() == QLatin1StringView("__BORDER__")) {
            m_window = qobject_cast<QQuickWindow *>(p);
            setup(m_window);
        }
    }
}

void Oran7WindowAgent::componentComplete()
{
    if (m_mousePassThrough) {
        setMousePassThrough(true);
    }
}

void Oran7WindowAgent::setMousePassThrough(bool enable)
{
    if (m_mousePassThrough == enable)
        return;

    m_mousePassThrough = enable;
    emit mousePassThroughChanged();

    if (m_window) {
#ifdef WIN32
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        if (hwnd) {
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            if (enable) {
                style |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
            } else {
                style &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);
            }
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, style);
        }
#else
        m_window->setAttribute(Qt::WA_TransparentForMouseEvents, enable);
#endif
    }
}
