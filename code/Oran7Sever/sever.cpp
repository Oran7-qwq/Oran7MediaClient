#include "sever.h"
#include"loginhandler.h"
#include"loadingdatahandler.h"

#include<QCoreApplication>

Sever::Sever(QObject *parent)
    : QObject{parent},
    tcpSever(new QTcpServer(this)),
    dbManager(new Database(this)),
    threadPool(QThreadPool::globalInstance())
{
    //链接处理新客户端链接槽函数
    connect(tcpSever,&QTcpServer::newConnection,this,&Sever::onNewConnection);

    //设置线程池子数量,设置最大线程数
    threadPool.setMaxThreadCount(QThread::idealThreadCount());

    ///注册-处理器
    //<<==========<<注册-登录处理包>>==========>>
    this->registerHandler(Login,[this](QTcpSocket* socket,const QByteArray& packet,std::function<void(const QByteArray&)> callback){
        Q_UNUSED(socket)
        //执行子线程
        QThreadPool::globalInstance()->start([=]() {
            QByteArray response = LoginHandler(dbManager).handlePacket(packet);  // 耗时操作
            callback(response);  // 回调会在子线程执行，但会被 Sever 的 lambda 捕获并切换到主线程
        });
    });
    //<<==========<<注册-加载缓存云端数据处理包>>==========>>
    this->registerHandler(LoadingData,[this](QTcpSocket* socket,const QByteArray& packet,std::function<void(const QByteArray&)> callback){
        Q_UNUSED(socket)
        //执行子线程
        QThreadPool::globalInstance()->start([=]() {
            QByteArray response = LoadingDataHandler(dbManager).handlePacket(packet);
            callback(response);
        });
    });
}

void Sever::registerHandler(quint8 opcode, AsyncPacketHandler handler) {
    m_handlers[opcode] =
        [handler](QTcpSocket* socket, const QByteArray& packet)->void
    {
        if (!socket->isOpen()) return;
        handler(socket,packet,[socket](const QByteArray& response) {  // 传入回调函数callback
                QMetaObject::invokeMethod(qApp, [=]()
                {
                    if (socket->isOpen()&&response!=QByteArray(1,static_cast<char>(0x00))) {
                        socket->write(response);  // 主线程安全写入客户端
                    }
                }, Qt::QueuedConnection);
            }
        );
    };
}
//注册逻辑解析：
//在构造函数中调用void registerHandler(quint8 opcode,AsyncPacketHandler handler);来注册处理器函数，传入的是一个指定操作码和一个加载子线程·调用处理协议的函数对象AsyncPacketHandler
//在void Sever::registerHandler(quint8 opcode, AsyncPacketHandler handler);中首先存储map内部的函数对象：将opcode与之对应的std::function<void(QTcpSocket*,QByteArray&)>匹配，
//opcode匹配的是一个键对应的值的函数对象，以lamda表达式的形式传入， [handler]参数传入用于调用，
//在函数onReadyRead()中利用opcode查找m_handlers调用该map中匹配的函数对象时并以(auto)->it.value()(socket,packet);调用
//在该map(函数)值中：先执行if (!socket->isOpen()) return;，如果当前客户端的套接字未启动则返回不做对客户端的响应处理，防止线程穿透错乱响应
//再执行调用处理协议的函数handler，将socket、数据包packet、以及一个回调函数callback传入
//抵达加载子线程处理，
// QThreadPool::globalInstance()->start([=]() {
//     QByteArray response = LoadingDataHandler(dbManager).handlePacket(packet);
//     callback(response);
// });
//将协议处理类的方法处理完成后返回的回应数据response传入callback中
//在callback回调函数中[socket](const QByteArray& response
//  {
// QMetaObject::invokeMethod(qApp, [=]()
//     {
//         if (socket->isOpen()&&response!=QByteArray(1,static_cast<char>(0x00))) {
//             socket->write(response);  // 主线程安全写入客户端
//         }
//     }, Qt::QueuedConnection);
//  }
//若response的处理正确（即response在QByteArray中非0x00）再调用socket->write(response);安全写入客户端 ,以Qt::QueuedConnection方式建立连接

Sever::~Sever()
{
    stopSever();
}

bool Sever::startServer(quint16 port)
{
    //初始化数据库连接
    if(!dbManager->initializeDatabase())
    {
        qDebug()<<"[Sever:]Failed to initialize database";
        return false;
    }

    //启动服务器
    if(!tcpSever->listen(QHostAddress::Any,port))
    {
        qDebug()<<"[Sever:]Sever could not start!";
        qDebug()<<"Error:"<<tcpSever->errorString();
        return false;
    }


    //服务器启动成功,port端口号
    qDebug()<<"[Sever:]Sever on started on port:"<<port;
    return true;
}

bool Sever::stopSever()
{
    if(tcpSever->isListening())
    {
        tcpSever->close();
        qDebug()<<"[Sever:]Sever stoped.";
        return true;
    }
    return false;
}

void Sever::onNewConnection()
{
    QTcpSocket *socket=tcpSever->nextPendingConnection();
    qDebug()<<"[Sever:]New Client connection:"<<socket->peerAddress().toString();

    connect(socket,&QTcpSocket::readyRead,this,&Sever::onReadyRead);
    connect(socket,&QTcpSocket::disconnected,this,&Sever::onDisconnected);
}

void Sever::onReadyRead()
{
    QTcpSocket *socket=qobject_cast<QTcpSocket*>(sender());
    if(!socket) return;
    qDebug()<<"[Sever:]New Client write from:"<<socket->peerAddress();

    m_buffers[socket].append(socket->readAll());

    // qDebug()<<"[Sever:]packetSize:"<<m_buffers[socket].size();

    //<解析传输协议>
    //[总长度(4字节)]+
    //[操作指令码(1字节)]+
    //[各种指令数据]
    while(m_buffers[socket].size()>=5)//4字节长度+1字节操作码
    {
        // quint32 totalLength=*reinterpret_cast<const quint32*>(m_buffers[socket].constData());//这个默认会解析小端序列的数据
        //正确的大端序的解析方法
        const char* data = m_buffers[socket].constData();
        quint32 totalLength =
            (static_cast<quint32>(data[0]) << 24) |
            (static_cast<quint32>(data[1]) << 16) |
            (static_cast<quint32>(data[2]) << 8) |
            static_cast<quint32>(data[3]);

        //提取操作码指令
        quint8 opcode=m_buffers[socket].at(4);

        if(m_buffers[socket].size()<totalLength)
        {//数据不完整
            qDebug()<<"[Sever:]data is not enouth."<<"Expectsize:"<<totalLength<<", Actual:"<<m_buffers[socket].size();
            break;
        }
        //提取完整数据包
        QByteArray packet = m_buffers[socket].mid(0,totalLength);
        //清除缓存
        m_buffers[socket].remove(0,totalLength);
        //调用对应协议处理器
        auto it=m_handlers.find(opcode);
        if(it!=m_handlers.end())
        {
            it.value()(socket,packet);
        }
        else
        {
            qDebug()<<"[Sever:]Unknow opcode:"<<opcode;
            break;
        }
    }
}

void Sever::onDisconnected()
{
    //客户端断开连接处理
    QTcpSocket *socket=qobject_cast<QTcpSocket*>(sender());
    if(socket)
    {
        qDebug()<<"[Sever:]Client disconnected:"<<socket->peerAddress().toString();
        socket->deleteLater();
    }
}







