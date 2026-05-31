#ifndef APPLICATIONCONTEXT_H
#define APPLICATIONCONTEXT_H

#include "AsyncManagers.h"
#include "Oran7MediaClient.h"

#include "AppJsonConfigManager.h"
#include "Oran7ThemeProfileManager.h"

#include "WindowApi.h"

#include <QObject>
#include <QQmlApplicationEngine>

class ApplicationContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AsyncWorker* asyncWorker READ asyncWorker CONSTANT)
    Q_PROPERTY(Client* client READ client CONSTANT)
public:

    static ApplicationContext& instance()
    {
        static ApplicationContext m_instance;

        return m_instance;
    }

    AsyncWorker* asyncWorker() const;
    Client* client() const;

    //auto used by construct function
    void AppContext_CreateClientObject();
    void AppContext_CreateAsyncWorkerObject();

private:
    explicit ApplicationContext(QObject* parent = nullptr);
    ~ApplicationContext();

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;

private:
    Client* m_client;
    AsyncWorker* m_asyncWorker;
};


#endif // APPLICATIONCONTEXT_H
