#ifndef ASYNCMANAGERS_H
#define ASYNCMANAGERS_H

#pragma once
#include <QObject>
#include <QRunnable>
#include <QString>
#include <QThreadPool>
#include <QMutex>
#include <QTimer>
#include <QHash>

#include <QFileInfo>
#include <QQmlEngine>

enum TaskType {
    NONE_Task_ = 0,
    SearchLocalMediaFiles_Task_
};
//AsyncTask base Class,by every children class task to inheritance for do work（Will be submit to threadPool for execution）
class AsyncTask : public QObject,public QRunnable
{
    Q_OBJECT
public:
    explicit AsyncTask(QObject * parent=nullptr) : QObject(parent){
        setAutoDelete(false);
    }

    virtual void process() = 0;
    virtual TaskType type() const = 0;

    void run() override{
        process();
    }
signals:
    void taskCompleted(const QString& result);
    void taskError(const QString& error);
    void taskProgressUpdated(int taskId,int percent,const QString& statues);
    void taskSpecificResult(const QVariant& data);
};

//==========Tasks Classes declare==========//
class SearchLocalMediaFiles_Task;


//total AsyncTasks Manager
class AsyncWorker : public QObject
{
    Q_OBJECT
    //For qml property
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(int activeTaskCount READ activeTaskCount NOTIFY activeTaskCountChanged)
public:
    explicit AsyncWorker(QObject *parent=nullptr);
    ~AsyncWorker();

    //====For Qml Ways====//
    Q_INVOKABLE int startSearchLocalMediaFiles_Task(const QString& folderPath);

    //universal startTask way
    Q_INVOKABLE int startTask(int taskType,const QVariantMap& parameters);

public:
    bool isBusy() const {return m_runningTasks.isEmpty();}
    int activeTaskCount() const {return m_runningTasks.size();}

private:
    int generateTaskId();
    void registerTask(int taskId,AsyncTask *task);
    void unregisterTask(int taskId);
    void connectTaskSignals(AsyncTask* task, int taskId);
    void cancelTask(int taskId);
    void cancelAll();

private:
    QThreadPool *m_threadPool;
    QMutex m_mutex;

    //Tasks manager
    int m_nextTaskId = 1;
    QHash<int,QPointer<AsyncTask>> m_runningTasks;
    QHash<int,QVariantMap> m_taskParameters;
    QHash<int,QVariantList> m_taskResults;

    //Tasks statistic
    QHash<int,QElapsedTimer> m_taskTimers;

signals:
    //normal state
    void busyChanged();
    void activeTaskCountChanged();

    //normal task
    void taskStarted(int taskId,TaskType taskType);
    void taskProgress(int taskId,int percent,const QString& statues);
    void taskCompleted(int taskId,const QString& result);
    void taskError(int taskId,const QString& error);
    void taskCancelled(int taskId);

    //specific task
    void SearchLocalMediaFiles_Result(int taskId,const QString& filePath);
};

//==========Tasks Classes define==========
class SearchLocalMediaFiles_Task : public AsyncTask
{
    Q_OBJECT
public:
    struct Parameters {
        QString directory;
    };

    explicit SearchLocalMediaFiles_Task(int taskId,const Parameters& params={},QObject *parent =nullptr);

    TaskType type() const override {return TaskType::SearchLocalMediaFiles_Task_;}
    void process()override;
    void cancel(){m_cancelled.store(true);}

signals:
    void fileCountUpdated(int Count);

private:
    int m_taskId;
    Parameters m_params;
    //atomic
    std::atomic<bool> m_cancelled{false};
    std::atomic<int> m_fileCount{0};
};


#endif // ASYNCMANAGERS_H
