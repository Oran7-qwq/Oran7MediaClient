#ifndef ORAN7APPLICATIONCONTEXT_H
#define ORAN7APPLICATIONCONTEXT_H

#include "Oran7MediaClient.h"

#include <QObject>
#include <QQmlApplicationEngine>

class Oran7ApplicationContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Oran7MediaClient* client READ client CONSTANT)
public:

    static Oran7ApplicationContext& instance()
    {
        static Oran7ApplicationContext m_instance;

        return m_instance;
    }

    Oran7MediaClient* client() const;

    //auto used by construct function
    void AppContext_CreateClientObject();

private:
    explicit Oran7ApplicationContext(QObject* parent = nullptr);
    ~Oran7ApplicationContext();

    Oran7ApplicationContext(const Oran7ApplicationContext&) = delete;
    Oran7ApplicationContext& operator=(const Oran7ApplicationContext&) = delete;

private:
    Oran7MediaClient* m_client;
};


#endif // ORAN7APPLICATIONCONTEXT_H
