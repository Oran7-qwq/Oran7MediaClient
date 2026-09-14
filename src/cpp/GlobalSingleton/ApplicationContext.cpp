#include "ApplicationContext.h"
#include "GlobalHelper.h"
#include "Oran7ScreenCapture.h"

ApplicationContext::ApplicationContext(QObject *parent) : QObject(parent)
{
    this->AppContext_CreateClientObject();
}

ApplicationContext::~ApplicationContext()
{
    INFO_LOG << "[DTOR] ApplicationContext::~ApplicationContext() called";
    INFO_LOG << "[DTOR]   m_client:" << m_client;

    if (m_client) {
        INFO_LOG << "[DTOR]   Deleting m_client...";
        delete m_client;
        m_client = nullptr;
    }
    INFO_LOG << "[DTOR] ApplicationContext::~ApplicationContext() finished";
}

Client *ApplicationContext::client() const
{
    if(m_client==nullptr)return nullptr;
    return m_client;
}

void ApplicationContext::AppContext_CreateClientObject()
{
    if(m_client!=nullptr)return;
    try
    {
        m_client = new Client("127.0.0.1",55711);
        QQmlEngine::setObjectOwnership(m_client, QQmlEngine::CppOwnership);
        if(m_client ==nullptr)
            throw std::runtime_error("Failed to created Client Object.");

        //Client source of ScreenCpature
        if(!m_client->m_screenCap)
        {
            m_client->m_screenCap = new Oran7ScreenCaptureController(nullptr);
        }
    }
    catch (const std::exception &e)
    {
        INFO_LOG<<e.what();
    }
}
