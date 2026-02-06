#ifndef LOGINHANDLER_H
#define LOGINHANDLER_H

#include "protocolhandler.h"
#include"database.h"

#include<QTcpSocket>

class LoginHandler : public ProtocolHandler
{
public:
    LoginHandler(Database *db_)
        : ProtocolHandler()
        ,db(db_){}

    QByteArray handlePacket(const QByteArray& packet)override;

private:
    Database *db;
};

#endif // LOGINHANDLER_H
