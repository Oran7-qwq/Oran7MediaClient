#include "Oran7ApplicationContext.h"
#include "Oran7GlobalHelper.h"

Oran7ApplicationContext::Oran7ApplicationContext(QObject *parent) : QObject(parent)
{
    this->AppContext_CreateClientObject();
}

Oran7ApplicationContext::~Oran7ApplicationContext()
{
    INFO_LOG << "[DTOR] Oran7ApplicationContext::~Oran7ApplicationContext() called";
    INFO_LOG << "[DTOR]   m_client:" << m_client;

    if (m_client) {
        INFO_LOG << "[DTOR]   Deleting m_client...";
        delete m_client;
        m_client = nullptr;
    }
    INFO_LOG << "[DTOR] Oran7ApplicationContext::~Oran7ApplicationContext() finished";
}

Oran7MediaClient *Oran7ApplicationContext::client() const
{
    if(m_client==nullptr)return nullptr;
    return m_client;
}

void Oran7ApplicationContext::AppContext_CreateClientObject()
{
    if(m_client!=nullptr)return;
    try
    {
        m_client = new Oran7MediaClient("127.0.0.1",55711);
        QQmlEngine::setObjectOwnership(m_client, QQmlEngine::CppOwnership);
        if(m_client ==nullptr)
            throw std::runtime_error("Failed to created Oran7MediaClient Object.");

    }
    catch (const std::exception &e)
    {
        INFO_LOG<<e.what();
    }
}
