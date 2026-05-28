#include "ApplicationContext.h"
#include "GlobalHelper.h"
#include "Oran7ScreenCapture.h"

ApplicationContext::ApplicationContext(QObject *parent) : QObject(parent)
{
    this->AppContext_CreateClientObject();
    this->AppContext_CreateAsyncWorkerObject();
}

ApplicationContext::~ApplicationContext()
{
    INFO_LOG << "[DTOR] ApplicationContext::~ApplicationContext() called";
    INFO_LOG << "[DTOR]   m_client:" << m_client;
    INFO_LOG << "[DTOR]   m_asyncWorker:" << m_asyncWorker;

    if (m_client) {
        INFO_LOG << "[DTOR]   Deleting m_client...";
        delete m_client;
        m_client = nullptr;
    }
    if (m_asyncWorker) {
        INFO_LOG << "[DTOR]   Deleting m_asyncWorker...";
        delete m_asyncWorker;
        m_asyncWorker = nullptr;
    }
    INFO_LOG << "[DTOR] ApplicationContext::~ApplicationContext() finished";
}

Client *ApplicationContext::client() const
{
    if(m_client==nullptr)return nullptr;
    return m_client;
}

AsyncWorker *ApplicationContext::asyncWorker() const
{
    if(m_asyncWorker==nullptr)return nullptr;
    return m_asyncWorker;
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

void ApplicationContext::AppContext_CreateAsyncWorkerObject()
{
    if(m_asyncWorker!=nullptr)return;
    try
    {
        m_asyncWorker = new AsyncWorker();
        QQmlEngine::setObjectOwnership(m_asyncWorker, QQmlEngine::CppOwnership);
        if(m_asyncWorker == nullptr)
            throw std::runtime_error("Failed to create AsyncWorker Object.");
    }
    catch (const std::exception &e)
    {
        INFO_LOG<<e.what();
    }
}
