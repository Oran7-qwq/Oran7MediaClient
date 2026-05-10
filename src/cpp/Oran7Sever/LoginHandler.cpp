#include "LoginHandler.h"

#include <QDebug>

QByteArray LoginHandler::handlePacket(const QByteArray& packet)
{
    //解析协议[总长度(4字节)][操作码(1字节)][用户名长度(1字节)][密码长度(1字节)][用户名][密码]

    //确保数据包至少包含数据包长度
    //数据包长度4+用户名字长度1+密码长度1
    if(packet.size()<4)
    {
        qDebug()<<"[Sever:]Invalid login packet :too short";
        return QByteArray(1,static_cast<char>(0x00));
    }

    //解析数据包长度0 1 2 3位
    // quint32 totalLength=*reinterpret_cast<const quint32*>(packet.constData());//这个默认会解析小端序列的数据
    //正确的大端序的解析方法
    const char* data = packet.constData();
    quint32 totalLength =
        (static_cast<quint32>(data[0]) << 24) |
        (static_cast<quint32>(data[1]) << 16) |
        (static_cast<quint32>(data[2]) << 8) |
        static_cast<quint32>(data[3]);
    if(totalLength!=packet.size())
    {
        qDebug()<<"[Sever:]Packet length mismatch.Expected:"<<totalLength<<",Actual:"<<packet.size();
        return QByteArray(1,static_cast<char>(0x00));
    }
    qDebug()<<"[Sever:]packetSize is Actual.";

    //解析操作码at(4)
    quint8 opcode = *(reinterpret_cast<const quint8*>(&packet.constData()[4]));

    // quint8 opcode=m_buffers[socket].at(4);
    if(opcode!=Login)
    {
        qDebug()<<"Invild opcode for loginHandler:"<<opcode;
        return QByteArray(1,static_cast<char>(0x00));
    }
    qDebug()<<"[Sever:]opcode is Actual.";

    //确保数据包含用户名的密码的长度位
    //数据包长度4+用户名字长度1+密码长度1
    if(packet.size()<7)
    {
        qDebug()<<"Invalid login packet: insufficient data for username and password lengths";
        return QByteArray(1,static_cast<char>(0x00));
    }

    //解析用户名长度at(5)
    quint8 usernameLength=*reinterpret_cast<const quint8*>(&packet.constData()[5]);
    //解析密码长度
    quint8 passwordLength=*reinterpret_cast<const quint8*>(&packet.constData()[6]);

    //计算用户名和密码处于协议流的起始位置
    int usernameStart = 7;
    int usernameEnd = usernameStart+usernameLength;
    int passwordStart=usernameEnd;
    int passwordEnd=passwordStart+passwordLength;

    if(packet.size()<passwordEnd)
    {
        qDebug()<<"Invalid login packet: insufficient data for username and password.";
        return QByteArray(1,static_cast<char>(0x00));
    }
    qDebug()<<"[Sever:]passwordSizeEnd is Actual.";

    //提取用户名和密码
    QByteArray usernameData=packet.mid(usernameStart,usernameLength);
    QByteArray passwordData=packet.mid(passwordStart,passwordLength);

    //将字节组转换为QString
    QString username=QString::fromUtf8(usernameData);
    QString password=QString::fromUtf8(passwordData);

    //验证用户名和密码(状态码)
    //1：账号不存在0x01
    //2：密码不正确0x02
    //0：成功登录0x00
    qDebug()<<"[Sever:]Loading Database......";
    int isValid=db->validateLogin(username,password);
    qDebug()<<"[Sever:]isValid:"<<isValid;
    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
//QDataStream的版本兼容选择,Qt6一般不需要显式设置版本号QDataStream::Qt_DefaultCompiledVersion
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setVersion(QDataStream::Qt_6_0);  // Qt 6 使用 Qt_6_0
#else
    out.setVersion(QDataStream::Qt_5_15);  // Qt 5 使用 Qt_5_15
#endif
//登录查询状态回应插入传输协议流分支
    switch (isValid) {
    case 0:
    {
        quint32 responseLength = 11+usernameLength;//总长度4+回应指令1+状态码1+用户UID4+[usernameLength]1+用户名length
        out<<responseLength;
        out<<quint8(Login);
        out<<quint8(0x00);
        //获取用户UID
        quint32 UID=db->getUID(username);
        out<<UID;
        out<<static_cast<char>(usernameLength);
        out.writeRawData(usernameData.constData(), usernameData.size());
        break;
    }
    case 1:
    {
        quint32 responseLength=6;//总长度4+回应指令1+状态码1
        out<<responseLength;
        out<<quint8(Login);
        out<<quint8(0x01);
        break;
    }
    case 2:
    {
        quint32 responseLength=6;//总长度4+回应指令1+状态码1
        out<<responseLength;
        out<<quint8(Login);
        out<<quint8(0x02);
        break;
    }
    default:
        break;
    }
    //发送给客户端
    qDebug()<<"[Sever]:validateResult is sended to Client.";
    qDebug() << "[Sever:]response size:" << response.size();
    qDebug() << "[Sever:]response:" <<response.toHex(' ');
    // socket->write(response);
    // socket->flush();

    //END
    return response;
    // return 0x00;
}



