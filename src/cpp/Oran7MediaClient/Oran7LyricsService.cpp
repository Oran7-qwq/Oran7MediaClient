#include "Oran7LyricsService.h"
#include "Oran7OnlineLyricsFetcher.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>
#include <utility>

Oran7LyricsService::Oran7LyricsService(MetadataReader metadataReader, QObject* parent,
                                     FetcherFactory fetcherFactory)
    : QObject(parent), m_metadataReader(std::move(metadataReader)),
      m_fetcherFactory(std::move(fetcherFactory))
{
    if (!m_fetcherFactory)
        m_fetcherFactory = [] { return std::make_unique<Oran7OnlineLyricsFetcher>(); };
}

Oran7LyricsService::~Oran7LyricsService()
{
    resetRequest();
}

void Oran7LyricsService::resetRequest()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++m_generation; // 必须先失效：abort 可能同步触发网络完成回调。
    m_fetcher.reset();
    m_networkDirectory.reset();
}

void Oran7LyricsService::stop()
{
    resetRequest();
    emit lyricsUnavailable();
}

void Oran7LyricsService::load(const QString& mediaPath, const QString& lrcDirectory, const Oran7Track& track)
{
    resetRequest();
    const quint64 generation = m_generation;
    emit lyricsUnavailable();
    if (generation != m_generation || mediaPath.isEmpty() || lrcDirectory.isEmpty()) return;

    const QUrl url(mediaPath);
    const bool network = url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https");
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : mediaPath;
    if (!network) {
        const QString cachedPath = QDir(lrcDirectory).filePath(QFileInfo(localPath).completeBaseName() + ".lrc");
        if (QFileInfo(cachedPath).isFile()) {
            emit lyricsAvailable(cachedPath);
            return;
        }
    }

    qint64 songId = network ? track.songId : 0;
    QString artist = track.artist;
    QString title = track.title;
    if (network && songId <= 0) {
        const auto match = QRegularExpression(QStringLiteral("[?&]id=(\\d+)")).match(mediaPath);
        if (match.hasMatch()) songId = match.captured(1).toLongLong();
    }
    if (!network) {
        const auto info = m_metadataReader ? m_metadataReader(localPath) : QMap<QString, QVariant>();
        if (generation != m_generation) return;
        if (info.isEmpty() || info.value("success").toInt() != 0) {
            emit onlineLyricsNotFound();
            return;
        }
        artist = info.value("artist").toString();
        title = info.value("title").toString();
    }
    if (songId <= 0 && (title.isEmpty() || title == QStringLiteral("未知歌曲")
        || (!network && (artist.isEmpty() || artist == QStringLiteral("未知歌手"))))) {
        emit onlineLyricsNotFound();
        return;
    }

    if (!QDir().mkpath(lrcDirectory)) {
        emit onlineLyricsNotFound();
        return;
    }
    QString targetDirectory = lrcDirectory;
    if (network) {
        // 每轮拥有独立目录：CDN URL 无 id 或两轮 songId 相同都不会共享临时文件。
        m_networkDirectory = std::make_unique<QTemporaryDir>(QDir(lrcDirectory).filePath("oran7-lyrics-XXXXXX"));
        if (!m_networkDirectory->isValid()) {
            m_networkDirectory.reset();
            emit onlineLyricsNotFound();
            return;
        }
        targetDirectory = m_networkDirectory->path();
    }
    m_fetcher = m_fetcherFactory();
    if (!m_fetcher) {
        m_networkDirectory.reset();
        emit onlineLyricsNotFound();
        return;
    }
    connect(m_fetcher.get(), &Oran7OnlineLyricsFetcher::lyricsFound, this,
        [this, generation](const QString& path) {
            if (generation == m_generation) emit lyricsAvailable(path);
        }, Qt::QueuedConnection);
    connect(m_fetcher.get(), &Oran7OnlineLyricsFetcher::lyricsNotFound, this,
        [this, generation] {
            if (generation == m_generation) emit onlineLyricsNotFound();
        }, Qt::QueuedConnection);
    emit onlineLyricsSearching();
    if (generation != m_generation) return;
    if (songId > 0) m_fetcher->searchById(songId, mediaPath, targetDirectory);
    else m_fetcher->search(artist, title, mediaPath, targetDirectory);
}
