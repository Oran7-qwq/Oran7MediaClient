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
        // if (p->objectName() == QLatin1StringView("__Oran7SettingsContainerWindow__")) {
        //     m_window = qobject_cast<QQuickWindow *>(p);
        //     setup(m_window);
        // }
    }
}

void Oran7WindowAgent::componentComplete()
{

}
