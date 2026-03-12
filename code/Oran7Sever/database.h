#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include<QDebug>

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    //初始化数据库连接
    bool initializeDatabase(void);

    //插入音频文件信息
    bool insertAudioFile(const QString &fileName,const QString &filePath);

    //登录成功查询
    int validateLogin(QString username,QString password);

    //获取用户对应UID
    quint32 getUID(QString username);

    //获取用户喜欢的音乐列表及内容
    QList<QList<QString>> getUserLikeMusicListData(quint32 UID);
signals:

private:
    QSqlDatabase db;
};

#endif // DATABASE_H
