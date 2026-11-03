#include "Oran7LocalLibraryService.h"
#include "Oran7MediaInfoReader.h"
#include "Oran7Async.h"
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QThread>
#include <QUrl>
#include <algorithm>
#include <exception>
#include <utility>

namespace {
QString pathKey(const QString& path)
{
    auto normalized = Oran7LocalLibraryService::normalizedPath(path);
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}
}

Oran7LocalLibraryService::Oran7LocalLibraryService(MetadataReader reader, TrashMover trashMover, QObject* parent)
    : QObject(parent), m_reader(std::move(reader)), m_trashMover(std::move(trashMover))
{
    if (!m_reader) m_reader = &Oran7MediaInfoReader::analyzeMediaFileInfo;
    if (!m_trashMover) m_trashMover = [](const QString& path) { return QFile::moveToTrash(path); };
}

Oran7LocalLibraryService::~Oran7LocalLibraryService() { shutdown(); }

QString Oran7LocalLibraryService::normalizedPath(const QString& path)
{
    if (path.isEmpty()) return {};
    const QUrl url(path);
    if (!url.isLocalFile() && path.contains("://")) return {};
    return QDir::cleanPath(QFileInfo(url.isLocalFile() ? url.toLocalFile() : path).absoluteFilePath());
}

void Oran7LocalLibraryService::scan(const QString& directory) { enqueue({Operation::Scan, directory, {}}); }
void Oran7LocalLibraryService::importFiles(const QString& directory, const QVariantList& paths)
{
    if (!paths.isEmpty()) enqueue({Operation::Import, directory, paths});
}
void Oran7LocalLibraryService::removeFiles(const QString& directory, const QVariantList& paths)
{
    if (!paths.isEmpty()) enqueue({Operation::Remove, directory, paths});
}

void Oran7LocalLibraryService::shutdown()
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_stopping = true;
    ++m_generation;
    m_queue.clear();
    if (m_scanCanceled) m_scanCanceled->store(true);
    m_task.cancel();
}

void Oran7LocalLibraryService::enqueue(Oran7LibraryJob job)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_stopping || Oran7Async::isShuttingDown()) return;
    job.directory = normalizedPath(job.directory);
    job.generation = ++m_generation;
    if (m_scanCanceled) m_scanCanceled->store(true);
    // 保留已接受的文件操作；未启动的扫描合并为最后一次请求。
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
        [](const auto& queued) { return queued.operation == Operation::Scan; }), m_queue.end());
    m_queue.append(std::move(job));
    startNext();
}

void Oran7LocalLibraryService::startNext()
{
    if (m_running || m_queue.isEmpty() || m_stopping) return;
    const auto job = m_queue.takeFirst();
    m_running = true;
    m_scanCanceled = std::make_shared<std::atomic_bool>(false);
    m_task = Oran7Async::run(this,
        [job, reader = m_reader, trashMover = m_trashMover, canceled = m_scanCanceled] {
            return execute(job, reader, trashMover, canceled);
        },
        [this, generation = job.generation](Oran7LibraryResult result) {
            QPointer<Oran7LocalLibraryService> guard(this);
            for (const auto& error : std::as_const(result.errors)) {
                emit operationFailed(error);
                if (!guard) return;
            }
            if (!m_stopping && generation == m_generation && result.valid) {
                m_entries = std::move(result.entries);
                sortEntries(); // 使用 GUI 最新顺序，不覆盖扫描期间发生的拖拽排序。
                QVariantList tracks;
                tracks.reserve(m_entries.size());
                for (const auto& entry : std::as_const(m_entries)) {
                    tracks.append(QVariantMap{{"cover",entry.cover},{"title",entry.title},
                        {"artist",entry.artist},{"album",entry.album},{"timesize",entry.timesize},
                        {"path",entry.file.absoluteFilePath()},{"source",QStringLiteral("local")}});
                }
                emit tracksReady(tracks);
                if (!guard) return;
            }
            m_running = false;
            startNext();
        });
    if (!m_task.isValid()) { m_running = false; m_queue.clear(); }
}

Oran7LocalLibraryService::Oran7LibraryResult Oran7LocalLibraryService::execute(
    const Oran7LibraryJob& job, const MetadataReader& reader, const TrashMover& trashMover,
    const std::shared_ptr<std::atomic_bool>& canceled)
{
    Oran7LibraryResult result;
    try {
        if (job.directory.isEmpty()) { result.valid = true; return result; }
        if (job.operation == Operation::Import && !QDir().mkpath(job.directory)) {
            result.errors.append(QStringLiteral("无法创建曲库目录：%1").arg(job.directory));
            return result;
        }
        const QString canonicalRoot = QDir(job.directory).canonicalPath();
        QSet<QString> visited;
        for (const auto& value : job.paths) {
            const QString source = normalizedPath(value.toString());
            const QString key = pathKey(source);
            if (source.isEmpty() || visited.contains(key)) continue;
            visited.insert(key);
            const QFileInfo file(source);
            if (job.operation == Operation::Import) {
                const QString destination = QDir(job.directory).filePath(file.fileName());
                if (!file.isFile()) {
                    result.errors.append(QStringLiteral("导入文件不存在：%1").arg(source));
                } else if (!QFileInfo::exists(destination) && !QFile::copy(source, destination)) {
                    result.errors.append(QStringLiteral("导入失败：%1").arg(source));
                }
            } else if (job.operation == Operation::Remove) {
                // 只允许本次曲库目录中的普通文件，拒绝外部路径、目录与符号链接。
                if (canonicalRoot.isEmpty() || pathKey(file.absoluteDir().canonicalPath()) != pathKey(canonicalRoot)
                    || file.isDir() || file.isSymLink()) {
                    result.errors.append(QStringLiteral("跳过曲库目录外或不支持的文件：%1").arg(source));
                } else if (file.exists()) {
                    if (trashMover(source)) {
                        QFile::remove(file.absolutePath() + "/audioCover/" + file.completeBaseName() + ".jpg");
                    } else {
                        result.errors.append(QStringLiteral("移至回收站失败：%1").arg(source));
                    }
                }
            }
        }
        QDirIterator iterator(job.directory, QDir::Files);
        while (!canceled->load() && iterator.hasNext()) {
            const QString path = iterator.next();
            try {
                const auto info = reader(path);
                if (info.value("success", 1).toInt() != 0) continue;
                Oran7LibraryEntry entry;
                entry.file = QFileInfo(path);
                entry.title = info.value("title").toString();
                entry.artist = info.value("artist").toString();
                entry.album = info.value("album").toString();
                entry.timesize = info.value("duration").toString();
                const QString cover = info.value("cover").toString();
                entry.cover = QUrl(cover).isLocalFile() ? QUrl(cover).toString(QUrl::FullyEncoded)
                    : (cover.isEmpty() ? QString() : QUrl::fromLocalFile(cover).toString(QUrl::FullyEncoded));
                result.entries.append(std::move(entry));
            } catch (const std::exception& error) {
                result.errors.append(QStringLiteral("读取媒体失败：%1（%2）").arg(path, QString::fromUtf8(error.what())));
            }
        }
        result.valid = !canceled->load();
    } catch (const std::exception& error) {
        result.errors.append(QStringLiteral("曲库操作失败：%1").arg(QString::fromUtf8(error.what())));
    } catch (...) {
        result.errors.append(QStringLiteral("曲库操作发生未知异常"));
    }
    return result;
}

QList<QFileInfo> Oran7LocalLibraryService::sortedFiles(const QList<QFileInfo>& files, const QVariantList& order)
{
    QHash<QString, int> ranks;
    for (const auto& value : order) {
        const auto key = pathKey(value.toString());
        if (!key.isEmpty() && !ranks.contains(key)) ranks.insert(key, ranks.size());
    }
    auto sorted = files;
    std::sort(sorted.begin(), sorted.end(), [&ranks](const auto& first, const auto& second) {
        const auto a = pathKey(first.absoluteFilePath()), b = pathKey(second.absoluteFilePath());
        const int ra = ranks.value(a, -1), rb = ranks.value(b, -1);
        if (ra >= 0 && rb >= 0) return ra < rb;
        if (ra >= 0 || rb >= 0) return ra < 0; // 沿用新增文件排在保存顺序之前的规则。
        if (first.birthTime() != second.birthTime()) return first.birthTime() > second.birthTime();
        return a < b;
    });
    return sorted;
}

void Oran7LocalLibraryService::sortEntries()
{
    const auto sorted = sortedFiles(files(), m_order);
    QHash<QString, Oran7LibraryEntry> entries;
    for (const auto& entry : std::as_const(m_entries)) entries.insert(pathKey(entry.file.absoluteFilePath()), entry);
    m_entries.clear();
    m_order.clear();
    for (const auto& file : sorted) {
        m_entries.append(entries.value(pathKey(file.absoluteFilePath())));
        m_order.append(file.absoluteFilePath());
    }
}

void Oran7LocalLibraryService::setPlayOrder(const QVariantList& paths)
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_order = paths;
    if (!m_entries.isEmpty()) sortEntries();
}
QVariantList Oran7LocalLibraryService::playOrder() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return m_order;
}
QList<QFileInfo> Oran7LocalLibraryService::files() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QList<QFileInfo> result;
    for (const auto& entry : m_entries) result.append(entry.file);
    return result;
}
