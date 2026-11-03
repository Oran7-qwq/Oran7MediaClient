#include "Oran7LegacyServerClient.h"
#include "Oran7GlobalHelper.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDataStream>
#include <atomic>

Oran7LegacyServerClient::Oran7LegacyServerClient(const QString& host, quint16 port, const QString& databasePath, bool enabled)
{
    // 默认只保留接口，不建 Socket、不打开数据库、不连接旧服务器。
    if (!enabled) return;
    Client_socket = new QTcpSocket(this);
    static std::atomic<quint64> serial{0};
    m_connectionName = QStringLiteral("LocalSqlite_connection_%1").arg(++serial);
    localdb = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    localdb.setDatabaseName(databasePath);
    if (!localdb.open()) qWarning() << "无法打开遗留客户端缓存：" << localdb.lastError().text();
    connect(Client_socket, &QTcpSocket::connected, this, &Oran7LegacyServerClient::onConnected);
    connect(Client_socket, &QTcpSocket::disconnected, this, &Oran7LegacyServerClient::onDisconnected);
    registerHandler_(Login_, [this](quint32 length, quint8 opcode, const QByteArray& packet) { loginResponse(length, opcode, packet); });
    registerHandler_(LoadingData_, [this](quint32 length, quint8 opcode, const QByteArray& packet) { explainSeverUserData(length, opcode, packet); });
    Client_socket->connectToHost(host, port);
}

Oran7LegacyServerClient::~Oran7LegacyServerClient()
{
    if (!enabled()) return;
    Client_socket->disconnect(this);
    Client_socket->abort();
    localdb.close();
    localdb = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

void Oran7LegacyServerClient::registerHandler_(quint8 opcode,PacketHandler handler)
{
    Client_handler[opcode]=handler;
}

void Oran7LegacyServerClient::onConnected()
{
    CLIENT_LOG << "connected Sever host:" << Client_socket->peerAddress()<<", port:"<<Client_socket->peerPort();
    connect(Client_socket,&QTcpSocket::readyRead,this,&Oran7LegacyServerClient::onReadReady);
}

void Oran7LegacyServerClient::onDisconnected()
{
    CLIENT_LOG<<"Disconnected from server";
}

void Oran7LegacyServerClient::onReadReady()
{
    QTcpSocket *socket=qobject_cast<QTcpSocket*>(sender());
    //验证是同一客户端提示
    Q_ASSERT(socket==Client_socket);
    CLIENT_LOG<<socket->peerAddress()<<"be responsed.";

    //向缓冲区读入数据
    responseFromServer.append(socket->readAll());

    CLIENT_LOG << "[LegacyClient] 接收字节数=" << responseFromServer.size();
    while(responseFromServer.size()>=5)
    {
        QDataStream stream(responseFromServer);
        stream.setByteOrder(QDataStream::BigEndian); // 根据协议设置字节序
        quint32 totalLength;
        stream >> totalLength;

        if (totalLength < 5) { responseFromServer.clear(); return; }
        if(responseFromServer.size()<totalLength)
        {
            //数据传输是分块发送的，如果缓冲区数据不完整，继续读取
            CLIENT_LOG<<"respons not enought.Expected:"<<totalLength<<" ,Actual:"<<responseFromServer.size();
            CLIENT_LOG << "[LegacyClient] 缓冲字节数=" << responseFromServer.size();
            break;
        }
        //数据完整，开始解析
        CLIENT_LOG<<"Data is already ";

        // quint8 opcode=*reinterpret_cast<const quint8*>(&responseFromServer.constData()[4]);//用指针转换方法
        quint8 opcode=static_cast<quint8>(responseFromServer.at(4));
        if(opcode!=Login_&&opcode!=LoadingData_)
        {
            CLIENT_LOG<<"Opcode of Login is Error."<<"Comming Opcode:"<<opcode;
            return;
        }

        //提取预期的完整数据包
        QByteArray packet=responseFromServer.mid(0,totalLength);
        //清除0级缓存
        responseFromServer.remove(0,totalLength);

        auto it=Client_handler.find(opcode);
        if(it!=Client_handler.end())
        {
            //调用响应处理器
            it.value()(totalLength,opcode,packet);
        }
        else
        {
            CLIENT_LOG<<"Unknown opcode:"<<opcode;
            break;
        }
    }
}

//<<==========<<登录>>==========>>

//登录验证
void Oran7LegacyServerClient::validateLogin(QString username,QString password)
{
    if (!enabled()) return;
    if(username.size()!=11)//检查长度
    {
        CLIENT_LOG<<"Error username format.";
        emit loginInputFormatValid(1);
        return;
    }
    else
    {
        //检查格式
        for(QChar x : username)
        {
            //账号中只能含有数字，否则格式错误
            if(!(x>='0'&&x<='9'))
            {
                CLIENT_LOG<<"Error username format.";
                emit loginInputFormatValid(1);
                return;
            }
        }
    }
    CLIENT_LOG << "[LegacyClient] 校验登录参数";
    if(!(password.size()>=8&&password.size()<16))//检查长度
    {
        CLIENT_LOG<<"Error password format.";
        emit loginInputFormatValid(2);
        return;
    }
    else
    {
        //检查格式
        for(QChar x : password)
        {
            //密码只能含有数字大小写字母和下滑线，否则格式错误
            if(!((x>='0'&&x<='9')||(x>='a'&&x<='z')||(x>='A'&&x<='Z')||(x=='_')))
            {
                CLIENT_LOG<<"Error password format.";
                emit loginInputFormatValid(2);
                return;
            }
        }
    }
    emit loginInputFormatValid(0);
    CLIENT_LOG<<"Password right format.";
    //基本格式正确,开始询问服务器

    //<构造传输协议>
    QByteArray query;
    // [长度4]（转换成大端序）quint32在x86x64平台上是默认是小端序列
    quint32 totalLength = 4 + 1 + 1 + 1 + username.size() + password.size();
    query.append(static_cast<char>((totalLength >> 24) & 0xFF));
    query.append(static_cast<char>((totalLength >> 16) & 0xFF));
    query.append(static_cast<char>((totalLength >> 8) & 0xFF));
    query.append(static_cast<char>(totalLength & 0xFF));
    // [操作码1]
    query.append(static_cast<char>(Login_));
    // [账号长度1]
    query.append(static_cast<char>(username.size()));
    // [密码长度1]
    query.append(static_cast<char>(password.size()));
    // [username]
    query.append(username.toUtf8());
    // [password]
    query.append(password.toUtf8());

    CLIENT_LOG << "query size:" << query.size();

    Client_socket->write(query);
    Client_socket->flush();//强制刷新缓冲区快速获得回应
}

//处理登录响应
void Oran7LegacyServerClient::loginResponse(quint32 totalLength, quint8 opcode, const QByteArray &packet)
{
    if (!enabled()) return;
    Q_UNUSED(totalLength)
    Q_UNUSED(opcode)
    //这里从中提取的初始位参考服务器端的传输协议
    //总长度4+回应指令1+状态码1+用户UID4+[usernameLength]1+用户名length
    if (packet.size() < 11) return;
    if (packet.size() < 11 + quint8(packet.at(10))) return;
    quint8 statueCode=static_cast<quint8>(packet.at(5));
    quint8 usernameSize=static_cast<quint8>(packet.at(10));
    QString username=QString::fromUtf8(packet.mid(11,usernameSize));

    //<传输协议>
    //0x00登录成功->3
    //0x01账号不存在或未注册->4
    //0x02密码错误->5
    switch (statueCode) {
    case 0x00:
    {
        // quint32 UID_=*reinterpret_cast<const quint32*>(&packet.constData()[6]);//这个默认会解析小端序列的数据
        const auto *uid_data=reinterpret_cast<const unsigned char*>(packet.constData()+6);
        UID=(static_cast<quint32>(uid_data[0])<<24)|
              (static_cast<quint32>(uid_data[1])<<16)|
            (static_cast<quint32>(uid_data[2])<<8)|
            (static_cast<quint32>(uid_data[3]));
        emit loginInputFormatValid(3);
        //耗时操作：加载云端用户数据到本地缓存数据库中，在主线程执行该耗时操作是合理的，让用户等待加载完成
        loadingSeverUserDataQuery();
        emit loginSuccess(username.mid(0,3)+"--"+username.mid(7,4));
        break;
    }
    case 0x01:emit loginInputFormatValid(4);
        break;
    case 0x02:emit loginInputFormatValid(5);
        break;
    default:CLIENT_LOG<<"Error statueCode:"<<statueCode;
        break;
    }
}

//<<==========<<加载缓存云端数据>>==========>>
void Oran7LegacyServerClient::loadingSeverUserDataQuery()
{
    if (!enabled()) return;
    //<构造传输协议>
    //[长度]4+[操作码]1+[UID]4
    QByteArray query;
    // [长度4]（转换成大端序）quint32在x86x64平台上是默认是小端序列
    quint32 totalLength=4+1+4;
    query.append(static_cast<char>((totalLength >> 24) & 0xFF));
    query.append(static_cast<char>((totalLength >> 16) & 0xFF));
    query.append(static_cast<char>((totalLength >> 8) & 0xFF));
    query.append(static_cast<char>(totalLength & 0xFF));
    //[操作码]1
    query.append(static_cast<char>(LoadingData_));
    //[UID]1
    query.append(static_cast<char>((UID >> 24) & 0xFF));
    query.append(static_cast<char>((UID >> 16) & 0xFF));
    query.append(static_cast<char>((UID >> 8) & 0xFF));
    query.append(static_cast<char>(UID & 0xFF));
    // qDebug()<<"[Oran7MediaClient:]UID:"<<UID;
    CLIENT_LOG << "query size:" << query.size();

    Client_socket->write(query);
    Client_socket->flush();//强制刷新缓冲区快速获得回应
}

void Oran7LegacyServerClient::explainSeverUserData(quint32 totalLength, quint8 opcode, const QByteArray &packet)
{
    if (!enabled()) return;
    Q_UNUSED(totalLength)
    Q_UNUSED(opcode)

    CLIENT_LOG << "[LegacyClient] 数据包字节数=" << packet.size();
    //<传输协议>
    //[协议头:4B total_length + 1B opcode + 4B song_count]
    //[歌曲1: 4B length + (2B name_len + name + 2B artist_len + artist + 2B album_len + album +4B[skip0x00]+ 4B timesize + 4B music_id)]
    //[歌曲2: 4B length + ...]
    //<传输协议解码流>
    QDataStream in(packet);
    in.setByteOrder(QDataStream::BigEndian);//大端序解析
    if (packet.size() < 9) return;
    quint32 Song_Count = 0;
    in>>totalLength>>opcode>>Song_Count;
    CLIENT_LOG<<"Song_Count:"<<Song_Count;
    QList<QList<QString>> list_musicList;
    for(quint32 i=0;i<Song_Count;i++)
    {
        if (in.status() != QDataStream::Ok || in.device()->bytesAvailable() < 22) return;
        quint32 song_length = 0;
        in>>song_length;

        quint16 name_length = 0;
        in>>name_length;
        if (in.status() != QDataStream::Ok || in.device()->bytesAvailable() < name_length) return;
        QByteArray name_data=packet.mid(in.device()->pos(),name_length);
        QString name=QString::fromUtf8(name_data);
        in.skipRawData(name_length);

        quint16 artist_length = 0;
        in>>artist_length;
        if (in.status() != QDataStream::Ok || in.device()->bytesAvailable() < artist_length) return;
        QByteArray artist_data=packet.mid(in.device()->pos(),artist_length);
        QString artist=QString::fromUtf8(artist_data);
        in.skipRawData(artist_length);

        quint16 album_length = 0;
        in>>album_length;
        if (in.status() != QDataStream::Ok || in.device()->bytesAvailable() < album_length) return;
        QByteArray album_data=packet.mid(in.device()->pos(),album_length);
        QString album=QString::fromUtf8(album_data);
        in.skipRawData(album_length);

        in.skipRawData(4);//跳过 分隔的0x00固定长度timesize_length(4)
        quint32 timesize;
        in>>timesize;

        quint32 music_id;
        in>>music_id;

        QList<QString> list_musicDataList;
        list_musicDataList.append(name);
        list_musicDataList.append(artist);
        list_musicDataList.append(album);
        list_musicDataList.append(QString::number(timesize));
        list_musicDataList.append(QString::number(music_id));

        list_musicList.append(list_musicDataList);
    }
    if (in.status() != QDataStream::Ok) return;
    int listSize=list_musicList.size();
    for(int i=0;i<listSize;i++)
    {
        qDebug()<<list_musicList[i][0];
        qDebug()<<list_musicList[i][1];
        qDebug()<<list_musicList[i][2];
        qDebug()<<list_musicList[i][3];
        qDebug()<<list_musicList[i][4];
    }

    //将用户的歌曲信息列表加载到本地缓存的数据库中
    QSqlQuery query(localdb);
    for(int i=0;i<listSize;i++)
    {
        query.prepare("SELECT COUNT(*) FROM user_favorite WHERE music_id=:id");
        query.bindValue(":id",list_musicList[i][4]);
        if(!query.exec())
        {
            CLIENT_LOG<<"<explainSeverUserData>Failed to query for search id.";
            return;
        }
        if(query.next()&&query.value(0).toInt()==0)
        {
            CLIENT_LOG<<"<explainSeverUserData>Not matched for id:"<<list_musicList[i][4];
            CLIENT_LOG<<"<explainSeverUserData>SureBegain to load new local music...";
        }
        else
        {
            CLIENT_LOG<<"<explainSeverUserData>Already have music of id:"<<list_musicList[i][4];
            continue;
        }

        //开始往本地数据库中加载歌曲信息
        query.prepare(
            "INSERT INTO user_favorite("
            "music_id, music_name, music_artist, music_album, music_timesize,"
            "music_file_path, music_icon_path, add_date, show_order"
            ") VALUES ("
            ":music_id, :music_name, :music_artist, :music_album, :music_timesize,"
            ":music_file_path, :music_icon_path, :add_date, :show_order"
            ")"
            );
        query.bindValue(":music_id", list_musicList[i][4]);
        query.bindValue(":music_name", list_musicList[i][0]);
        query.bindValue(":music_artist", list_musicList[i][1]);
        query.bindValue(":music_album", list_musicList[i][2]);
        query.bindValue(":music_timesize", list_musicList[i][3]);
        query.bindValue(":music_file_path", QString("xxx"));
        query.bindValue(":music_icon_path", QString("xxx"));
        query.bindValue(":add_date", QString("xxx"));
        query.bindValue(":show_order", QString("xxx"));

        // 执行并检查错误
        if (!query.exec()) {
            CLIENT_LOG << "<explainSeverUserData>Failed to insert music_data.";
            CLIENT_LOG << "SQL Error:" << query.lastError().text();
            CLIENT_LOG << "Failed Query:" << query.lastQuery();
            return;
        } else {
            CLIENT_LOG << "<explainSeverUserData>Successfully inserted music_data.";
        }
    }
    for(int i=0;i<listSize;i++)
    {
        //往qml中musicLIstListModel开始push元数据
        QString icon("qrc:/image/hslm.png");
        emit integratedMusicElementReady(icon,list_musicList[i][0],list_musicList[i][1],list_musicList[i][2],list_musicList[i][3],list_musicList[i][4]);
    }
}


