#include "LoadingDataHandler.h"

QByteArray LoadingDataHandler::handlePacket(const QByteArray &packet)
{
    //解析协议[长度(4字节)][操作码(1字节)][UID(4字节)]

    //确保数据至少包含数据包长度
    if(packet.size()<4)
    {
        qDebug()<<"[Sever:]Invalid login packet :too short";
        return QByteArray(1,static_cast<char>(0x00));
    }
    //解析大端序列的数据包长度,到小端序字节的quint32
    const char* data=packet.constData();
    quint32 totalLength=
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
    quint8 opcode=static_cast<quint8>(packet.at(4));
    if(opcode!=LoadingData)
    {
        qDebug()<<"Invild opcode for loginHandler:"<<opcode;
        return QByteArray(1,static_cast<char>(0x00));
    }
    qDebug()<<"[Sever:]opcode is Actual.";
    //提取UID
    data=&packet.constData()[5];
    quint32 UID=
        (static_cast<quint32>(data[0]) << 24) |
        (static_cast<quint32>(data[1]) << 16) |
        (static_cast<quint32>(data[2]) << 8) |
        static_cast<quint32>(data[3]);

    //<<-----<<加载用户喜欢的音乐列表>>----->>
    QList<QList<QString>> list;
    //调用服务器数据库方法
    list=db->getUserLikeMusicListData(UID);
    int listSize=list.size();
    for(int i=0;i<2;i++)
    {
        qDebug()<<list[i][0];
        qDebug()<<list[i][1];
        qDebug()<<list[i][2];
        qDebug()<<list[i][3];
        qDebug()<<list[i][4];
    }

    //<传输协议>
    //[协议头:4B total_length + 1B opcode + 4B song_count]
    //[歌曲1: 4B length + (2B name_len + name + 2B artist_len + artist + 2B album_len + album + 4B timesize + music_id[4B])]
    //[歌曲2: 4B length + ...]
    QByteArray response;
    QDataStream out(&response,QDataStream::WriteOnly);
//QDataStream的版本兼容选择,Qt6一般不需要显式设置版本号QDataStream::Qt_DefaultCompiledVersion
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setVersion(QDataStream::Qt_6_0);  // Qt 6 使用 Qt_6_0
#else
    out.setVersion(QDataStream::Qt_5_15);  // Qt 5 使用 Qt_5_15
#endif
    quint32 responseLength=4+1+4;
    for(int i=0;i<listSize;i++)
    {
        responseLength+=4+2*3+list[i][0].toUtf8().size()+list[i][1].toUtf8().size()+list[i][2].toUtf8().size()+4+4+4;
    }
    out<<responseLength;
    out<<quint8(LoadingData);
    out<<quint32(listSize);
    for (int i = 0; i <listSize; ++i)
    {
        out<<quint32(4+2*3+4+list[i][0].toUtf8().size()+list[i][1].toUtf8().size()+list[i][2].toUtf8().size()+4+4);
        for(int j=0;j<3;j++)
        {
            out<<quint16(list[i][j].toUtf8().size());
            out.writeRawData(list[i][j].toUtf8().constData(), list[i][j].toUtf8().size());
        }
        out<<quint32(4);
        out<<quint32(list[i][3].toInt());
        out<<quint32(list[i][4].toInt());
    }
    qDebug() << "[Sever:]response size:" << response.size();
    qDebug() << "[Sever:]response:" <<response.toHex(' ');
    return response;
     // return 0x00;
}
