#pragma once
#include "Oran7PlaybackState.h"
#include "../Oran7Config/Oran7AppConfig.h"
#include <QMap>
#include <QVariant>
#include <functional>
#include <optional>

class Oran7AppConfigManager;
class QQmlApplicationEngine;

// 配置与界面/媒体值之间的映射；不持有播放器、网络管理器或第二份播放状态。
class Oran7ConfigBinding
{
public:
    using MetadataReader = std::function<QMap<QString, QVariant>(QString)>;
    explicit Oran7ConfigBinding(Oran7AppConfigManager& config, MetadataReader reader = {});
    QString createDirectories();
    QString audioDirectory() const { return m_audioDirectory; }
    QString lyricsDirectory() const { return m_audioDirectory + "/audioLRC"; }
    std::optional<Oran7Track> restoredTrack();
    std::optional<Oran7Track> localTrack(const QString& path);
    void rememberTrack(const Oran7Track& track);
    void completePersistedDetails(const Oran7Track& track);
    void rememberMusicSource(const QString& path, bool isVip);
    void setFocusedMusicSource(const QString& path);
    void saveLastFocused(bool isVip);
    QVariantList localOrder() const;
    void saveLocalOrder(const QVariantList& order);
    QVariantList integratedMusic();
    void saveIntegratedMusic(const QVariantList& list);
    int volume() const;
    void saveVolume(int volume);
    Oran7AudioConfig audio() const;
    void saveAudio(const Oran7AudioConfig& audio);
    bool roamFilterLiked() const;
    bool saveRoamFilterLiked(bool enabled);
    void loadWindowSize(QQmlApplicationEngine& engine);
    void loadWindowPosition(QQmlApplicationEngine& engine);
    void saveWindowSize(QQmlApplicationEngine& engine);
    void saveWindowPosition(QQmlApplicationEngine& engine);
private:
    Oran7AppConfigManager& m_config;
    MetadataReader m_reader;
    QString m_audioDirectory;
    QString m_lastMusicSource; // 最后音乐入口，仅供持久化；播放视频时仍保留。
};
