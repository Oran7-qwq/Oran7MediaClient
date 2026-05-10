#include "DataBase.h"

Database::Database(QObject *parent)
    : QObject{parent}
{
    //在initializeDatabase中初始化
}

Database::~Database()
{
    if(db.isOpen())
    {
        db.close();
        qDebug()<<"Database is closed.";
    }
}

//初始化数据库MYSQL
bool Database::initializeDatabase(void)
{
    //配置数据库连接
    db =QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setDatabaseName("wyy_audio_sever");
    db.setUserName("root");
    db.setPassword("117742");

    if(!db.open())
    {
        qDebug()<<"[Sever-Database:]Database connection failed:"<<db.lastError().text();
        return false;
    }
    qDebug()<<"[Sever-Database:]Database initialized successfully.";
    return true;
}

bool Database::insertAudioFile(const QString &fileName,const QString &filePath)
{
    if(!db.isOpen())
    {
        qDebug()<<"[Sever-Database:]Database is not connection";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO audio_files (file_name,file_path) VALUES (:file_name,:file_path)");
    query.bindValue(":file_name",fileName);
    query.bindValue(":file_path",filePath);

    if(!query.exec())
    {
        qDebug()<<"Failed insert into audio file into database."<<db.lastError().text();
        return false;
    }
    qDebug()<<"[Sever-Database:]Successfully insert audio file into database";
    return true;
}
//<<=========<<LoginHandler_Calss>>==========>>

//登录成功查询
int Database::validateLogin(QString username,QString password)
{
    //1：账号不存在
    //2：密码不正确
    //0：成功登录
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM users WHERE username = :username");
    query.bindValue(":username", username);
    if(query.exec()&&query.next()&&query.value(0).toInt()!=0)
    {
        //账号存在再查询密码是否匹配
        query.prepare("SELECT COUNT(*) FROM users WHERE username = :username AND password = :password");
        query.bindValue(":username",username);
        query.bindValue(":password",password);
        if(query.exec()&&query.next()&&query.value(0).toInt()!=0)
        {
            //账号密码都存在
            qDebug()<<"[Database:]username:"<<username<<" ,password:"<<password<<"All right.";
        }
        else
        {
            qDebug()<<"[Database:]username:"<<username<< "of password Error.";
            return 2;
        }
    }
    else
    {
        qDebug()<<"[Database:]username:"<<username<<" is Not Exit";
        return 1;
    }
    return 0;
}

//获取对应用户的UID
quint32 Database::getUID(QString username)
{
    QSqlQuery query(db);
    query.prepare("SELECT UID FROM users WHERE username = :username");
    query.bindValue(":username",username);

    quint32 UID;
    if(!(query.exec()&&query.next()))
    {
        qDebug()<<"[Database:]<getUID>Failed to query";
    }

    UID =  query.value(0).toInt();

    return UID;
}

//获取用户喜欢的音乐列表及内容
QList<QList<QString>> Database::getUserLikeMusicListData(quint32 UID)
{
    QList<QList<QString>> list_musicList;

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM user_favorite WHERE UID = :uid");
    query.bindValue(":uid",UID);
    if(!(query.exec()&&query.next()))
    {
        qDebug()<<"[Database:]<getUserLikeMusicListData>Failed to query";
        // throw("[Database:]<getUserLikeMusicListData>Failed to query");
    }
    int List_musicNum=query.value(0).toInt();
    for(int i=0;i<List_musicNum;i++)
    {
        QString musicQueryStr = QString("SELECT music_name,music_artist,music_album,music_timesize,music_id "
                                        "FROM user_favorite WHERE UID = :uid LIMIT 1 OFFSET %1").arg(i);
        QSqlQuery musicQuery(db);
        musicQuery.prepare(musicQueryStr);
        musicQuery.bindValue(":uid",UID);
        if(!(musicQuery.exec()&&musicQuery.next()))
        {
            qDebug()<<"[Database:]<getUserLikeMusicListData>Failed to musicQuery element i->"<<i;
            // throw(QString("[Database:]<getUserLikeMusicListData>Failed to musicQuery element i->%1").arg(i));
            continue;
        }
        QList<QString> list_musicInfo;
        list_musicInfo.append(musicQuery.value(0).toString());
        list_musicInfo.append(musicQuery.value(1).toString());
        list_musicInfo.append(musicQuery.value(2).toString());
        list_musicInfo.append(QString::number(musicQuery.value(3).toInt()));
        list_musicInfo.append(QString::number(musicQuery.value(4).toInt()));
        list_musicList.append(list_musicInfo);
    }

    QSqlQuery query_(db);
    QString query_str;
    query_str = QString(
        "CREATE DATABASE LoanDatabase;");
    query_.exec();
    query_str = QString(
        "USE LoanDatabase;");
    query_.exec();
    query_str = QString(
        "IF OBJECT_ID(N'Loans', N'U') IS NOT NULL"
        "DROP TABLE Loans;");

    return list_musicList;
}
