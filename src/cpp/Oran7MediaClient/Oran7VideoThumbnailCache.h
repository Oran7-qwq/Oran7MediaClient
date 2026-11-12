#pragma once
#include <QObject>
#include <QQueue>
#include <QSet>
#include <atomic>
#include <memory>

// 单队列后台提帧，工作闭包只持路径值和取消标记。
class Oran7VideoThumbnailCache : public QObject
{
    Q_OBJECT
public:
    explicit Oran7VideoThumbnailCache(QString directory = {}, QString executable = {});
    ~Oran7VideoThumbnailCache() override;
    void request(const QString& path);
    void shutdown();
signals:
    void ready(const QString& path, const QString& url);
private:
    void startNext();
    QString m_directory, m_executable;
    QQueue<QString> m_queue;
    QSet<QString> m_pending;
    bool m_running = false;
    std::shared_ptr<std::atomic_bool> m_stopped;
};
