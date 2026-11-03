#pragma once
#include "Oran7TaskHandle.h"
#include <QObject>
#include <QFileInfo>
#include <QMap>
#include <QVariant>
#include <functional>

// 文件集合、排序只在 GUI 线程更新。后台任务仅持有值快照和可独立存活的函数。
class Oran7LocalLibraryService : public QObject
{
    Q_OBJECT
public:
    using MetadataReader = std::function<QMap<QString, QVariant>(const QString&)>;
    using TrashMover = std::function<bool(const QString&)>;
    explicit Oran7LocalLibraryService(MetadataReader reader = {}, TrashMover trashMover = {},
                                     QObject* parent = nullptr);
    ~Oran7LocalLibraryService() override;
    void scan(const QString& directory);
    void importFiles(const QString& directory, const QVariantList& paths);
    void removeFiles(const QString& directory, const QVariantList& paths);
    void setPlayOrder(const QVariantList& paths);
    QVariantList playOrder() const;
    QList<QFileInfo> files() const;
    bool isBusy() const { return !m_stopping && (m_running || !m_queue.isEmpty()); }
    void shutdown();
    static QString normalizedPath(const QString& path);
    static QList<QFileInfo> sortedFiles(const QList<QFileInfo>& files, const QVariantList& order);

signals:
    void tracksReady(const QVariantList& tracks);
    void operationFailed(const QString& message);

private:
    struct Oran7LibraryEntry {
        QFileInfo file;
        QString cover, title, artist, album, timesize;
    };
    enum class Operation { Scan, Import, Remove };
    struct Oran7LibraryJob {
        Operation operation;
        QString directory;
        QVariantList paths;
        quint64 generation = 0;
    };
    struct Oran7LibraryResult {
        QList<Oran7LibraryEntry> entries;
        QStringList errors;
        bool valid = false;
    };
    void enqueue(Oran7LibraryJob job);
    void startNext();
    void sortEntries();
    static Oran7LibraryResult execute(const Oran7LibraryJob& job, const MetadataReader& reader,
                                     const TrashMover& trashMover,
                                     const std::shared_ptr<std::atomic_bool>& canceled);
    MetadataReader m_reader;
    TrashMover m_trashMover;
    QList<Oran7LibraryEntry> m_entries;
    QVariantList m_order;
    QList<Oran7LibraryJob> m_queue;
    Oran7TaskHandle m_task;
    std::shared_ptr<std::atomic_bool> m_scanCanceled;
    quint64 m_generation = 0;
    bool m_running = false;
    bool m_stopping = false;
};
