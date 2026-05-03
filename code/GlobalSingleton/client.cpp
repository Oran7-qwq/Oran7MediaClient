#include "client.h"

#include "ffmsg.h"
#include "globalhelper.h"
#include "appjsonconfigmanager.h"
#include "applicationcontext.h"
#include "d3d11deviceprovider.h"

#include <thread>
#include <functional>
#include <utility>

#include <QFile>
#include <QByteArray>
#include <QSqlError>
#include <QList>
#include <QPair>
#include <QCollator>

#include <QStandardPaths>
#include <QDirIterator>
#include <QDebug>
#include <QFileInfo>
#include <QPair>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QQuickItem>

Client::Client(const QString &host,quint16 port,QObject *parent)
    : QObject{parent},
    Client_socket(new QTcpSocket(this))
{
    //初始化本地缓存数据，初始化本地数据库
    UID=0;
    localdb=QSqlDatabase::addDatabase("QSQLITE","LocalSqlite_connection");//Sever中默认链接QMYSQL引擎默认命名qt_sql_default_connection，在此另起更名LocalSqlite_connection
    localdb.setDatabaseName("SQLite/userLocal.db");
    if(!localdb.open())
    {
        qWarning("Failed connected to database, Unable to open database %s", "userLocal");
        qWarning() << "Failed to open database:" << localdb.lastError().text();
    }
    connect(Client_socket,&QTcpSocket::connected,this,&Client::onConnected);
    connect(Client_socket,&QTcpSocket::disconnected,this,&Client::onDisconnected);

    //客户端连接到服务器
    Client_socket->connectToHost(host,port);

    //注册客户端响应服务器协议处理器
    //<<-----<<登录验证响应>>----->>
    this->registerHandler_(Login_,[this](quint32 totalLength,quint8 opcode,const QByteArray &packet){
        //处理登录响应
        loginResponse(totalLength,opcode,packet);
    });
    //<<-----<<云端基本数据传输响应----->>
    this->registerHandler_(LoadingData_,[this](quint32 totalLength,quint8 opcode,const QByteArray &packet){
        //处理登录响应
        explainSeverUserData(totalLength,opcode,packet);
    });

    //用于客户端底部进度条更新状态，请求内层按时发送当前播放的位置pts
    Progress_SliderPos_ReqUpdateTimer =new QTimer(this);
    //connections
    InitSignalsAndSlots();

    //启动防ui频繁请求触发工具QElapsedTimer  ，已在[Client::qmlClickedReqPreparePlayMusic:]中使用
    m_callTimer.start();

    //Oran7ScreenCaptureController
    // if (!m_screenCap) m_screenCap = new Oran7ScreenCaptureController(this);
}

Client::~Client()
{
    if(localdb.isOpen())
        localdb.close();
    if(mp_!=nullptr)//确保释放Oran7MediaPlayer内层
    {
        mp_->oran7mp_destroy();
        delete mp_;
        mp_ = nullptr;
    }
    //释放定时器
    Progress_SliderPos_ReqUpdateTimer->stop();
    delete Progress_SliderPos_ReqUpdateTimer;

    for (auto& conn : connections) {
        QObject::disconnect(conn);
    }
}

int Client::InitSignalsAndSlots()
{
    QPointer<Client> self(this);
    //============[Oran7MediaPlayer]============
    connections << connect(this,&Client::SigPlayOrPause,this,[=](){
        CLIENT_LOG<<"SigPlayOrPause Reveived";
        this->OnPlayOrPause();
        },Qt::QueuedConnection);
    connections << connect(this,&Client::sigStop,this,[=](){
        this->OnStop();
        },Qt::QueuedConnection);
    connections << connect(this->Progress_SliderPos_ReqUpdateTimer,&QTimer::timeout,this,[=](){
        reqUpdateCurrentPosition();
        },Qt::QueuedConnection);

    this->Progress_SliderPos_ReqUpdateTimer->start(100);//启动触发前端播放进度条更新的定时器,间隔10ms

    connections << connect(this,&Client::reOrder_localMusicList,this,[self](const QVariantList reOrdered_list){
        QMutexLocker locker(&self->client_mutex);
        self->appData.Custom_LocalMusic_playOrder = reOrdered_list;//reset
        // this->refreshLocalMusicList();
        },Qt::QueuedConnection);

    //Get d3dDev from D3D11DeviceProvider
    //传递·D3D11 Device from Qt device
    connections << connect(D3D11DeviceProvider::instance(), &D3D11DeviceProvider::deviceReady,
        this, [this](/**dev*/){
            CLIENT_LOG << "Client received ID3D11Device.";
            // -->从 provider acquire，确保 AddRef 生命周期正确
            m_qtDevice.Attach(D3D11DeviceProvider::instance()->acquireDevice());
            // 如果 mp_ 已经存在,可以立即 set
            if (mp_) {
                setD3D11Device(m_qtDevice.Get());
            }
            //Set Oran7ScreenCaptureController D3D11Device
        },Qt::QueuedConnection);
    //Oran7ScreenCapture error info
    connections << connect(m_screenCap,&Oran7ScreenCaptureController::errorOccurred,
        this,[this](const QString &err){
            WARNING_LOG<<err;
        },Qt::QueuedConnection);

    return 0;
}

QString Client::createAppDirectories()
{
    //获取系统临时目录文件
    // QString tempPath = QDir::tempPath(); //<--Discard
    //创建应用主目录 Oran7CloudMusic
    this->appData.appDirPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir appDir(appData.appDirPath);
    if (!appDir.exists())
    {
        if (!appDir.mkpath("."))
        {
            WARNING_LOG << "Client::createAppDirectories:Could not Create request Path:" << appData.appDirPath;
            return QString();
        }
    }
    //在应用目录下创建 audio 子目录
    this->appData.audioDirPath = appData.appDirPath + "/audio";
    QDir audioDir(appData.audioDirPath);
    if (!audioDir.exists())
    {
        if (!audioDir.mkpath("."))
        {
            WARNING_LOG << "Client::createAppDirectories:Could not Create request Path:"  << appData.audioDirPath;
            return QString();
        }
    }
    //audioCover存储封面子目录
    this->appData.audioCoverDirPath = appData.audioDirPath + "/audioCover";
    QDir audioCoverDir(appData.audioCoverDirPath);
    if(!audioCoverDir.exists())
    {
        if(!audioCoverDir.mkdir("."))
        {
            WARNING_LOG << "Client::createAppDirectories:Could not Create request Path:"  << appData.audioCoverDirPath;
            return QString();
        }
    }
    return QDir(appData.audioDirPath).absolutePath();
}

void Client::saveConfig_lastCloseAppFocusedMusic()
{
    QMutexLocker loceker(&client_mutex);
    if(this->appData.audioAbsoluteFilePath.isEmpty())return;
    AppConfigManager::instance().setLastUsedFile(this->appData.audioAbsoluteFilePath);
}

void Client::loadConfig_lastCloseAppFocusedMusic()
{
    QString lastFilePath = AppConfigManager::instance().getLastUsedFile();
    QMap<QString,QVariant> mediaInfo= this->analyzeMediaFileInfo(lastFilePath);
    if(mediaInfo["success"].toInt() !=0)
    {
        CONFIG_LOG<<"Client::loadLastCloseAppFocusedMusic:mediaInfo extract Failed:"<<mediaInfo["success"].toInt();
        return;
    }
    /*编码封面路径*/
    QString coverPath = mediaInfo["cover"].toString();
    // 确保路径格式正确
    if (!coverPath.startsWith("file:///")) {
        coverPath = "file:///" + QDir::toNativeSeparators(coverPath);
    }
    // 对特殊字符进行URL编码
    QUrl url(coverPath);
    QString encodedPath = url.toString(QUrl::FullyEncoded);

    emit configSignal_loadLastCloseAppFocusedMusic(
        encodedPath,                              //cover Path
        mediaInfo["title"].toString(),         //music_name
        mediaInfo["artist"].toString(),       //music_artist
        mediaInfo["album"].toString(),     //music_album
        mediaInfo["duration"].toString(),   //music_duration
        QString::number(-1),                    //music_id
        lastFilePath);                                //music_localPath
}

/**
 * @brief Client::getNestedValue
 * @param keys
 * @param obj
 * @return
 */
QVariant Client::getNestedValue(const QStringList& keys, const QJsonObject& obj)
{
    if (keys.isEmpty()) return QVariant();

    QString currentKey = keys.first();
    if (!obj.contains(currentKey))
    {
        return QVariant();
    }

    QJsonValue value = obj.value(currentKey);

    if (keys.size() == 1) {
        // 最后一级键
        if (value.isBool()) return value.toBool();
        if (value.isDouble()) return value.toDouble();
        if (value.isString()) return value.toString();
        if (value.isArray()) return value.toArray().toVariantList();
        if (value.isObject()) return value.toObject().toVariantMap();
        return QVariant();
    }

    if (value.isObject())
    {
        return getNestedValue(keys.mid(1), value.toObject());//继续调用递归
    }

    return QVariant();
}

/**
 * @brief extractMusicInfoTag
 * @param root
 * @param tagName
 * @param defaultValue
 * @return
 */
QString Client::extractMusicInfoTag(const QJsonObject& root, const QString& tagName, const QString& defaultValue)
{
    // 定义可能的键名变体（按可能性排序）
    QStringList possibleKeys;

    //信息标签灵活推测
    if (tagName.toLower() == "album") {
        possibleKeys = {"album", "ALBUM", "Album"};
    }
    else if (tagName.toLower() == "artist") {
        possibleKeys = {"artist", "ARTIST", "Artist"};
    }
    else if (tagName.toLower() == "title") {
        possibleKeys = {"title", "TITLE", "Title"};
    }
    else {
        possibleKeys = {tagName.toLower(), tagName.toUpper(),
                        tagName.toLower().replace(0, 1, tagName[0].toUpper())};
    }

    // 尝试每种可能的键名
    for (const QString& key : std::as_const(possibleKeys))
    {
        QVariant value = getNestedValue({"format", "tags", key}, root);
        if (value.isValid() && !value.isNull() && !value.toString().isEmpty())
        {
            return value.toString();
        }
    }

    return defaultValue;//返回经过推测的键对应的值
}

// 使用示例

/**
 * @brief 从ffprobe JSON结果中提取媒体信息
 * @param root ffprobe的JSON根对象
 * @return 包含媒体信息的结构体或QMap
 */
QMap<QString, QVariant> Client::extractMediaInfo(const QJsonObject& root)
{
    QMap<QString, QVariant> mediaInfo;

    // 使用getNestedValue工具函数提取信息

    QVariant duration = getNestedValue({"format", "duration"}, root);
    //应对album,artist,title可能出现不同标准的大小写标签 ，内部仍调用getNestedValue递归遍历提取
    QVariant album = extractMusicInfoTag(root,"album","未知专辑");
    QVariant artist = extractMusicInfoTag(root,"artist","未知歌手");
    QVariant title = extractMusicInfoTag(root,"title","未知歌曲");

    // 处理时长
    if (duration.isValid())
    {
        bool ok;
        double seconds = duration.toString().toDouble(&ok);
        mediaInfo["duration"] = ok ? QString::number(static_cast<int>(seconds)) : "0";
    }
    else
    {
        mediaInfo["duration"] = "0";
    }

    // 处理文本信息
    mediaInfo["album"] = album.isValid() ? album.toString() : "未知专辑";
    mediaInfo["artist"] = artist.isValid() ? artist.toString() : "未知艺术家";
    mediaInfo["title"] = title.isValid() ? title.toString() : "未知标题";

    return mediaInfo;
}

/**
 * @brief Client::analyzeMediaFileInfo
 * @param filepath
 * @return
 */
QMap<QString, QVariant> Client::analyzeMediaFileInfo(QString filepath)
{

    QMap<QString, QVariant> mediaInfo;
    mediaInfo["success"] = 0; //设置一个处理成功标志位  0:successed

    QFileInfo fileInfo(filepath);

    //获取ffmpeg中分析媒体文件处理工具ffprobe
    QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    QString ffprobe_exePath = QDir::toNativeSeparators(appDir + "/ffprobe.exe");
#else
    QString ffprobe_exePath = QDir::toNativeSeparators(appDir + "/ffprobe");
#endif

    //实例化QProcess处理执行ffprobe
    QProcess ffprobe;
    QString absFilePath = fileInfo.absoluteFilePath();
    QString absFFprobePath = QFileInfo(ffprobe_exePath).absoluteFilePath();
    ffprobe.setWorkingDirectory(QCoreApplication::applicationDirPath());

    QStringList ffprobe_args;
    ffprobe_args << "-threads" << "1"
                << "-print_format" << "json"
                << "-show_format"
                << "-i"
                << absFilePath;
    ffprobe.start(absFFprobePath, ffprobe_args);
    //等待进程完成（最多30秒）
    if (!ffprobe.waitForFinished(30000))
    {
        // qDebug() << "Starting FFprobe with arguments:";
        // qDebug() << "  Executable:" << ffprobe_exePath;
        // qDebug() << "  Working dir:" << ffprobe.workingDirectory();
        WARNING_LOG << "Client::analyzeMediaFileInfo:FFprobe timed out for file:" << fileInfo.absoluteFilePath();
        ffprobe.kill(); // 强制终止进程

        //修改success标志并直接返回
        mediaInfo["success"] = 1;
        return mediaInfo;
    }

    if (ffprobe.exitCode() != 0)
    {
        WARNING_LOG << "Client::analyzeMediaFileInfo:FFprobe ERROR:" << ffprobe.readAllStandardError();
        //修改success标志并直接返回
        mediaInfo["success"] = 2;
        return mediaInfo;
    }
    //读取ffprobe输出的Json信息
    QByteArray output = ffprobe.readAllStandardOutput();
    if (output.isEmpty())
    {
        WARNING_LOG << "Client::analyzeMediaFileInfo:FFprobe returned empty output for file:" << fileInfo.absoluteFilePath();
        //修改success标志并直接返回
        mediaInfo["success"] = 3;
        return mediaInfo;
    }
    //解析Json信息
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(output,&parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        WARNING_LOG << "Client::analyzeMediaFileInfo:JSON parse error:" << parseError.errorString();
        //修改success标志并直接返回
        mediaInfo["success"] = 4;
        return mediaInfo;
    }
    QJsonObject root = doc.object();

    /*调用整理好的模块化提取函数*/
    mediaInfo = extractMediaInfo(root);

    if(mediaInfo["duration"].toInt() <=0)
    {
        //媒体时长信息提取失败......
        //修改success标志并直接返回
        mediaInfo["success"] = 5;
        return mediaInfo;
    }
    /*开始提取封面图片*/
#ifdef Q_OS_WIN
    QString ffmpegPath = QDir::toNativeSeparators(appDir + "/ffmpeg.exe");
#else
    QString ffmpegPath = QDir::toNativeSeparators(appDir + "/ffmpeg");
#endif
    //创建封面图片输出路径
    QString coverPath = fileInfo.absolutePath() + "/audioCover/" +     //<--------auidoCoverPath
                        fileInfo.completeBaseName() + ".jpg";
    mediaInfo["cover"] = coverPath;
    //首先要检测是已经存在已有的封面避免重复拷贝
    if(!QFile::exists(coverPath))
    {
        //ffprobe parameters setting
        QStringList arguments02;
        arguments02 << "-i" << absFilePath
                    << "-map" << "0:v?"
                    << "-c" << "copy"
                    << coverPath;
        //ffmpeg.exec()
        QProcess ffmpeg;
        ffmpeg.start(ffmpegPath, arguments02);
        if (!ffmpeg.waitForFinished(30000))
        {
            WARNING_LOG << "Client::analyzeMediaFileInfo:Getting audioCover time out:" << absFilePath;
            ffmpeg.kill();
            //修改success标志并直接返回
            mediaInfo["success"] = 6;
            return mediaInfo;
        }
        if (ffmpeg.exitCode() != 0)
        {
            WARNING_LOG << "Client::analyzeMediaFileInfo:ffmpeg exit ERROR of" << absFilePath;
            WARNING_LOG << "Client::analyzeMediaFileInfo:ffmpeg exit ERROR:" << ffmpeg.readAllStandardError();
            //修改success标志并直接返回
            mediaInfo["success"] = 7;
            return mediaInfo;
        }
        // 验证封面文件  小于1KB MayBe无效封面
        if (QFileInfo(coverPath).size() < 1024)
        {
            WARNING_LOG << "Client::analyzeMediaFileInfo:AudioCover file may be invalid:" << coverPath;
            QFile::remove(coverPath);
        }
    }
    //成功返回
    return mediaInfo;
}

void Client::qmlClickedReqPreparePlayMusic(QString file_path)
{
    if(file_path.isEmpty())return;
    appData.audioAbsoluteFilePath = file_path;
    this->preparePlayingMedia(file_path);
}

void Client::qmlClickedReqPreparePlayVideo(QString file_path)
{
    if(file_path.isEmpty())return;
    appData.videoAbsoluteFilePath = file_path;
    this->preparePlayingMedia(file_path);
}

void Client::preparePlayingMedia(QString file_path)
{
    QMutexLocker locker(&client_mutex);

    if(file_path.isEmpty())return;
    INFO_LOG<<"REQ For Prepare Play:"<<file_path;
    if(!appData.CurMediaFilePath.isEmpty())
    {
        if(appData.CurMediaFilePath!=file_path)
        {
            if(mp_)
            {
                //播放中--->切换播放文件，防频繁点击事件
                if (m_callTimer.elapsed() < CALL_INTERVAL_MS)//100ms
                {
                    INFO_LOG << "Client::qmlClickedReqPreparePlayMusic:The call is too frequent and the request has been ignored";
                    return;
                }
                m_callTimer.restart();
                appData.CurMediaFilePath=file_path;
                emit this->sigStop();
                //休眠等待100ms，待内存播放器进程销毁后再触发重启
                QTimer::singleShot(100, this,[this](){emit this->SigPlayOrPause();});
                return;
            }
            else
            {
                //到这里指：上次播放的媒体已经结束，并请求播放下一首和上次不同的文件
                appData.CurMediaFilePath=file_path;
                emit this->SigPlayOrPause();
                return;
            }
        }
        else
        {
            //上一次播放不为空，而且这次请求的路径也是一样的，那就直接暂停
            emit this->SigPlayOrPause();
            return;
        }
    }
    else
    {
        //上一次播放为空，直接复制路径然后触发播放
        appData.CurMediaFilePath=file_path;
        emit this->SigPlayOrPause();
        return;
    }
}

//注册客户端响应服务器协议处理器方法——绑定操作码(动态协议处理)
void Client::registerHandler_(quint8 opcode,Packedhandler_ handler)
{
    Client_handler[opcode]=handler;
}

void Client::onConnected()
{
    CLIENT_LOG << "connected Sever host:" << Client_socket->peerAddress()<<", port:"<<Client_socket->peerPort();
    connect(Client_socket,&QTcpSocket::readyRead,this,&Client::onReadReady);
}

void Client::onDisconnected()
{
    CLIENT_LOG<<"Disconnected from server";
}

void Client::onReadReady()
{
    QTcpSocket *socket=qobject_cast<QTcpSocket*>(sender());
    //验证是同一客户端提示
    Q_ASSERT(socket==Client_socket);
    CLIENT_LOG<<socket->peerAddress()<<"be responsed.";

    //向缓冲区读入数据
    responseFromSever.append(socket->readAll());

    CLIENT_LOG<<"[CLIENT_BUFFER:]responseFromSever:"<<responseFromSever.toHex(' ');
    while(responseFromSever.size()>=5)
    {
        QDataStream stream(responseFromSever);
        stream.setByteOrder(QDataStream::BigEndian); // 根据协议设置字节序
        quint32 totalLength;
        stream >> totalLength;

        if(responseFromSever.size()<totalLength)
        {
            //数据传输是分块发送的，如果缓冲区数据不完整，继续读取
            CLIENT_LOG<<"respons not enought.Expected:"<<totalLength<<" ,Actual:"<<responseFromSever.size();
            CLIENT_LOG<<"CLIENT_BUFFER:responseFromSever:"<<responseFromSever.toHex(' ');
            break;
        }
        //数据完整，开始解析
        CLIENT_LOG<<"Data is already ";

        // quint8 opcode=*reinterpret_cast<const quint8*>(&responseFromSever.constData()[4]);//用指针转换方法
        quint8 opcode=static_cast<quint8>(responseFromSever.at(4));
        if(opcode!=Login_&&opcode!=LoadingData_)
        {
            CLIENT_LOG<<"Opcode of Login is Error."<<"Comming Opcode:"<<opcode;
            return;
        }

        //提取预期的完整数据包
        QByteArray packet=responseFromSever.mid(0,totalLength);
        //清除0级缓存
        responseFromSever.remove(0,totalLength);
        // qDebug()<<"[CLIENT_BUFFER:]responseFromSever:"<<responseFromSever;

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
void Client::validateLogin(QString username,QString password)
{
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
    CLIENT_LOG<<"password.size:"<<password.size();
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
    CLIENT_LOG << "query:" << query.toHex(' ');
    Client_socket->write(query);
    Client_socket->flush();//强制刷新缓冲区快速获得回应
}

//处理登录响应
void Client::loginResponse(quint32 totalLength, quint8 opcode, const QByteArray &packet)
{
    Q_UNUSED(totalLength)
    Q_UNUSED(opcode)
    //这里从中提取的初始位参考服务器端的传输协议
    //总长度4+回应指令1+状态码1+用户UID4+[usernameLength]1+用户名length
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
        const char *uid_data=&packet.constData()[6];
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
void Client::loadingSeverUserDataQuery()
{
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
    // qDebug()<<"[Client:]UID:"<<UID;
    CLIENT_LOG << "query size:" << query.size();
    CLIENT_LOG << "query:" << query.toHex(' ');
    Client_socket->write(query);
    Client_socket->flush();//强制刷新缓冲区快速获得回应
}

void Client::explainSeverUserData(quint32 totalLength, quint8 opcode, const QByteArray &packet)
{
    Q_UNUSED(totalLength)
    Q_UNUSED(opcode)

    qDebug()<<packet.toHex(' ');
    //<传输协议>
    //[协议头:4B total_length + 1B opcode + 4B song_count]
    //[歌曲1: 4B length + (2B name_len + name + 2B artist_len + artist + 2B album_len + album +4B[skip0x00]+ 4B timesize + 4B music_id)]
    //[歌曲2: 4B length + ...]
    //<传输协议解码流>
    QDataStream in(packet);
    in.setByteOrder(QDataStream::BigEndian);//大端序解析
    quint32 Song_Count;
    in>>totalLength>>opcode>>Song_Count;
    CLIENT_LOG<<"Song_Count:"<<Song_Count;
    QList<QList<QString>> list_musicList;
    for(quint32 i=0;i<Song_Count;i++)
    {
        quint32 song_length;
        in>>song_length;

        quint16 name_length;
        in>>name_length;
        QByteArray name_data=packet.mid(in.device()->pos(),name_length);
        QString name=QString::fromUtf8(name_data);
        in.skipRawData(name_length);

        quint16 artist_length;
        in>>artist_length;
        QByteArray artist_data=packet.mid(in.device()->pos(),artist_length);
        QString artist=QString::fromUtf8(artist_data);
        in.skipRawData(artist_length);

        quint16 album_length;
        in>>album_length;
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
        emit pushMyFavoriteMusicLIstListModel_ElementData(icon,list_musicList[i][0],list_musicList[i][1],list_musicList[i][2],list_musicList[i][3],list_musicList[i][4]);
    }
}


//===============================<Oran7MediaPlayer>======================//

int Client::message_loop(void *arg)
{
    Oran7MediaPlayer *mp = (Oran7MediaPlayer *)arg;
    // 线程循环
    CLIENT_LOG << "Client::message_loop:message_loop into";
    while (1)
    {
        AVMessage msg;
        //取消息队列的消息，如果没有消息就阻塞，直到有消息被发到消息队列。
        int retval = mp->oran7mp_get_msg(&msg, 1);    // 主要处理Java->C的消息

        if (retval < 0)
            break;
        switch (msg.value) {
        case FFP_MSG_FLUSH:
            CLIENT_LOG << __FUNCTION__ << " FFP_MSG_FLUSH";
            break;
        case FFP_MSG_PREPARED:
            CLIENT_LOG << __FUNCTION__ << " FFP_MSG_PREPARED";
            //准备就绪，通过oran7mp_start()发送FFP_REQ_START启动请求
            mp->oran7mp_start();
            break;
        case FFP_MSG_PLAY_FNISH:
            CLIENT_LOG << __FUNCTION__ << "FFP_MSG_PLAY_FNISH";
            /*数据播放完毕，停止播放*/
            emit this->updataQmlTransforStopIcon();//更新qml前端暂停图标为停止
            emit this->sigStop();//停止并销毁Oran7MediaPlayer，以及其中内层的所有子线程，在安静时清空播放器运行资源
            shutting_down_ = false;//主动播放结束-->重置不拦截video frame
            /*触发自动播放下一首*/
            this->reqPlayNext();
            break;
        case FFP_MSG_FIND_STREAM_INFO:
            CLIENT_LOG << __FUNCTION__ << "FFP_MSG_FIND_STREAM_INFO";
            /*获取当前媒体文件总播放时长*/
            getTotalDuration();
            break;
        case FFP_MSG_SEEK_COMPLETE:
            req_seeking_ = false;
            break;
        default:
            CLIENT_LOG  << __FUNCTION__ << " default " << msg.value <<"";
            break;
        }
        msg_free_obj_res(&msg);
        // 休眠10ms
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        /*Ui请求播放进度条更新*/
        // reqUpdateCurrentPosition();
    }
    CLIENT_LOG << "Client::message_loop:message_loop leave"<< "";
    return 0;
}

void Client::OnPlayOrPause()
{
    INFO_LOG << "Client::OnPlayOrPause:OnPlayOrPause signal called.";
    int ret = 0;
    // 先检测mp是否已经创建
    if(!mp_)
    {
        //在此处创建播放器Native层对象Oran7MediaPlayer实例
        mp_ = new Oran7MediaPlayer();
        //------>注册回调函数Client::message_loop();
        /*​​std::placeholders::_1​​：表示 message_loop的第一个参数是一个 ​​占位符​​，由 oran7mp_create在调用时传入。
        this会被绑定为 message_loop的 arg 参数，
        类似于lamada表达式:[this](const std::string& msg) { this->message_loop(msg); }
        */
        //准备工作,在oran7mp_create内部创建深层播放器FFPlayer对象实例,并给Oran7MediaPlayer绑定message_loop
        ret = mp_->oran7mp_create(std::bind(&Client::message_loop, this, std::placeholders::_1));
        if(ret<0)
        {
            INFO_LOG << "Client::OnPlayOrPause:Oran7MediaPlayer create failed";
            delete mp_;
            mp_ = NULL;
            return;
        }
        mp_->AddVideoRefreshCallback(std::bind(&Client::OutputVideo, this,std::placeholders::_1,std::placeholders::_2));
        setD3D11Device(this->m_qtDevice.Get());//绑定QtScencGraphi正在使用的D3D11Device
        //设置url播放资源
        {
            QMutexLocker locker(&client_mutex);
            mp_->oran7mp_set_data_source(QString(appData.CurMediaFilePath).toUtf8());//<---default
            //TEXT
            //mp_->oran7mp_set_data_source("C:/Users/funny/Videos/zzz1.mp4");
            //mp_->oran7mp_set_data_source("C:/Users/funny/QtProject.doc/MediaTestSrc/4K极致_8K__120fps_带来全球最美的_HDR.mp4");
            //mp_->oran7mp_set_data_source("C:/Users/funny/QtProject.doc/MediaTestSrc/TIS_4K_fps60-80.mp4");
            //mp_->oran7mp_set_data_source("C:/Users/funny/QtProject.doc/MediaTestSrc/TIS_2K_60fps.mp4");
            //mp_->oran7mp_set_data_source("https://cn-hnzz-cm-01-01.bilivideo.com/live-bvc/562932/live_392836434_52698792_1500.flv?expires=1767704937&pt=web&deadline=1767704937&len=0&oi=3748162459&platform=web&qn=150&trid=10006ab3e23531abe017336e424201695cfb&uipk=100&uipv=100&nbs=1&bmt=1&uparams=cdn,deadline,len,oi,platform,qn,trid,uipk,uipv,nbs,bmt&cdn=cn-gotcha01&upsig=eb2a76a19fc2a9093dcd140516e09f1a&site=cb32b561aa8a44b30694506800228399&free_type=0&mid=0&sche=ban&sid=cn-hnzz-cm-01-01&chash=0&sg=lr&trace=8388673&isp=cm&rg=Central&pv=Henan&score=100&codec=0&p2p_type=1&pp=rtmp&suffix=1500&long_ab_flag_value=test&long_ab_flag=live_default_longitudinal&strategy_ids=122&strategy_types=1&source=puv3_onetier&sl=2&strategy_version=latest&long_ab_id=45&deploy_env=prod&info_source=origin&hdr_type=0&media_type=0&sk=4df5cbc6ceaea7e09d6f0935100e745e&hot_cdn=909701&origin_bitrate=1351&vd=nc&zoneid_l=151388163&sid_l=live_392836434_52698792_1500&src=puv3&order=1");
        }
        ret = mp_->oran7mp_prepare_async();
        if(ret <0)
        {
            //出现异常，准备失败
            INFO_LOG << "Client::OnPlayOrPause:Oran7MediaPlayer create failed";
            delete mp_;
            mp_ = NULL;
            return;
        }
        INFO_LOG<<"Client::OnPlayOrPause:Successed create and prepare async of Oran7MediaPlayer. ";
        /*设置默认保存的音量条*/
    }
    else
    {
        INFO_LOG<<"Client::OnPlayOrPause:All Already _ User Clicked PlayOrPause. ";
        if(mp_->oran7mp_get_state() == MP_STATE_STARTED)
        {
            INFO_LOG<<"Client::OnPlayOrPause:Will be paused. ";
            mp_->oran7mp_pause();
        }
        else if(mp_->oran7mp_get_state() == MP_STATE_PAUSED)
        {
            INFO_LOG<<"Client::OnPlayOrPause:Will be started. ";
            mp_->oran7mp_start();
        }
    }
}

void Client::OnStop()
{
    INFO_LOG << "Client::OnStop:OnStop signal called.";
    if(mp_)
    {
        INFO_LOG << "Client::OnStop:Destorying FFPlayer.";
        delete mp_;
        mp_ = nullptr;
    }
}

int Client::progressSlider_Seek(int cur_valule)
{
    //curvalue  单位s
    INFO_LOG<<"Client::ProgressSlider_Seek:Client Request Progress Slider seet to:"<<cur_valule;
    if(mp_)
    {
        req_seeking_=true;
        int64_t milliseconds = cur_valule * 1000;
        mp_->oran7mp_seek_to(milliseconds);
    }
    else return -1;
    return 0;
}

void Client::reqChangeVolumeValue(int cur_valume)
{
    this->appData.playerVolume = cur_valume;
    DEFAULT_VOLUM_SIZE = SDL_MIX_MAXVOLUME * cur_valume * 1.0 / 100;
    if(mp_==nullptr)return;
    mp_->oran7mp_set_playback_volume(cur_valume);
}

void Client::createVideoItem(const RenderObject key,QQuickItem *host)
{
    if (!host) {
        WARNING_LOG << "createVideoItem: host is nullptr";
        return;
    }

    auto &s = m_d3d11Slots[key];
    if (!s.item) {
        D3D11VideoItem* item = new D3D11VideoItem(nullptr);
        if(item)
        {
            //<----Successed create D3D11VideoItem
            s.item = item;
            //传递 D3D11VideoItem::sourceVideoSize,回调syncVideoItemSize,设置VideoRenderItem中渲染大小
            connect(s.item, &D3D11VideoItem::sourceSizeChanged,
                    this, [this,key](int w, int h){ setVideoSourceSize(key,QSize(w,h)); },
                    Qt::QueuedConnection);
            //传递 D3D11VideoItem中updatePaintNode 获取到的VideoInfo
            connect(s.item,&D3D11VideoItem::sendVideoFrameInfo,this,[this,key](Oran7VideoInfo info){
                //加入ScaleMode信息
                const auto mode = m_d3d11Slots[key].scaleMode;
                switch (mode) {
                case ScaleMode::Fit:
                    info.fillModeName = "Fit";
                    break;
                case ScaleMode::Fill:
                    info.fillModeName = "Fill";
                    break;
                default:
                    break;
                }
                emit updateQmlRenderedVideoInfo(Oran7VideoInfo_To_QVariantMap(info));
            });
            INFO_LOG << "Success create D3D11VideoItem of "<<(key ==RenderObject::VideoPlayerRender ? "VideoPlayerRender" : "ScreenCaptureRender");
        }
        else
        {
            WARNING_LOG<<"Failed to create D3D11VideoItem !!!";
        }
    }

    s.item->setParentItem(host);
    s.item->setParent(host);
    s.item->setVisible(true);

    QPointer<QQuickItem> safeHost(host);

    auto tryHook = [this, safeHost, key]() {
        if (!safeHost) return false;
        QQuickWindow *w = safeHost->window();
        if (!w) return false;

        D3D11DeviceProvider::instance()->attachWindow(w);

        auto item = m_d3d11Slots[key].item;
        if (item) QMetaObject::invokeMethod(item, "update", Qt::QueuedConnection);
        return true;
    };

    if (!tryHook()) {
        // 等 host 真正进场景后再拿 window
        QObject::connect(host, &QQuickItem::windowChanged,
                         this, [tryHook](QQuickWindow*) { tryHook(); },
                         Qt::QueuedConnection);
    }
}

bool Client::attachVideoItem(const RenderObject key, QQuickItem *host)
{
    if (!host) return false;

    auto &s = m_d3d11Slots[key];
    if (!s.item) createVideoItem(key, host);

    // ScreenCapture
    if (key == RenderObject::ScreenCaptureRender) {
        m_screenCap->setVideoItem(s.item);
        m_screenCap->setOutputIndex(0);
        m_screenCap->setFps(60);
        m_screenCap->setDrawMouse(true);
    }

    if (s.host == host) {
        syncVideoItemSize(key);
        return true;
    }

    s.item->setParentItem(host);
    // s.item->setParent(host);
    s.item->setVisible(true);

    s.item->setSize(host->size());
    s.item->setPosition({0, 0});
    s.host = host;
    syncVideoItemSize(key);

    if (s.cW) QObject::disconnect(s.cW);
    if (s.cH) QObject::disconnect(s.cH);
    s.cW = QObject::connect(host, &QQuickItem::widthChanged, this, [this, key]{
        this->scheduleSyncVideoItemSize(key);
    });
    s.cH = QObject::connect(host, &QQuickItem::heightChanged, this, [this, key]{
        this->scheduleSyncVideoItemSize(key);
    });

    return true;
}



void Client::setD3D11Device(ID3D11Device *dev)
{
    if (!mp_) return;
    mp_->oran7mp_setD3D11Device(dev);
}

void Client::getTotalDuration()
{
    if(mp_)
    {
        total_duration_=mp_->oran7mp_get_duration();
        emit updataQmlPlayNowFileAllTime(total_duration_/1000);
    }
}

void Client::reqUpdateCurrentPosition()
{
    if(mp_&&!req_seeking_)
    {
        Current_SecPosition_=mp_->oran7mp_get_current_position(); //单位秒

        if(Current_SecPosition_ * 1000 > total_duration_ || Current_SecPosition_ < 0)return;

        int Slider_posValue=MAX_SLIDER_VALUE * Current_SecPosition_  * 1.0 / (total_duration_ * 1.0 /1000);

        if(Slider_posValue > MAX_SLIDER_VALUE || Slider_posValue <0)return;

        emit updataQmlPlayProgressSliderCurPos(Slider_posValue,Current_SecPosition_);//发送至qml
    }
}


int Client::OutputVideo(const Frame* frame, AVFrame* copy_frame)
{
    if (!copy_frame) {
        WARNING_LOG<<"Client of OutputVideo - copy_frame is nullptr.";
        return -1;
    }

    if(shutting_down_.load(std::memory_order_relaxed) == true)
    {
        WARNING_LOG<<"shutting_down_ is true.";
        //av_frame_free(&copy_frame);
        return 0;
    }

    //std::unique_ptr<AVFrame, void(*)(AVFrame*)> g(copy_frame, [](AVFrame* f){ av_frame_free(&f); });//--->Discard 2026/1/27
    QPointer<Client> self(this);
    if (!self) return 0;
    auto &s = m_d3d11Slots[RenderObject::VideoPlayerRender];

    // if (!self->m_textureProvider) return 0;//--->Discard 2026/1/27
    // self->processVideoFrame(frame->frame);//--->Discard 2026/1/27

    //New --->2026/1/27   Used by D3D11VideoItem : public QQuickItem
    //copy_frame的释放权交给渲染侧D3D11VideoItem
    if (!s.item) { av_frame_free(&copy_frame); return 0; }
    AVFrame* safe = av_frame_clone(copy_frame); // clone 会 ref 内部 buffer
    av_frame_free(&copy_frame);
    s.item->submitFrame(safe); // submitFrame 接管所有权-->由 item 释放

    return 0;
}

void Client::syncVideoItemSize(const RenderObject &key)
{
    auto &s =m_d3d11Slots[key];
    if (!s.item || !s.host) return;

    const QSizeF hostSize(s.host->width(), s.host->height());
    if (hostSize.isEmpty()) return;

    if (s.srcSize.isEmpty()) {
        //WARNING_LOG << "sync key="<<int(key)<<" srcSize EMPTY -> force fill host";
        s.item->setX(0);
        s.item->setY(0);
        s.item->setWidth(hostSize.width());
        s.item->setHeight(hostSize.height());
        return;
    }

    const bool fill = (s.scaleMode == ScaleMode::Fill);
    qreal dpr = 1.0;
    if (s.host && s.host->window())
        dpr = s.host->window()->devicePixelRatio();
    QRectF r = calcAspectRectSnapped(QSizeF(s.srcSize), hostSize, fill, dpr);

    s.item->setX(r.x());
    s.item->setY(r.y());
    s.item->setWidth(r.width());
    s.item->setHeight(r.height());
}

void Client::scheduleSyncVideoItemSize(const RenderObject &key)
{
    auto &s = m_d3d11Slots[key];
    if (s.syncPending) return;
    s.syncPending = true;
    QMetaObject::invokeMethod(this, [this,key]{
        auto &s = m_d3d11Slots[key];
        s.syncPending = false;
        syncVideoItemSize(key);
    }, Qt::QueuedConnection);
}


//===================================================================

void Client::addNewLocalMusic(const QVariantList &fileList)
{
    QMutexLocker locker(&client_mutex);

    for(const QVariant& filePath : std::as_const(fileList))
    {
        QFileInfo fileInfo(filePath.toString().replace("file:///",""));
        QString destFile = appData.audioDirPath + "/" + fileInfo.fileName();
        if(appData.localMusic_fileSet.contains(destFile))
        {
            CLIENT_LOG<<"addNewLocalMusic:Already has file:"<<fileInfo.absoluteFilePath();
            continue;
        }

        //<---这里可以结合ffprobe深度分析文件是否重复

        //拷贝文件到应用程序存储本地文件目录
        QFile::copy(fileInfo.absoluteFilePath(), destFile);
        CLIENT_LOG<<"addNewLocalMusic:Do copy for "<<fileInfo.absoluteFilePath() << " to "<<appData.audioDirPath;
    }
    //刷新
    refreshLocalMusicList();
}

QVariantList& Client::getCustom_LocalMusic_playOrder()
{
    QMutexLocker locker(&client_mutex);
    return appData.Custom_LocalMusic_playOrder;
}

QSet<QString>& Client::getLocalMusic_fileSet()
{
    QMutexLocker locker(&client_mutex);
    return appData.localMusic_fileSet;
}

QList<QFileInfo>& Client::getLocalMusic_fileList()
{
    QMutexLocker locker(&client_mutex);
    return this->appData.localMusic_fileList;
}

void Client::refreshLocalMusicList()
{
    emit flushClear_LocalMusicStackList();

    CLIENT_LOG<<"refreshLocalMusicList...";
    //重遍历列表
    ApplicationContext::instance()->asyncWorker()->startSearchLocalMediaFiles_Task(this->appData.audioDirPath);
}

void Client::loadConfig_localMusicList_playOrder()
{
    QMutexLocker locker(&client_mutex);
    this->appData.Custom_LocalMusic_playOrder=AppConfigManager::instance().getLocalMusicList_palyOrder();
}

void Client::loadConfig_AppWindowSize(const QQmlApplicationEngine &engine)
{
    int AppWindow_width = AppConfigManager::instance().getValueQVariant("window.width").toInt();
    int AppWindow_height = AppConfigManager::instance().getValueQVariant("window.height").toInt();

    auto rootObjects = engine.rootObjects();
    if(rootObjects.isEmpty())return;

    QObject* AppWindow = rootObjects.first();
    if(AppWindow)
    {
        AppWindow->setProperty("width",AppWindow_width);
        AppWindow->setProperty("height",AppWindow_height);
    }

    CONFIG_LOG<<"LoadConfig of set AppWindow_width"<<AppWindow_width<<"; AppWindow_height"<<AppWindow_height;
}

void Client::loadConfig_AppWindowPosition(const QQmlApplicationEngine &engine)
{
    int AppWindow_PosX = AppConfigManager::instance().getValueQVariant("window.position.x").toInt();
    int AppWindow_PosY = AppConfigManager::instance().getValueQVariant("window.position.y").toInt();

    auto rootObjects = engine.rootObjects();
    if(rootObjects.isEmpty())return;

    QObject* AppWindow = rootObjects.first();
    if(AppWindow)
    {
        AppWindow->setProperty("x",AppWindow_PosX);
        AppWindow->setProperty("y",AppWindow_PosY);
    }
}

void Client::loadConfig_AppSetPlayerVolume()
{
    int value = AppConfigManager::instance().getValueQVariant("playerVolume").toInt();
    this->reqChangeVolumeValue(value);
    if(value <0 || value >100)return;
    emit this->configSignal_loadPlayerVolumeConfig(value);
}

QList<QFileInfo> Client::sortFileInfoList_byFilePaths(const QList<QFileInfo> &fileInfoList, const QVariantList &orderPaths)
{
    bool qVariantList_orderPaths_isEmpty=false;
    if(orderPaths.isEmpty())
    {
        CONFIG_LOG<<"in sortFileInfoList_byFilePaths : Config of orderPaths is Empty";
        qVariantList_orderPaths_isEmpty=true;
    }

    //1.copy QVariantList副本
    QStringList order=QStringList();
    if(qVariantList_orderPaths_isEmpty==false)
        for(const QVariant& op : std::as_const(orderPaths))
        {
            QString path=op.toString();
            if(!path.isEmpty())
                order.append(path);
        }

    //2.创建路径到索引的映射
    QHash<QString,int> orderIndex=QHash<QString,int>();
    if(qVariantList_orderPaths_isEmpty==false)
        for(int i=0;i<order.size();++i)
        {
            QString normalizedPath = QDir::fromNativeSeparators(order[i]);
            orderIndex[normalizedPath]=i;
        }

    //3.copy QList<QFileInfo>副本 for sort
    QList<QFileInfo> sortedList = fileInfoList;

    //4.std::sort
    std::sort(sortedList.begin(),sortedList.end(),[&orderIndex](const QFileInfo& a,const QFileInfo&b){
        QString pathA=QDir::fromNativeSeparators(a.absoluteFilePath());
        QString pathB =QDir::fromNativeSeparators(b.absoluteFilePath());

        int indexA=orderIndex.value(pathA,-1);
        int indexB = orderIndex.value(pathB,-1);

        if(indexA>=0 && indexB>=0)
            return indexA < indexB;//normal
        else if(indexA >=0 && indexB<0)
            return false;//no exist put forward
        else if(indexA <0 && indexB >=0)
            return true;//no exist put forward
        else{
            return a.birthTime() > b.birthTime();//default sort_type:birthTime
        }
    });

    return sortedList;
}

void Client::saveConfig_localMusicList_playOrder()
{
    QMutexLocker locker(&client_mutex);
    if(appData.Custom_LocalMusic_playOrder.isEmpty())
    {
        if(appData.localMusic_fileList.isEmpty())return;

        for(const QFileInfo& fl : std::as_const(appData.localMusic_fileList))
        {
            appData.Custom_LocalMusic_playOrder.append(QVariant(fl.absoluteFilePath()));
        }
    }
    AppConfigManager::instance().saveLocalMusicList_playOrder(appData.Custom_LocalMusic_playOrder);
}

void Client::saveConfig_AppWindowSize(const QQmlApplicationEngine &engine)
{
    auto rootObjects = engine.rootObjects();
    if (rootObjects.isEmpty()) return;

    QObject* AppWindow = rootObjects.first();

    int width = AppWindow->property("savedNormalWidth").toInt();
    int height = AppWindow->property("savedNormalHeight").toInt();


    if (width > 0 && height > 0)
    {
        AppConfigManager::instance().setValueQVariant("window.width", width);
        AppConfigManager::instance().setValueQVariant("window.height", height);
    }
}

void Client::saveConfig_AppWindowPosition(const QQmlApplicationEngine &engine)
{
    auto rootObjects = engine.rootObjects();
    if(rootObjects.isEmpty())return;

    QObject* AppWindow = rootObjects.first();

    int posX = AppWindow->property("savedNormalX").toInt();
    int posY = AppWindow->property("savedNormalY").toInt();

    if(posX >= 0 && posY >=0)
    {
        AppConfigManager::instance().setValueQVariant("window.position.x",posX);
        AppConfigManager::instance().setValueQVariant("window.position.y",posY);
    }
}

void Client::saveConfig_AppSetPlayerVolume()
{
    AppConfigManager::instance().setValueQVariant("playerVolume",appData.playerVolume);
}






