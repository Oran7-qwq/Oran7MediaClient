#ifndef LOADINGDATAHANDLER_H
#define LOADINGDATAHANDLER_H

#include "ProtocolHandler.h"
#include"DataBase.h"

class LoadingDataHandler : public ProtocolHandler
{
public:
    LoadingDataHandler(Database *db_)
        :ProtocolHandler()
        ,db(db_){}
    QByteArray handlePacket(const QByteArray&)override;
private:
    Database *db;
};

#endif // LOADINGDATAHANDLER_H
