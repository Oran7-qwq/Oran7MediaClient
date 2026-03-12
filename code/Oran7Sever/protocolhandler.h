#ifndef PROTOCOLHANDLER_H
#define PROTOCOLHANDLER_H

#include<QTcpSocket>
#include<QByteArray>
#include<functional>

using AsyncPacketHandler = std::function<void(QTcpSocket*,const QByteArray&,std::function<void(const QByteArray&)>)>;

enum ProtocolHandlerCMD{
    Login=0x01,
    LoadingData=0x02
};

class ProtocolHandler
{
public:
    virtual ~ProtocolHandler()=default;

    //处理接收到的数据包
    virtual QByteArray handlePacket(const QByteArray& packet) = 0;
};

#endif // PROTOCOLHANDLER_H
