#include "Oran7MediaClient.h"

#include "ffmsg.h"
#include "GlobalHelper.h"
#include "AppJsonConfigManager.h"
#include "D3d11DeviceProvider.h"
#include "Oran7MusicSearchService.h"
#include "Oran7Async.h"
#include "NcmSession.h"
#include "NcmSongApi.h"

#include <thread>
#include <functional>
#include <utility>

#include <QFile>
#include <QImage>
#include <QRegularExpression>
#include <QByteArray>
#include <QSqlError>
#include <QList>
#include <QPair>
#include <QCollator>

#include <QStandardPaths>
#include <QDirIterator>
#include <QSet>
#include <QDebug>
#include <algorithm>
#include <QFileInfo>
#include <QDateTime>
#include <QPair>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QQuickItem>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
// 本地音乐扫描的单项结果（worker 线程收集，GUI 线程发信号用）
struct LocalMediaEntry {
    QString cover;
    QString name;
    QString artist;
    QString album;
    QString timesize;
    QString filePath;
};
} // namespace

Client::Client(const QString &host,quint16 port,QObject *parent)
    : QObject{parent},
    Client_socket(new QTcpSocket(this)),
    m_destroyedFlag(std::make_shared<std::atomic<bool>>(false))
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

    //启动防ui频繁请求触发工具QElapsedTimer  ，已在[Client::requestPlayMusic:]中使用
    m_callTimer.start();

    //Oran7ScreenCaptureController
    // if (!m_screenCap) m_screenCap = new Oran7ScreenCaptureController(this);
}

Client::~Client()
{
    // 通知后台异步任务（搜索/删除）立即退出，避免 worker 访问已析构对象
    m_destroyedFlag->store(true);

    INFO_LOG << "[DTOR] Client::~Client() called";
    INFO_LOG << "[DTOR]   mp_:" << mp_;
    INFO_LOG << "[DTOR]   Progress_SliderPos_ReqUpdateTimer:" << Progress_SliderPos_ReqUpdateTimer;

    if(localdb.isOpen())
        localdb.close();
    if(mp_!=nullptr)//确保释放Oran7MediaPlayer内层
    {
        INFO_LOG << "[DTOR]   Destroying mp_";
        mp_->oran7mp_destroy();
        delete mp_;
        mp_ = nullptr;
        INFO_LOG << "[DTOR]   mp_ destroyed";
    }
    //释放定时器
    Progress_SliderPos_ReqUpdateTimer->stop();
    delete Progress_SliderPos_ReqUpdateTimer;
    INFO_LOG << "[DTOR]   Progress_SliderPos_ReqUpdateTimer deleted";

    for (auto& conn : connections) {
        QObject::disconnect(conn);
    }
    INFO_LOG << "[DTOR] Client::~Client() finished";
}

int Client::InitSignalsAndSlots()
{
    QPointer<Client> self(this);

    // 网络歌播放地址解析结果（VIP 重新拉取；一次性，m_pendingNetworkSongId 区分是否播放触发）
    connections << connect(NcmSession::instance().songApi(), &NcmSongApi::urlsResolved,
        this, &Client::onNetworkUrlResolved, Qt::UniqueConnection);

    //============[OnlineLyricsFetcher]============
    m_lyricsFetcher = new OnlineLyricsFetcher(this);
    connections << connect(m_lyricsFetcher, &OnlineLyricsFetcher::lyricsFound,
        this, [this](const QString &lrcFilePath) {
            // 防止过期结果：检查下载的歌词是否属于当前正在播放的歌曲
            const QString cur = appData.CurMediaFilePath;
            if (isNetworkUrl(cur)) {
                // 网络资源：缓存文件名是 song_<id>.lrc，按歌曲 id 比对，并记录待销毁路径
                if (extractSongId(lrcFilePath) == extractSongId(cur)) {
                    m_networkLyricsCachePath = lrcFilePath;
                    INFO_LOG << "Client: online lyrics found(network) ->" << lrcFilePath;
                    emit lyricsAvailable(lrcFilePath);
                }
            } else {
                QFileInfo lrcInfo(lrcFilePath);
                QFileInfo curInfo(cur);
                if (lrcInfo.completeBaseName() == curInfo.completeBaseName()) {
                    INFO_LOG << "Client: online lyrics found ->" << lrcFilePath;
                    emit lyricsAvailable(lrcFilePath);
                }
            }
        }, Qt::QueuedConnection);
    connections << connect(m_lyricsFetcher, &OnlineLyricsFetcher::lyricsNotFound,
        this, [this]() {
            emit onlineLyricsNotFound();
        }, Qt::QueuedConnection);

    //============[Oran7MediaPlayer]============
    connections << connect(this,&Client::playOrPauseRequested,this,[=](){
        CLIENT_LOG<<"playOrPauseRequested Reveived";
        this->onPlayOrPause();
        },Qt::QueuedConnection);
    connections << connect(this,&Client::stopRequested,this,[=](){
        this->onStop();
        },Qt::QueuedConnection);
    connections << connect(this->Progress_SliderPos_ReqUpdateTimer,&QTimer::timeout,this,[=](){
        reqUpdateCurrentPosition();
        },Qt::QueuedConnection);

    this->Progress_SliderPos_ReqUpdateTimer->start(100);//启动触发前端播放进度条更新的定时器,间隔10ms

    connections << connect(this,&Client::localMusicListReordered,this,[self](const QVariantList reOrdered_list){
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
            QMutexLocker mpLocker(&m_mpMutex);
            if (mp_) {
                setD3D11Device(m_qtDevice.Get());
            }
            //Set Oran7ScreenCaptureController D3D11Device
        },Qt::QueuedConnection);
    //Oran7ScreenCapture error info
    connections << connect(m_screenCap,&Oran7ScreenCaptureController::errorOccurred,
        this,[this](const QString &err){
            ERROR_LOG<<err;
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
    this->appData.audioDirPath = GlobalHelper::getConfigDir() + "/audioTemp";
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
    //audioLRC存储歌词子目录
    this->appData.audioLRCDirPath = appData.audioDirPath + "/audioLRC";
    QDir audioLRCDir(appData.audioLRCDirPath);
    if(!audioLRCDir.exists())
    {
        if(!audioLRCDir.mkpath("."))
        {
            WARNING_LOG << "Client::createAppDirectories:Could not Create request Path:" << appData.audioLRCDirPath;
            return QString();
        }
    }
    return QDir(appData.audioDirPath).absolutePath();
}

void Client::saveConfig_lastCloseAppFocusedMusic()
{
    QMutexLocker loceker(&client_mutex);
    if(this->appData.audioAbsoluteFilePath.isEmpty())return;
    AppConfigManager::ins().setLastUsedFile(this->appData.audioAbsoluteFilePath, m_currentIsVip);
}

void Client::loadConfig_lastCloseAppFocusedMusic()
{
    AppConfigManager &cfg = AppConfigManager::ins();
    const qint64 lastSongId = cfg.getValueQVariant("last_media_songid").toLongLong();
    QString lastFilePath = cfg.getLastUsedFile();

    // 上一首是网络歌（last_media_songid>0）：直接用持久化元数据恢复。
    // 中途退出/URL 未解析成功时 last_used_file 可能过期或仍指向旧歌，
    // 若它不是网络 URL 则置空，播放时按 songId 重新解析，避免被误当本地歌播放。
    // （lastSongId==0 但 last_used_file 是网络 URL：兼容旧版本只存了 URL 的配置）
    if (lastSongId > 0 || isNetworkUrl(lastFilePath))
    {
        if (!isNetworkUrl(lastFilePath))
            lastFilePath.clear();
        const QString name     = cfg.getValueQVariant("last_media_name").toString();
        const QString artist   = cfg.getValueQVariant("last_media_artist").toString();
        const QString album    = cfg.getValueQVariant("last_media_album").toString();
        const QString cover    = cfg.getValueQVariant("last_media_cover").toString();
        const QString timesize = cfg.getValueQVariant("last_media_timesize").toString();
        m_currentSongId        = lastSongId;
        m_currentIsVip         = cfg.getValueQVariant("last_media_isvip").toBool();

        emit focusedMusicRestored(cover, name, artist, album, timesize,
                                  QString::number(lastSongId), lastFilePath);
        // 持久化元数据缺专辑/时长时,按 songId 重新拉取并补发(如"我喜欢的音乐"部分歌 API 无专辑/时长字段)
        if (album.isEmpty() || timesize.isEmpty())
            refetchLastMediaDetail();
        return;
    }
    if (lastFilePath.isEmpty())
        return;
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

    emit focusedMusicRestored(
        encodedPath,                              //cover Path
        mediaInfo["title"].toString(),         //music_name
        mediaInfo["artist"].toString(),       //music_artist
        mediaInfo["album"].toString(),     //music_album
        mediaInfo["duration"].toString(),   //music_duration
        QString::number(-1),                    //music_id
        lastFilePath);                                //music_localPath
}

// 持久化网络歌元数据缺专辑/时长时,按 songId 重新拉取并补发 focusedMusicRestored
void Client::refetchLastMediaDetail()
{
    const qint64 id = m_currentSongId;
    if (id <= 0)
        return;
    if (!m_metaNam)
        m_metaNam = new QNetworkAccessManager(this);

    QUrl url(QStringLiteral("https://music.163.com/api/song/detail"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"), QStringLiteral("[%1]").arg(id));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Oran7MediaClient/1.0"));
    request.setRawHeader("Referer", "https://music.163.com");
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_metaNam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray songs = doc.object().value(QStringLiteral("songs")).toArray();
        for (const QJsonValue &val : songs) {
            const QJsonObject song = val.toObject();
            if (song.value(QStringLiteral("id")).toVariant().toLongLong() != id)
                continue;

            const QString album = song.value(QStringLiteral("album")).toObject()
                                      .value(QStringLiteral("name")).toString();
            const qint64 ms = song.value(QStringLiteral("duration")).toVariant().toLongLong();
            const QString timesize = QString::number(static_cast<int>(ms / 1000));
            if (album.isEmpty() && timesize.isEmpty())
                return;   // 仍然拿不到有效数据,放弃

            // 把补全的真实元数据写回持久化配置,下次启动直接生效,无需再重拉
            AppConfigManager::ins().setValueQVariant("last_media_album", album);
            AppConfigManager::ins().setValueQVariant("last_media_timesize", timesize);
            AppConfigManager::ins().saveConfig();

            const QString cover = AppConfigManager::ins().getValueQVariant("last_media_cover").toString();
            const QString name  = AppConfigManager::ins().getValueQVariant("last_media_name").toString();
            const QString artist= AppConfigManager::ins().getValueQVariant("last_media_artist").toString();
            // last_used_file 若过期/非网络 URL，置空让播放时按 songId 重新解析（与 load 逻辑保持一致）
            QString lastFilePath = AppConfigManager::ins().getLastUsedFile();
            if (!isNetworkUrl(lastFilePath))
                lastFilePath.clear();
            emit focusedMusicRestored(cover, name, artist, album, timesize,
                                      QString::number(id), lastFilePath);
            return;
        }
    });
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

    // 网络 URL：封面/时长/作者/专辑/歌名无法用 ffprobe 解析（只能通过网络/搜索结果获取）。
    // 直接返回空，由 QML 侧提供真实元数据；无网络时保持空。避免 ffprobe 把 URL 当本地文件报错。
    if (isNetworkUrl(filepath))
        return mediaInfo;

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
    // 网络 URL（网易云播放链接）保留原样传给 ffprobe（ffprobe 支持 http 输入）；
    // QFileInfo.absoluteFilePath() 会把 http:// 破坏成 http:/ 导致找不到文件
    QString absFilePath = isNetworkUrl(filepath)
                        ? filepath
                        : QFileInfo(filepath).absoluteFilePath();
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

        mediaInfo["success"] = 1;
        return mediaInfo;
    }

    if (ffprobe.exitCode() != 0)
    {
        WARNING_LOG << "Client::analyzeMediaFileInfo:FFprobe ERROR:" << ffprobe.readAllStandardError();
        mediaInfo["success"] = 2;
        return mediaInfo;
    }
    //读取ffprobe输出的Json信息
    QByteArray output = ffprobe.readAllStandardOutput();
    if (output.isEmpty())
    {
        WARNING_LOG << "Client::analyzeMediaFileInfo:FFprobe returned empty output for file:" << fileInfo.absoluteFilePath();
        mediaInfo["success"] = 3;
        return mediaInfo;
    }
    //解析Json信息
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(output,&parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        WARNING_LOG << "Client::analyzeMediaFileInfo:JSON parse error:" << parseError.errorString();
        mediaInfo["success"] = 4;
        return mediaInfo;
    }
    QJsonObject root = doc.object();

    /*调用整理好的模块化提取函数*/
    mediaInfo = extractMediaInfo(root);

    if(mediaInfo["duration"].toInt() <=0)
    {
        //媒体时长信息提取失败......
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
        // 附加图片可能是 png/jpg/bmp；当前 ffmpeg 构建缺 png 解码器（"Oran7FFmpegDecoderCore (codec png) not found"），
        // 因此用 -c:v copy 直接把附加图流拷出来（不解码），再用 Qt 解码并转成 jpg。
        QString rawCoverPath = fileInfo.absolutePath() + "/audioCover/" +
                               fileInfo.completeBaseName() + ".raw";

        QStringList arguments02;
        arguments02 << "-y"
                    << "-i" << absFilePath
                    << "-map" << "0:v?"
                    << "-frames:v" << "1"
                    << "-c:v" << "copy"
                    << "-f" << "image2"
                    << rawCoverPath;
        //ffmpeg.exec()
        QProcess ffmpeg;
        ffmpeg.start(ffmpegPath, arguments02);
        if (!ffmpeg.waitForFinished(30000))
        {
            WARNING_LOG << "Client::analyzeMediaFileInfo:Getting audioCover time out:" << absFilePath;
            ffmpeg.kill();
            QFile::remove(rawCoverPath);
            mediaInfo["success"] = 6;
            return mediaInfo;
        }
        if (ffmpeg.exitCode() != 0)
        {
            WARNING_LOG << "Client::analyzeMediaFileInfo:ffmpeg exit ERROR of" << absFilePath;
            WARNING_LOG << "Client::analyzeMediaFileInfo:ffmpeg exit ERROR:" << ffmpeg.readAllStandardError();
            QFile::remove(rawCoverPath);
            mediaInfo["success"] = 7;
            return mediaInfo;
        }
        // 用 Qt 解码原始封面（自动识别 png/jpg/bmp），转成 jpg
        QFile rawFile(rawCoverPath);
        if (rawFile.open(QIODevice::ReadOnly))
        {
            const QByteArray rawData = rawFile.readAll();
            rawFile.close();
            QImage coverImg = QImage::fromData(rawData);
            if (!coverImg.isNull())
                coverImg.save(coverPath, "JPG");
            else
                WARNING_LOG << "Client::analyzeMediaFileInfo:Qt decode cover failed:" << rawCoverPath;
        }
        QFile::remove(rawCoverPath);

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

void Client::requestPlayMusic(QString file_path)
{
    if(file_path.isEmpty())return;
    appData.audioAbsoluteFilePath = file_path;
    // 开播即持久化 last_used_file：网络歌 URL 解析成功才开始播放，
    // 若一直等到关闭才写，中途退出或解析未完成时 last_used_file 会停留在上一首，与 last_media_* 错配
    AppConfigManager &cfg = AppConfigManager::ins();
    if (isNetworkUrl(file_path)) {
        cfg.setLastUsedFile(file_path, m_currentIsVip);
    } else {
        // 本地歌：清掉上一首网络歌残留的 songid，否则 last_media_songid>0 会误导
        // 启动恢复逻辑把本地歌当网络歌；再落盘 last_used_file
        cfg.setValueQVariant("last_media_songid", 0);
        cfg.setLastUsedFile(file_path, false);
        cfg.saveConfig(); // 值未变化时 setLastUsedFile 不会触发保存，这里兜底确保 songid=0 落盘
    }
    this->preparePlayingMedia(file_path);
}

void Client::requestPlayVideo(QString file_path)
{
    if(file_path.isEmpty())return;
    appData.videoAbsoluteFilePath = file_path;
    this->preparePlayingMedia(file_path);
}

void Client::preparePlayingMedia(QString file_path)
{
    QMutexLocker locker(&client_mutex);

    if(file_path.isEmpty())return;

    // 新的一次播放请求：先销毁上一首网络歌曲残留的歌词缓存（本地歌曲不受影响）
    cleanupNetworkLyricsCache();

    INFO_LOG<<"REQ For Prepare Play:"<<file_path;
    if(!appData.CurMediaFilePath.isEmpty())
    {
        if(appData.CurMediaFilePath!=file_path)
        {
            QMutexLocker mpLocker(&m_mpMutex);
            if(mp_)
            {
                //播放中--->切换播放文件，防频繁点击事件
                if (m_callTimer.elapsed() < CALL_INTERVAL_MS)//100ms
                {
                    INFO_LOG << "Client::requestPlayMusic:The call is too frequent and the request has been ignored";
                    return;
                }
                m_callTimer.restart();
                appData.CurMediaFilePath=file_path;
                tryLoadLrcForFile(file_path);
                emit this->stopRequested();
                // 等待旧播放器后台销毁（Oran7Async，实测网络流 60~100ms）完成后再创建新播放器，
                // 间隔拉长避免新旧播放器 SDL 音频设备并发打开/关闭导致崩溃。
                // 防重入：300ms 延迟回调已在排队时，直接更新 CurMediaFilePath 并复用首次回调
                //（回调用最新的 CurMediaFilePath 创建播放器），避免多次连续切歌累积多个
                // singleShot，导致反复创建/暂停/播放、音频设备并发开闭。
                if (!m_pendingSwitch)
                {
                    m_pendingSwitch = true;
                    QTimer::singleShot(300, this,[this](){
                        m_pendingSwitch = false;
                        emit this->playOrPauseRequested();
                    });
                }
                return;
            }
            else
            {
                //到这里指：上次播放的媒体已经结束，并请求播放下一首和上次不同的文件
                appData.CurMediaFilePath=file_path;
                tryLoadLrcForFile(file_path);
                emit this->playOrPauseRequested();
                return;
            }
        }
        else
        {
            //上一次播放不为空，而且这次请求的路径也是一样的，那就直接暂停
            emit this->playOrPauseRequested();
            return;
        }
    }
    else
    {
        //上一次播放为空，直接复制路径然后触发播放
        appData.CurMediaFilePath=file_path;
        tryLoadLrcForFile(file_path);
        emit this->playOrPauseRequested();
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
    responseFromServer.append(socket->readAll());

    CLIENT_LOG<<"[CLIENT_BUFFER:]responseFromServer:"<<responseFromServer.toHex(' ');
    while(responseFromServer.size()>=5)
    {
        QDataStream stream(responseFromServer);
        stream.setByteOrder(QDataStream::BigEndian); // 根据协议设置字节序
        quint32 totalLength;
        stream >> totalLength;

        if(responseFromServer.size()<totalLength)
        {
            //数据传输是分块发送的，如果缓冲区数据不完整，继续读取
            CLIENT_LOG<<"respons not enought.Expected:"<<totalLength<<" ,Actual:"<<responseFromServer.size();
            CLIENT_LOG<<"CLIENT_BUFFER:responseFromServer:"<<responseFromServer.toHex(' ');
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
        // qDebug()<<"[CLIENT_BUFFER:]responseFromServer:"<<responseFromServer;

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
        emit integratedMusicElementReady(icon,list_musicList[i][0],list_musicList[i][1],list_musicList[i][2],list_musicList[i][3],list_musicList[i][4]);
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
            // 快速切歌竞态：旧播放器可能在销毁前发出 PLAY_FNISH 并已入队，
            // 若 mp 已不是当前 mp_（切歌后 mp_ 已指向新播放器/置空），
            // 说明是旧播放器的迟到信号，忽略，避免误清 isPlaying 导致按钮状态错误
            {
                QMutexLocker mpLocker(&m_mpMutex);
                if (mp != mp_)
                    break;
            }
            /*数据播放完毕，停止播放*/
            emit this->stopIconUpdated();//更新qml前端暂停图标为停止
            // 注：单曲循环不能在此 seek 回 0——EOF 后 ffplay 的解码线程已退出，seek 无法恢复播放。
            // 单曲循环改由 QML 的 onPlayNextTriggered 按 playMode==1 重新播放当前歌（见 SearchMusicStack/LocalMusicStack）。
            emit this->stopRequested();//停止并销毁Oran7MediaPlayer，以及其中内层的所有子线程，在安静时清空播放器运行资源
            shutting_down_ = false;//主动播放结束-->重置不拦截video frame
            /*触发自动播放下一首*/
            this->playNext();
            break;
        case FFP_MSG_FIND_STREAM_INFO:
            CLIENT_LOG << __FUNCTION__ << "FFP_MSG_FIND_STREAM_INFO";
            /*获取当前媒体文件总播放时长*/
            getTotalDuration(mp);
            break;
        case FFP_MSG_SEEK_COMPLETE:
            req_seeking_ = false;
            break;
        case FFP_MSG_ERROR:
            CLIENT_LOG << __FUNCTION__ << " FFP_MSG_ERROR " << msg.arg1;
            // 起播打开失败（read_thread 抛异常后由 Oran7FFplayerCore 补发的通知）。
            // 旧播放器的迟到错误同样要忽略——切歌后 mp_ 已指向新播放器/置空。
            {
                QMutexLocker mpLocker(&m_mpMutex);
                if (mp != mp_)
                    break;
            }
            // 本函数跑在消息线程，而补救逻辑要读写 appData / m_songUrlCache（主线程数据），
            // 并且会走 requestPlayMusic（它会写配置文件）→ 必须弹回主线程再执行。
            QMetaObject::invokeMethod(this, [this]() { onPlaybackOpenFailed(); },
                                      Qt::QueuedConnection);
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

void Client::onPlayOrPause()
{
    INFO_LOG << "Client::onPlayOrPause:OnPlayOrPause signal called.";
    QElapsedTimer opT; opT.start();
    int ret = 0;
    QMutexLocker mpLocker(&m_mpMutex);
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
            INFO_LOG << "Client::onPlayOrPause:Oran7MediaPlayer create failed";
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
            INFO_LOG << "Client::onPlayOrPause:Oran7MediaPlayer create failed";
            delete mp_;
            mp_ = NULL;
            return;
        }
        INFO_LOG<<"Client::onPlayOrPause:Successed create and prepare async of Oran7MediaPlayer. ";
        /*设置默认保存的音量条*/
    }
    else
    {
        INFO_LOG<<"Client::onPlayOrPause:All Already _ User Clicked PlayOrPause. ";
        if(mp_->oran7mp_get_state() == MP_STATE_STARTED)
        {
            INFO_LOG<<"Client::onPlayOrPause:Will be paused. ";
            mp_->oran7mp_pause();
        }
        else if(mp_->oran7mp_get_state() == MP_STATE_PAUSED)
        {
            INFO_LOG<<"Client::onPlayOrPause:Will be started. ";
            mp_->oran7mp_start();
        }
    }
    INFO_LOG << "Client::onPlayOrPause:total cost" << opT.elapsed() << "ms";
}

void Client::onStop()
{
    INFO_LOG << "Client::onStop:OnStop signal called.";
    // 播放结束/停止时销毁当前网络歌曲的歌词缓存（本地歌曲不受影响）
    cleanupNetworkLyricsCache();

    // 把播放器销毁挪到后台线程（Oran7Async）：delete mp_ 会 join 读线程 + 关闭音频设备，
    // 实测网络流耗时 60~100ms，留在主线程会卡 UI（本地文件几乎瞬时，故本地不卡）
    Oran7MediaPlayer* old = nullptr;
    {
        QMutexLocker locker(&m_mpMutex);
        old = mp_;
        mp_ = nullptr;
    }
    if (old) {
        INFO_LOG << "Client::onStop:Destroying Oran7FFplayerCore via Oran7Async (background).";
        Oran7Async::run(this,
            [old]() {
                // 先干净地暂停音频输出（FFP_REQ_PAUSE 入队，给消息线程 ~30ms 处理），
                // 再销毁，避免快速切歌时旧播放器音频设备关闭前仍输出残留缓冲 → 末尾声音抽搐
                old->oran7mp_pause();
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                delete old;
            },
            []() {});
    }

    // 播放器销毁后,把进程堆上已释放但未归还的碎片页压缩回操作系统,
    // 缓解"听一段时间后内存缓慢涨到1GB"的堆碎片问题
#ifdef Q_OS_WIN
    HeapCompact(GetProcessHeap(), 0);
#endif
}

bool Client::isNetworkUrl(const QString &path)
{
    return path.startsWith(QStringLiteral("http://"))
        || path.startsWith(QStringLiteral("https://"));
}

qint64 Client::extractSongId(const QString &s)
{
    // 匹配 "id=<数字>"（outer/url?id=123.mp3）或文件名 "song_<数字>"（song_123.lrc）
    QRegularExpression re(QStringLiteral("(?:id=|song_)(\\d+)"));
    auto m = re.match(s);
    return m.hasMatch() ? m.captured(1).toLongLong() : -1;
}

void Client::cleanupNetworkLyricsCache()
{
    if (m_networkLyricsCachePath.isEmpty())
        return;
    if (QFile::remove(m_networkLyricsCachePath))
        INFO_LOG << "Client: removed network lyrics cache" << m_networkLyricsCachePath;
    else
        WARNING_LOG << "Client: failed to remove network lyrics cache" << m_networkLyricsCachePath;
    m_networkLyricsCachePath.clear();
}

int Client::seekTo(int cur_value)
{
    //curvalue  单位s
    INFO_LOG<<"Client::ProgressSlider_Seek:Client Request Progress Slider seet to:"<<cur_value;
    QMutexLocker mpLocker(&m_mpMutex);
    if(mp_)
    {
        req_seeking_=true;
        int64_t milliseconds = cur_value * 1000;
        mp_->oran7mp_seek_to(milliseconds);
    }
    else return -1;
    return 0;
}

void Client::setPlayMode(int mode)
{
    // 0=顺序播放 1=单曲循环 2=随机播放
    m_playMode = mode;
    INFO_LOG << "Client::setPlayMode:" << mode;
}

void Client::setVolume(int cur_volume)
{
    this->appData.playerVolume = cur_volume;
    DEFAULT_VOLUM_SIZE = SDL_MIX_MAXVOLUME * cur_volume * 1.0 / 100;
    QMutexLocker mpLocker(&m_mpMutex);
    if(mp_==nullptr)return;
    mp_->oran7mp_set_playback_volume(cur_volume);
}

void Client::createVideoItem(const RenderObject key,QQuickItem *host)
{
    if (!host) {
        WARNING_LOG << "createVideoItem: host is nullptr";
        return;
    }

    auto &s = m_d3d11Slots[key];
    if (!s.item) 
    {
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
                info.sourceTag = (key == RenderObject::VideoPlayerRender) ? "player" : "screen";
                auto map = Oran7VideoInfo_To_QVariantMap(info);
                // INFO_LOG << "emit videoRenderInfoUpdated sourceTag=" << info.sourceTag
                //          << " srcWidth=" << info.srcSize.width()
                //          << " fps=" << info.fps;
                emit videoRenderInfoUpdated(map);
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
    QMutexLocker mpLocker(&m_mpMutex);
    if (!mp_) return;
    mp_->oran7mp_setD3D11Device(dev);
}

void Client::getTotalDuration(Oran7MediaPlayer* p)
{
    // 用所属播放器，避免旧播放器销毁期间消息线程误读全局 mp_（可能已指向新播放器）
    if(p)
    {
        total_duration_=p->oran7mp_get_duration();
        emit totalDurationUpdated(total_duration_/1000);
    }
}

void Client::reqUpdateCurrentPosition()
{
    QMutexLocker mpLocker(&m_mpMutex);
    if(mp_&&!req_seeking_)
    {
        Current_SecPosition_=mp_->oran7mp_get_current_position_double(); //浮点秒

        if(Current_SecPosition_ * 1000 > total_duration_ || Current_SecPosition_ < 0)return;

        int Slider_posValue=MAX_SLIDER_VALUE * Current_SecPosition_  * 1.0 / (total_duration_ * 1.0 /1000);

        if(Slider_posValue > MAX_SLIDER_VALUE || Slider_posValue <0)return;

        emit playProgressUpdated(Slider_posValue,Current_SecPosition_);//发送至qml
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
        av_frame_free(&copy_frame);  // 释放帧，避免内存泄漏
        return 0;
    }

    //std::unique_ptr<AVFrame, void(*)(AVFrame*)> g(copy_frame, [](AVFrame* f){ av_frame_free(&f); });//--->Discard 2026/1/27
    QPointer<Client> self(this);
    if (!self) { av_frame_free(&copy_frame); return 0; }
    auto &s = m_d3d11Slots[RenderObject::VideoPlayerRender];

    // if (!self->m_textureProvider) return 0;//--->Discard 2026/1/27
    // self->processVideoFrame(frame->frame);//--->Discard 2026/1/27

    //New --->2026/1/27   Used by D3D11VideoItem : public QQuickItem
    //copy_frame的释放权交给渲染侧D3D11VideoItem
    if (!s.item) { av_frame_free(&copy_frame); return 0; }

    // 安全检查：使用QPointer保护QML对象访问，避免关机时访问已销毁的QQuickItem
    QPointer<QQuickItem> itemGuard(s.item);
    if (!itemGuard) { av_frame_free(&copy_frame); return 0; }

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

    // D3D11VideoItem 几何永远铺满 host（x=0, y=0, w=hostW, h=hostH）。
    // Fit/Fill 模式只影响 render node 内部的 quad 顶点（contentRect），
    // 不移动/缩放 item 本体。这样 ShaderEffectSource offscreen 捕获时
    // 坐标系始终和 host 一致，不会混入布局偏移。
    s.item->setX(0);
    s.item->setY(0);
    s.item->setWidth(hostSize.width());
    s.item->setHeight(hostSize.height());

    // 计算 Fit/Fill 后的真实视频绘制区域（item-local 坐标）
    QRectF contentRect;
    if (s.srcSize.isEmpty()) {
        contentRect = QRectF(0, 0, hostSize.width(), hostSize.height());
    } else {
        const bool fill = (s.scaleMode == ScaleMode::Fill);
        qreal dpr = 1.0;
        if (s.host && s.host->window())
            dpr = s.host->window()->devicePixelRatio();
        contentRect = calcAspectRectSnapped(QSizeF(s.srcSize), hostSize, fill, dpr);
    }

    // 传递给 D3D11VideoItem → QSGRenderNode，用于 quad 顶点定位
    s.item->setContentRect(contentRect);
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

void Client::deleteLocalMusicFiles(const QVariantList &fileList)
{
    if (fileList.isEmpty()) return;

    // 1. 同步检查：待删列表是否包含当前正在播放的文件
    QString curPath;
    {
        QMutexLocker locker(&client_mutex);
        curPath = appData.CurMediaFilePath;
    }
    bool needStop = false;
    for (const QVariant& fp : std::as_const(fileList)) {
        if (fp.toString() == curPath) { needStop = true; break; }
    }
    // 同步停止播放器，释放文件句柄
    if (needStop) onStop();

    // 2. 转成 QSet 加速查重
    QSet<QString> toDelete;
    for (const QVariant& fp : std::as_const(fileList))
        toDelete.insert(fp.toString());

    // 3. 异步删除（文件 I/O + 短锁更新共享数据），完成后全量刷新列表
    Oran7Async::run(this,
        // worker：后台线程删文件 + 短锁清理数据结构（显式返回 void，避免 QtConcurrent 推导警告）
        [this, toDelete, destroyed = m_destroyedFlag]() -> void {
            if (destroyed->load())   // 生命周期守卫：Client 已析构则提前退出，避免 UAF
                return;
            for (const QString& path : std::as_const(toDelete)) {
                QFileInfo fi(path);
                QString coverPath = fi.absolutePath() + "/audioCover/" + fi.completeBaseName() + ".jpg";
                if (QFile::exists(coverPath))
                    QFile::remove(coverPath);
                if (!QFile::moveToTrash(path))
                    WARNING_LOG << "deleteLocalMusicFiles: Failed to move to trash:" << path;
                else
                    INFO_LOG << "deleteLocalMusicFiles: Moved to trash:" << path;
            }
            QMutexLocker locker(&client_mutex);
            appData.localMusic_fileList.erase(
                std::remove_if(appData.localMusic_fileList.begin(), appData.localMusic_fileList.end(),
                    [&toDelete](const QFileInfo& fi) { return toDelete.contains(fi.absoluteFilePath()); }),
                appData.localMusic_fileList.end());
            for (const QString& path : std::as_const(toDelete))
                appData.localMusic_fileSet.remove(path);
            for (int i = appData.Custom_LocalMusic_playOrder.size() - 1; i >= 0; --i) {
                if (toDelete.contains(appData.Custom_LocalMusic_playOrder.at(i).toString()))
                    appData.Custom_LocalMusic_playOrder.removeAt(i);
            }
        },
        // done：GUI 线程刷新列表
        [this]() {
            refreshLocalMusicList();
        });
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
    CLIENT_LOG << "refreshLocalMusicList...";
    searchLocalMediaFiles(this->appData.audioDirPath);
}

void Client::searchLocalMediaFiles(const QString &folderPath)
{
    // GUI 线程：先清空列表，等新结果逐条进来
    emit localMusicListCleared();

    Oran7Async::run(this,
        // worker（后台）：扫描 + 分析，先收集到局部结果，短锁更新共享 container（不长时间持锁）
        [this, folderPath, destroyed = m_destroyedFlag]() -> QList<LocalMediaEntry> {
            QList<LocalMediaEntry> result;
            if (destroyed->load())   // 生命周期守卫：Client 已析构则提前退出，避免 UAF
                return result;
            if (folderPath.isEmpty())
                return result;

            QDirIterator it(folderPath, QDir::Files);
            while (it.hasNext()) {
                const QString filePath = it.next();
                QMap<QString, QVariant> mediaInfo = analyzeMediaFileInfo(filePath);
                if (mediaInfo[QStringLiteral("success")].toInt() != 0)
                    continue;

                LocalMediaEntry e;
                e.filePath = filePath;
                e.name = mediaInfo[QStringLiteral("title")].toString();
                e.artist = mediaInfo[QStringLiteral("artist")].toString();
                e.album = mediaInfo[QStringLiteral("album")].toString();
                e.timesize = mediaInfo[QStringLiteral("duration")].toString();
                QString coverPath = mediaInfo[QStringLiteral("cover")].toString();
                if (!coverPath.startsWith(QStringLiteral("file:///")))
                    coverPath = QStringLiteral("file:///") + QDir::toNativeSeparators(coverPath);
                e.cover = QUrl(coverPath).toString(QUrl::FullyEncoded);
                result.append(e);
            }

            // 短锁更新共享 container：去重 + 排序 + playOrder 同步
            {
                QMutexLocker locker(&client_mutex);
                QList<QFileInfo>& localFileList = appData.localMusic_fileList;
                QSet<QString>& localFileSet = appData.localMusic_fileSet;
                QVariantList& localPlayOrder = appData.Custom_LocalMusic_playOrder;

                for (const LocalMediaEntry& e : std::as_const(result)) {
                    QFileInfo fileInfo(e.filePath);
                    if (!localFileSet.contains(fileInfo.absoluteFilePath())) {
                        localFileList.append(fileInfo);
                        localFileSet.insert(fileInfo.absoluteFilePath());
                    }
                }
                QList<QFileInfo> sorted = sortFileInfoList_byFilePaths(localFileList, localPlayOrder);
                localFileList = sorted;
                if (localPlayOrder.size() != sorted.size()) {
                    localPlayOrder.clear();
                    for (const QFileInfo& fl : std::as_const(sorted))
                        localPlayOrder.append(fl.absoluteFilePath());
                }
            }
            return result;
        },
        // done（GUI）：逐条发信号喂 QML 模型
        [this](QList<LocalMediaEntry> result) {
            for (const LocalMediaEntry& e : std::as_const(result)) {
                emit localMusicElementReady(e.cover, e.name, e.artist, e.album, e.timesize,
                                           QStringLiteral("-1"), e.filePath);
            }
        });
}

void Client::updateFocusCurMediaFile(QString filepath)
{
    appData.CurMediaFilePath = filepath;
    // 网络 URL 保留原样（避免 QFileInfo 把 http:// 破坏成 http:/）
    appData.audioAbsoluteFilePath = isNetworkUrl(filepath)
                                  ? filepath
                                  : QFileInfo(filepath).absoluteFilePath();

    // 网络歌曲元数据无法用 ffprobe 解析，QML 端（dealClicked）已从搜索结果提供真实信息，
    // 这里不再 emit focusedMusicRestored 空信息覆盖 UI。
    if (isNetworkUrl(filepath))
        return;

    QMap<QString,QVariant> mediaInfo= this->analyzeMediaFileInfo(filepath);
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

    emit focusedMusicRestored(
        encodedPath,                              //cover Path
        mediaInfo["title"].toString(),         //music_name
        mediaInfo["artist"].toString(),       //music_artist
        mediaInfo["album"].toString(),     //music_album
        mediaInfo["duration"].toString(),   //music_duration
        QString::number(-1),                    //music_id
        filepath);                                //music_localPath
}

void Client::loadConfig_localMusicList_playOrder()
{
    QMutexLocker locker(&client_mutex);
    this->appData.Custom_LocalMusic_playOrder=AppConfigManager::ins().getLocalMusicList_palyOrder();
}

void Client::loadConfig_AppWindowSize(QQmlApplicationEngine &engine)
{
    int AppWindow_width = AppConfigManager::ins().getValueQVariant("window.width").toInt();
    int AppWindow_height = AppConfigManager::ins().getValueQVariant("window.height").toInt();

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

void Client::loadConfig_AppWindowPosition(QQmlApplicationEngine &engine)
{
    int AppWindow_PosX = AppConfigManager::ins().getValueQVariant("window.position.x").toInt();
    int AppWindow_PosY = AppConfigManager::ins().getValueQVariant("window.position.y").toInt();

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
    int value = AppConfigManager::ins().getValueQVariant("playerVolume").toInt();
    this->setVolume(value);
    if(value <0 || value >100)return;
    emit this->playerVolumeConfigLoaded(value);
}

QList<QFileInfo> Client::sortFileInfoList_byFilePaths(const QList<QFileInfo> &fileInfoList, const QVariantList &orderPaths)
{
    bool qVariantList_orderPaths_isEmpty=false;
    if(orderPaths.isEmpty())
    {
        CONFIG_LOG<<"in sortFileInfoList_byFilePaths : Config of orderPaths is Empty";
        qVariantList_orderPaths_isEmpty=true;
    }

    //copy QVariantList副本
    QStringList order=QStringList();
    if(qVariantList_orderPaths_isEmpty==false)
        for(const QVariant& op : std::as_const(orderPaths))
        {
            QString path=op.toString();
            if(!path.isEmpty())
                order.append(path);
        }

    //创建路径到索引的映射
    QHash<QString,int> orderIndex=QHash<QString,int>();
    if(qVariantList_orderPaths_isEmpty==false)
        for(int i=0;i<order.size();++i)
        {
            QString normalizedPath = QDir::fromNativeSeparators(order[i]);
            orderIndex[normalizedPath]=i;
        }

    //copy QList<QFileInfo>Impl for sort
    QList<QFileInfo> sortedList = fileInfoList;

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
    AppConfigManager::ins().saveLocalMusicList_playOrder(appData.Custom_LocalMusic_playOrder);
}

void Client::setCurrentMediaSongId(qint64 songId)
{
    m_currentSongId = songId;
}

void Client::setCurrentMediaIsVip(bool isVip)
{
    m_currentIsVip = isVip;
}

void Client::playNetworkSong(qint64 songId, const QString &fallbackUrl, bool isVip)
{
    INFO_LOG << "Client::playNetworkSong: songId=" << songId
             << " isVip=" << isVip << " fallback=" << fallbackUrl.left(60)
             << " isNet=" << isNetworkUrl(fallbackUrl);
    // 同一首歌再次请求（BottomPage 播放/暂停按钮、列表内点击当前歌）且播放器已存在：
    // 直接切换 暂停↔继续，避免重新解析 URL（fallback 外链 vs 缓存真实地址字符串不同）被
    // preparePlayingMedia 当作"切歌"而从头重播。
    // 判定"同一首"= songId 相同，或 请求的 URL 就是当前正在播的文件（兼容无 songId 的旧配置网络歌）。
    // EOF 后 mp_ 已销毁，单曲循环重播走下方正常解析路径，不受影响。
    bool toggleCurrent = false;
    {
        QMutexLocker mpLocker(&m_mpMutex);
        // 判定"同一首"不能比 m_currentSongId：QML 会在调用前 setCurrentMediaSongId 成目标歌，
        // 切歌时它恒等于请求的 songId，用它判断会把"切歌"误当"暂停"。改用播放器实际在播的文件：
        // 要么 请求的 URL 就是当前播放文件，要么 当前播放文件能提取出的 id == 请求 songId。
        toggleCurrent = (mp_ != nullptr)
            && ((!fallbackUrl.isEmpty() && fallbackUrl == appData.CurMediaFilePath)
                || (songId > 0 && extractSongId(appData.CurMediaFilePath) == songId));
    }
    if (toggleCurrent) {
        INFO_LOG << "Client::playNetworkSong: same song, toggle play/pause on current media";
        emit playOrPauseRequested();
        return;
    }
    // 有 songId（且是网络歌或 VIP 收藏未存 url）→ 始终用登录态重新解析真实播放地址（持久化临时链接会过期 403）
    if (songId > 0 && (isNetworkUrl(fallbackUrl) || fallbackUrl.isEmpty())) {
        // 同步当前 songId/isVip：解析出的真实 m701 地址不含 id，歌词搜索等依赖 m_currentSongId
        m_currentSongId = songId;
        m_currentIsVip = isVip;
        m_openRetryDone = false;   // 新的一首歌：重置起播失败补救的一次性标志
        // 20 分钟内解析过的同一首歌直接复用缓存 URL，避免重复解析触发网易 -110 限流
        const QString cached = freshCachedUrl(songId);
        if (!cached.isEmpty()) {
            INFO_LOG << "Client::playNetworkSong: reuse cached url for songId" << songId;
            requestPlayMusic(cached);
            return;
        }
        m_pendingNetworkSongId = songId;
        // 非 VIP：用可用的网络外链立即起播，消除"等解析完才播"的 ~1s 卡顿；
        // 真实 URL 解析放后台并行，仅更新缓存，不打断已开始的播放
        if (!isVip) {
            QString directUrl = fallbackUrl;
            // 已解析的真实 CDN 地址（mXXX.music.126.net）仅 ~20min 有效期，重启恢复/持久化后极易过期返回 403；
            // 起播前替换成稳定外链（music.163.com/song/media/outer/url）确保可播，新鲜地址由后台重新解析并缓存
            if (!isNetworkUrl(directUrl) || directUrl.contains(QStringLiteral("music.126.net")))
                directUrl = QStringLiteral("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(songId);
            INFO_LOG << "Client::playNetworkSong: non-VIP direct play" << directUrl.left(60);
            m_playStartedByFallback = true;
            requestPlayMusic(directUrl);
            NcmSession::instance().songApi()->resolveUrls({songId});
            return;
        }
        // VIP：必须等解析（外链拿不到会员音质）
        NcmSession::instance().songApi()->resolveUrls({songId});
        return;
    }
    if (isNetworkUrl(fallbackUrl)) {
        // 网络 URL 但无 songId（历史记录是 m701 真实 CDN 地址，不含 id）→ 尝试从 URL 提取
        qint64 id = extractSongId(fallbackUrl);
        if (id > 0) {
            m_currentSongId = id;
            m_currentIsVip = isVip;
            m_pendingNetworkSongId = id;
            m_openRetryDone = false;   // 新的一首歌：重置起播失败补救的一次性标志
            NcmSession::instance().songApi()->resolveUrls({id});
            return;
        }
        // 提取不到 songId：历史记录的真实 CDN 地址本身可播（20 分钟内有效），直接播放兜底；
        // 若已过期，avformat_open_input 会失败 → Oran7FFplayerCore 补发 FFP_MSG_ERROR
        // → Client::onPlaybackOpenFailed：此时 songId 为 0、无缓存可换，直接 emit
        //   networkPlayResolveFailed，由 QML 已有的自动切歌流程接管
        INFO_LOG << "Client::playNetworkSong: no songId, direct play fallback url";
        requestPlayMusic(fallbackUrl);
        return;
    }
    // 本地文件：直接播放
    INFO_LOG << "Client::playNetworkSong: local file, direct play";
    requestPlayMusic(fallbackUrl);
}

void Client::onNetworkUrlResolved(const QHash<qint64, QString>& urlMap)
{
    if (m_pendingNetworkSongId <= 0) {
        INFO_LOG << "Client::onNetworkUrlResolved: ignored (not play-triggered), pending=" << m_pendingNetworkSongId;
        return; // 不是播放触发的解析（如搜索结果解析），忽略
    }
    const qint64 id = m_pendingNetworkSongId;
    m_pendingNetworkSongId = 0; // 一次性消费

    const QString url = urlMap.value(id);
    INFO_LOG << "Client::onNetworkUrlResolved: id=" << id
             << " found=" << !url.isEmpty() << " mapSize=" << urlMap.size();
    if (url.isEmpty()) {
        // 已在播外链时，解析失败不打断当前播放（外链已在放）
        if (m_playStartedByFallback) {
            m_playStartedByFallback = false;
            INFO_LOG << "Client::onNetworkUrlResolved: fallback already playing, ignore resolve failure";
            return;
        }
        // 未登录/无会员/无权限/限流(-110)：延迟重试一次（网易对播放地址接口有频率风控，稍候可恢复）
        if (!m_retriedOnce) {
            m_retriedOnce = true;
            INFO_LOG << "Client::onNetworkUrlResolved: songId" << id << "resolve failed, retry once after 3s";
            QTimer::singleShot(3000, this, [this, id]() {
                m_pendingNetworkSongId = id;
                NcmSession::instance().songApi()->resolveUrls({id});
            });
            return;
        }
        m_retriedOnce = false;
        INFO_LOG << "Client::onNetworkUrlResolved: songId" << id << "resolve failed after retry";
        emit networkPlayResolveFailed(id);
        return;
    }
    m_retriedOnce = false;
    // 缓存该 url（网易 expi=1200s，按 20 分钟缓存，供切回复用避免 -110 限流）
    m_songUrlCache.insert(id, qMakePair(url, QDateTime::currentMSecsSinceEpoch()));
    if (m_playStartedByFallback) {
        // 已用外链起播：解析结果只更新缓存，不重新播放
        m_playStartedByFallback = false;
        INFO_LOG << "Client::onNetworkUrlResolved: fallback playing, only cache url";
        return;
    }
    requestPlayMusic(url);
}

/*!
 * @brief 取缓存中该 songId 的新鲜播放地址
 *
 * 网易返回的 URL 有效期 expi=1200s，这里按 20 分钟判断；过期就顺手从缓存里清掉。
 * 没有/已过期一律返回空串。
 */
QString Client::freshCachedUrl(qint64 songId)
{
    if (songId <= 0)
        return QString();

    auto it = m_songUrlCache.find(songId);
    if (it == m_songUrlCache.end())
        return QString();

    const QString cached = it.value().first;
    const qint64 cachedAt = it.value().second;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!cached.isEmpty() && (now - cachedAt) < 20 * 60 * 1000)
        return cached;

    m_songUrlCache.erase(it); // 过期缓存清理
    return QString();
}

/*!
 * @brief 起播打开失败后的补救（主线程）
 *
 * 背景：非 VIP 歌为了快启动，先用稳定的外链（music.163.com/song/media/outer/url）
 * 起播，真实 CDN 地址在后台并行解析、默认只进缓存不打断播放。但外链并非总能播
 * —— 返回 HTML 错误页或 403 时 avformat_open_input 会失败，而这条失败此前是
 * 静默的：既不切歌也不报错，播放器直接挂死（Client::message_loop 永远阻塞在
 * msg_queue_get 上）。现在由 Oran7FFplayerCore 补发 FFP_MSG_ERROR 把它暴露出来。
 *
 * 策略：手里已有该歌新鲜的已解析地址 → 换它重试一次；不行才认输，交给 QML
 * 已经接好的自动切歌流程（networkPlayResolveFailed）。
 */
void Client::onPlaybackOpenFailed()
{
    // 只处理网络源：本地文件失败的既有行为保持不变，避免引入回归
    if (!isNetworkUrl(appData.CurMediaFilePath)) {
        INFO_LOG << "Client::onPlaybackOpenFailed: non-network source, keep legacy behaviour:"
                 << appData.CurMediaFilePath;
        return;
    }

    const qint64 id = m_currentSongId;
    const QString cached = freshCachedUrl(id);
    if (!cached.isEmpty() && !m_openRetryDone) {
        m_openRetryDone = true;
        INFO_LOG << "Client::onPlaybackOpenFailed: retry with resolved url, songId" << id;
        // 复用已有入口：preparePlayingMedia 会发现 CurMediaFilePath（外链）与它不同，
        // 自动走「stopRequested → 300ms → playOrPauseRequested」的重启分支
        requestPlayMusic(cached);
        return;
    }

    INFO_LOG << "Client::onPlaybackOpenFailed: give up, songId" << id
             << " hasCache=" << !cached.isEmpty() << " retried=" << m_openRetryDone;
    emit networkPlayResolveFailed(id);
}

void Client::rememberLastMedia(const QString &name, const QString &artist,
                               const QString &album, const QString &cover,
                               const QString &timesize, qint64 songId, bool isVip)
{
    m_currentSongId = songId;
    m_currentIsVip = isVip;

    QElapsedTimer cfgT; cfgT.start();
    // 持久化网络歌元数据，供启动时恢复上次播放的网络歌到 BottomPage
    AppConfigManager::ins().setValueQVariant("last_media_name", name);
    AppConfigManager::ins().setValueQVariant("last_media_artist", artist);
    AppConfigManager::ins().setValueQVariant("last_media_album", album);
    AppConfigManager::ins().setValueQVariant("last_media_cover", cover);
    AppConfigManager::ins().setValueQVariant("last_media_timesize", timesize);
    AppConfigManager::ins().setValueQVariant("last_media_songid", songId);
    AppConfigManager::ins().setValueQVariant("last_media_isvip", isVip);
    AppConfigManager::ins().saveConfig();
    INFO_LOG << "rememberLastMedia config cost" << cfgT.elapsed() << "ms";
}

void Client::saveIntegratedMusic(const QVariantList &list)
{
    QMutexLocker locker(&client_mutex);
    // 确保 config.json 已加载，避免覆盖用户其它配置（幂等）
    AppConfigManager::ins().loadConfig();
    AppConfigManager::ins().setValueQVariantList("integratedMusic.list", list);
    AppConfigManager::ins().saveConfig();
}

QVariantList Client::loadIntegratedMusic()
{
    QMutexLocker locker(&client_mutex);
    // RightPage.onCompleted 可能早于 main.cpp 的 Config_AppConfigManager_Load，先确保已加载
    AppConfigManager::ins().loadConfig();
    return AppConfigManager::ins().getValueQVariantList("integratedMusic.list");
}

void Client::saveConfig_AppWindowSize(QQmlApplicationEngine &engine)
{
    int typeId = qmlTypeId("Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting");

    QQmlEngine* qmlEngine = static_cast<QQmlApplicationEngine*>(&engine);
    QObject* singleton = qmlEngine->singletonInstance<QObject*>(typeId);

    if(!singleton)
    {
        CONFIG_LOG<<"Failed to get singleton instance of Oran7MainUiSetting for saveConfig_AppWindowSize.";
        return;
    }
    int width = singleton->property("savedNormalWidth").toInt();
    int height = singleton->property("savedNormalHeight").toInt();

    if (width > 0 && height > 0)
    {
        AppConfigManager::ins().setValueQVariant("window.width", width);
        AppConfigManager::ins().setValueQVariant("window.height", height);
    }
}

void Client::saveConfig_AppWindowPosition(QQmlApplicationEngine &engine)
{
    int typeId = qmlTypeId("Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting");

    QQmlEngine* qmlEngine = static_cast<QQmlApplicationEngine*>(&engine);
    QObject* singleton = qmlEngine->singletonInstance<QObject*>(typeId);

    if(!singleton)
    {
        CONFIG_LOG<<"Failed to get singleton instance of Oran7MainUiSetting for saveConfig_AppWindowPosition.";
        return;
    }

    int posX = singleton->property("savedNormalX").toInt();
    int posY = singleton->property("savedNormalY").toInt();

    if(posX >= 0 && posY >=0)
    {
        AppConfigManager::ins().setValueQVariant("window.position.x",posX);
        AppConfigManager::ins().setValueQVariant("window.position.y",posY);
    }
}

void Client::saveConfig_AppSetPlayerVolume()
{
    AppConfigManager::ins().setValueQVariant("playerVolume",appData.playerVolume);
}

void Client::tryLoadLrcForFile(const QString &mediaFilePath)
{
    if(mediaFilePath.isEmpty() || appData.audioLRCDirPath.isEmpty())
    {
        emit lyricsUnavailable();
        return;
    }

    QFileInfo fileInfo(mediaFilePath);
    QString baseName = fileInfo.completeBaseName();
    QString lrcPath = appData.audioLRCDirPath + "/" + baseName + ".lrc";

    if(QFile::exists(lrcPath))
    {
        if (m_lyricsFetcher) m_lyricsFetcher->cancel(); // 取消正在进行的在线搜索
        INFO_LOG << "Client::tryLoadLrcForFile: LRC found:" << lrcPath;
        emit lyricsAvailable(lrcPath);
    }
    else
    {
        INFO_LOG << "Client::tryLoadLrcForFile: No LRC for:" << mediaFilePath;
        emit lyricsUnavailable();
        tryOnlineLyricsSearch(mediaFilePath); // 触发在线搜索
    }
}

void Client::tryOnlineLyricsSearch(const QString &mediaFilePath)
{
    if (!m_lyricsFetcher) return;

    // 网络资源：无本地文件可提取元数据，直接按网易云 songId 拉歌词
    if (isNetworkUrl(mediaFilePath)) {
        qint64 songId = m_currentSongId;   // 优先：播放时由 QML 注入的 songId（收藏/搜索结果项）
        if (songId <= 0)
            songId = extractSongId(mediaFilePath);
        // 真实 music.126.net 播放地址不含 id，从最近一次搜索结果反查
        if (songId <= 0)
            songId = Oran7MusicSearchService::instance().songIdForUrl(mediaFilePath);
        if (songId > 0) {
            INFO_LOG << "Client::tryOnlineLyricsSearch: network songId" << songId;
            emit onlineLyricsSearching();
            m_lyricsFetcher->searchById(songId, mediaFilePath, appData.audioLRCDirPath);
            return;
        }
        // 历史记录（m701 地址无 id、songId 丢失）：用保存的歌名/歌手走模糊歌词搜索
        const QString savedName   = AppConfigManager::ins().getValueQVariant("last_media_name").toString();
        const QString savedArtist = AppConfigManager::ins().getValueQVariant("last_media_artist").toString();
        if (!savedName.isEmpty()) {
            INFO_LOG << "Client::tryOnlineLyricsSearch: fallback by saved meta" << savedArtist << "-" << savedName;
            emit onlineLyricsSearching();
            m_lyricsFetcher->search(savedArtist, savedName, mediaFilePath, appData.audioLRCDirPath);
        }
        return;
    }

    // 通过 ffprobe 提取元数据（同步，通常 < 100ms）
    QMap<QString, QVariant> info = analyzeMediaFileInfo(mediaFilePath);
    if (info["success"].toInt() != 0) {
        INFO_LOG << "Client::tryOnlineLyricsSearch: cannot extract metadata";
        return;
    }

    QString artist = info["artist"].toString();
    QString title  = info["title"].toString();

    // 过滤占位值
    if (artist == "未知歌手" || artist.isEmpty() ||
        title  == "未知歌曲" || title.isEmpty()) {
        INFO_LOG << "Client::tryOnlineLyricsSearch: no meaningful metadata";
        return;
    }

    INFO_LOG << "Client::tryOnlineLyricsSearch:" << artist << "-" << title;
    emit onlineLyricsSearching();
    m_lyricsFetcher->search(artist, title, mediaFilePath, appData.audioLRCDirPath);
}






