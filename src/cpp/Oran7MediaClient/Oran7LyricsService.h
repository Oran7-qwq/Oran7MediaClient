#pragma once

#include "Oran7PlaybackState.h"
#include <QMap>
#include <QVariant>
#include <functional>
#include <memory>

class Oran7OnlineLyricsFetcher;
class QTemporaryDir;

// GUI 线程的歌词服务；不读取 Client、配置单例或当前播放器的可变状态。
class Oran7LyricsService : public QObject
{
    Q_OBJECT
public:
    using MetadataReader = std::function<QMap<QString, QVariant>(const QString&)>;
    using FetcherFactory = std::function<std::unique_ptr<Oran7OnlineLyricsFetcher>()>;
    explicit Oran7LyricsService(MetadataReader metadataReader, QObject* parent = nullptr,
                               FetcherFactory fetcherFactory = {});
    ~Oran7LyricsService() override;

    void load(const QString& mediaPath, const QString& lrcDirectory, const Oran7Track& track);
    void stop();

signals:
    void lyricsAvailable(const QString& path);
    void lyricsUnavailable();
    void onlineLyricsSearching();
    void onlineLyricsNotFound();

private:
    void resetRequest();
    MetadataReader m_metadataReader;
    FetcherFactory m_fetcherFactory;
    std::unique_ptr<QTemporaryDir> m_networkDirectory;
    std::unique_ptr<Oran7OnlineLyricsFetcher> m_fetcher;
    quint64 m_generation = 0;
};
