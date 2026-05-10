#include "oran7windowagent.h"

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
            setup(qobject_cast<QQuickWindow *>(p));
        }
    }
}

void Oran7WindowAgent::componentComplete()
{

}
