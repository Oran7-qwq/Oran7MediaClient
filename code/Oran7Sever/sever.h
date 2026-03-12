#ifndef SEVER_H
#define SEVER_H

#include <QObject>
#include<QTcpServer>
#include<QTcpSocket>
#include<QMap>
#include<QThreadPool>
#include<QRunnable>

#include"database.h"
#include"protocolhandler.h"

class Sever : public QObject
{
    Q_OBJECT
public:
    explicit Sever(QObject *parent = nullptr);
    ~Sever();

    //服务器启动
    bool startServer(quint16 port);

    //关闭服务器
    bool stopSever();

    //注册处理器
    void registerHandler(quint8 opcode,AsyncPacketHandler handler);

private slots:
    //处理新连接
    void onNewConnection();

    //处理客户端数据
    void onReadyRead();

    //客户端断开连接
    void onDisconnected();

signals:

private:
    QTcpServer *tcpSever;
    Database *dbManager;

    QThreadPool threadPool;  // 服务器线程池

    QMap<quint8,std::function<void(QTcpSocket*,QByteArray&)>> m_handlers;
    QMap<QTcpSocket*,QByteArray> m_buffers;
};

#endif // SEVER_H
