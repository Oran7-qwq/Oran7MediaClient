#include "AsyncManagers.h"

#include "Client.h"
#include "ApplicationContext.h"

#include <QDebug>
#include <QVariant>
#include <QDir>
#include <QDirIterator>

AsyncWorker::AsyncWorker(QObject *parent)
    : QObject(parent)
    , m_threadPool(new QThreadPool(this))
{
    m_threadPool->setMaxThreadCount(QThread::idealThreadCount());
    qDebug() << "[AsyncWorker initialized:] max threads:"
            << m_threadPool->maxThreadCount();
}

AsyncWorker::~AsyncWorker()
{
    this->cancelAll();
    m_runningTasks.clear();
    m_threadPool->waitForDone();
}

int AsyncWorker::startSearchLocalMediaFiles_Task(const QString &folderPath)
{
    SearchLocalMediaFiles_Task::Parameters params{};
    params.directory=folderPath;
    int taskId=generateTaskId();

    //create task object
    SearchLocalMediaFiles_Task *task=new SearchLocalMediaFiles_Task(taskId,params,this);

    connectTaskSignals(task,taskId);
    registerTask(taskId,task);

    //submit to threadPool for deal with
    m_threadPool->start(task);

    //emit signal
    emit taskStarted(taskId,TaskType::SearchLocalMediaFiles_Task_);

    return taskId;
}

int AsyncWorker::startTask(int taskType, const QVariantMap &parameters)
{
    int taskId = generateTaskId();
    AsyncTask *task =nullptr;
    switch (taskType)
    {
    case TaskType::SearchLocalMediaFiles_Task_:{
        SearchLocalMediaFiles_Task::Parameters params;
        params.directory = parameters["directory"].toString();
        task = new SearchLocalMediaFiles_Task(taskId,params,this);
        break;
    }
    default:
        qWarning()<<"[AsyncWorker::startTask:]Unknown AsyncTask type:"<< taskType;
        break;
    }

    if(task !=nullptr)
    {
        connectTaskSignals(task,taskId);
        registerTask(taskId,task);
        m_threadPool->start(task);
        emit taskStarted(taskId,static_cast<TaskType>(taskType));
    }

    return taskId;
}

int AsyncWorker::generateTaskId()
{
    QMutexLocker locker(&m_mutex);
    int id = m_nextTaskId ++;
    if(m_nextTaskId >=65535)return -1;
    return id;
}

void AsyncWorker::registerTask(int taskId, AsyncTask *task)
{
    QMutexLocker locker(&m_mutex);
    m_runningTasks[taskId]=task;
    m_taskTimers[taskId].start();
}

void AsyncWorker::unregisterTask(int taskId)
{
    QMutexLocker locker(&m_mutex);
    AsyncTask *task = m_runningTasks.value(taskId);
    if (task)
    {
        task->disconnect();  // 断开所有连接
    }
    m_runningTasks.remove(taskId);
    m_taskTimers.remove(taskId);
    m_taskParameters.remove(taskId);
}

void AsyncWorker::connectTaskSignals(AsyncTask *task, int taskId)
{
    if (!task) {
        qWarning() << "connectTaskSignals: task is null for taskId" << taskId;
        emit taskError(taskId, "Failed to connect task signals: task is null");
        return;
    }

    QPointer<AsyncTask> taskPtr(task);

    #ifndef __clang_analyzer__
    connect(task,&AsyncTask::taskCompleted,this,[this, taskId, taskPtr](const QString& result){
        if (!taskPtr) return;
        unregisterTask(taskId);
        taskPtr->deleteLater();
        emit taskCompleted(taskId, result);
    });
    #endif

    #ifndef __clang_analyzer__
    connect(task,&AsyncTask::taskError,this,[this, taskId, taskPtr](const QString& error){
        if (!taskPtr) return;
        unregisterTask(taskId);
        taskPtr->deleteLater();
        emit taskError(taskId,error);
    });
    #endif

    #ifndef __clang_analyzer__
    connect(task,&AsyncTask::taskProgressUpdated,this,[this, taskId, taskPtr](int percent){
        emit taskProgress(taskId,percent,"");
    });
    #endif

    //Result
    #ifndef __clang_analyzer__
    connect(task,&AsyncTask::taskSpecificResult,this,[this,taskId](const QVariant& data){
        AsyncTask *task = m_runningTasks.value(taskId);
        if(!task)return;
        switch (task->type())
        {
        case TaskType::SearchLocalMediaFiles_Task_:
            //deal
            break;
        default:
            break;
        }
    });
    #endif
}

void AsyncWorker::cancelTask(int taskId)
{
    QMutexLocker locker(&m_mutex);
    if(AsyncTask *task = m_runningTasks.value(taskId))
    {
        if(SearchLocalMediaFiles_Task* specific_task = qobject_cast<SearchLocalMediaFiles_Task*>(task))
        {
            specific_task->cancel();
        }
        emit taskCancelled(taskId);
        emit activeTaskCountChanged();
    }
}

void AsyncWorker::cancelAll()
{
    QMutexLocker locker(&m_mutex);

    m_threadPool->clear();

    for(AsyncTask *task : std::as_const(m_runningTasks))
    {
        QPointer<AsyncTask> taskPtr = task;
        taskPtr->disconnect();
        taskPtr->deleteLater();
    }

    m_runningTasks.clear();
    m_taskParameters.clear();
    m_taskTimers.clear();

    emit activeTaskCountChanged();
    emit busyChanged();
}


//========================SearchLocalMediaFiles_Task=============================//

SearchLocalMediaFiles_Task::SearchLocalMediaFiles_Task(int taskId, const Parameters &params, QObject *parent)
    : m_taskId(taskId)
    , m_params(params)
    , AsyncTask(parent)
{

}

void SearchLocalMediaFiles_Task::process()
{
    m_cancelled.store(false);
    m_fileCount.store(0);

    Client * const client=ApplicationContext::instance().client();

    const QString &folderPath=client->createAppDirectories();
    if(folderPath.isEmpty())
    {
        qWarning()<<"[SearchLocalMediaFiles_Task::process:]AppData Temp folder in Oran7CloudMusic Could not be Created correctly";
        return;
    }

    //local container
    QList<QFileInfo>& localFileList=client->getLocalMusic_fileList();
    QSet<QString>& localFileSet=client->getLocalMusic_fileSet();
    QVariantList& localPlayOrder=client->getCustom_LocalMusic_playOrder();//from Config.josn

    //*访问Client共享数据，加锁
    QMutex& clientMutex = client->getClientMutex();
    QMutexLocker locker(&clientMutex);

    // 递归遍历所有文件 ,不要使用QDirIterator::Subdirectories遍历子目录
    QDirIterator it(folderPath, QDir::Files);
    while (it.hasNext())
    {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        if(!localFileSet.contains(fileInfo.absoluteFilePath()) || localFileSet.isEmpty())
        {
            //存储QFileInfo in QList
            localFileList.append(fileInfo);
            // 用于查重，或缺失检测
            localFileSet.insert(fileInfo.absoluteFilePath());
            //qDebug()<<"Successed insert "<<fileInfo.absoluteFilePath()<<" to QSet.";
        }
    }

    //查找记录的文件是否缺失，通过QSet记录来检测，若丢失则删除对应临时记录
    for(const QVariant& qV : std::as_const(localPlayOrder))
    {
        if(!localFileSet.contains(qV.toString()))
        {
            for (int i = localPlayOrder.size() - 1; i >= 0; --i)
                if (localPlayOrder.at(i)==qV)
                    localPlayOrder.removeAt(i);
        }
    }

    //std::sort
    QList<QFileInfo> sorted_musicList = client->sortFileInfoList_byFilePaths(localFileList,localPlayOrder);//自动处理新文件加入的结果，排序按默认加入头部
    localFileList = sorted_musicList;
    //更新临时存储的Custom_LocalMusic_playOrder，可能有新的文件加入
    if(localPlayOrder.size() != sorted_musicList.size())
    {
        localPlayOrder.clear();
        for(const QFileInfo& fl : std::as_const(sorted_musicList))
        {
            localPlayOrder.append(fl.absoluteFilePath());
        }
    }

    /*遍历获取媒体文件信息, 并打入qml前端ui*/
    for (/*const auto &fileInfo : std::as_const(sorted_musicList)*/int i=0;i<sorted_musicList.size();i++)
    {
        const auto &fileInfo =sorted_musicList[i];
        //qDebug()<<"[FileBirthTime:]"<<fileInfo.fileName()<<"-->"<<fileInfo.birthTime();
        QMap<QString,QVariant> mediaInfo= client->analyzeMediaFileInfo(fileInfo.absoluteFilePath());
        if(mediaInfo["success"].toInt() !=0)
        {
            qDebug()<<"[SearchLocalMediaFiles_Task::process:]mediaInfo extract Failed:"<<mediaInfo["success"].toInt();
            continue;
        }

        /*编码封面路径*/
        QString coverPath = mediaInfo["cover"].toString();
        // 调试：检查原始路径
        //qDebug() << "原始Cover Path:" << coverPath;
        //qDebug() << "文件是否存在:" << QFile::exists(coverPath);
        // 确保路径格式正确
        if (!coverPath.startsWith("file:///")) {
            coverPath = "file:///" + QDir::toNativeSeparators(coverPath);
        }
        // 对特殊字符进行URL编码
        QUrl url(coverPath);
        QString encodedPath = url.toString(QUrl::FullyEncoded);

        /*将分析出的所有媒体相关信息数据打入qml前端ui的数据模型中*/
        // qDebug()<<"Encode Cover Path:"<<encodedPath;
        emit client->pushLocalMusicListListModel_ElementData(
            encodedPath,                              //cover Path
            mediaInfo["title"].toString(),         //music_name
            mediaInfo["artist"].toString(),       //music_artist
            mediaInfo["album"].toString(),     //music_album
            mediaInfo["duration"].toString(),   //music_duration
            QString::number(-1),                    //music_id
            fileInfo.absoluteFilePath());           //music_localPath
        //触发加入动画
        emit client->triggerAddNewMusic_OpcityAniamtion(i);
    }

    //<<---Discard ,by reference
    // client->saveCustom_LocalMusic_playOrder(localPlayOrder);
    // client->saveLocalMusic_fileSet(localFileSet);
    //client->saveLocalMusic_fileList(localFileList);

    //一定要触发任结束信号，来删除任务对象
    emit taskCompleted("Successed.");
}
