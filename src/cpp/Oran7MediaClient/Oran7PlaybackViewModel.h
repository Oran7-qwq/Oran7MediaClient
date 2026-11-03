#pragma once
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include "Oran7RoamFilterStrategy.h"
class Oran7TrackLibrary;
class Oran7TrackListModel;
class Oran7PlaybackState;
class Oran7CloudMusicService;

// 音乐播放意图、归属与队列策略；播放事实仍只存于 Oran7PlaybackState。
class Oran7PlaybackViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString owner READ owner NOTIFY ownerChanged)
    Q_PROPERTY(bool roamFilterLiked READ roamFilterLiked WRITE setRoamFilterLiked NOTIFY roamFilterLikedChanged)
public:
    Oran7PlaybackViewModel(Oran7TrackLibrary& tracks, Oran7PlaybackState& playback,
                          Oran7CloudMusicService& cloud);
    QString owner() const { return m_owner; }
    bool roamFilterLiked() const { return m_roamFilterLiked; }
    void setRoamFilterLiked(bool enabled);
    quint64 commandRevision() const { return m_commandRevision; }
    Q_INVOKABLE bool activate(Oran7TrackListModel* model, int index);
    Q_INVOKABLE void toggleCurrent();
    void advance(int direction);
    void resolveFailed(qint64 songId);
    void cancel(bool leaveMusic = false);
    void shutdown() { m_closed = true; cancel(true); }
signals:
    void ownerChanged();
    void roamFilterLikedChanged();
    void musicActivated();
    void trackRequested(const QVariantMap& track, bool restart);
    void stopRequested();
private:
    void playRow(Oran7TrackListModel* model, int row, bool restart);
    void playTrack(QVariantMap fields, bool restart);
    void clearRoam();
    void nextRoam();
    void requestRoam(bool waiting);
    void prunePendingRoam();
    bool acceptsRoam(const QVariant& song) const;
    bool owns(Oran7TrackListModel* model) const;
    void adoptCurrent();
    Oran7TrackLibrary& m_tracks;
    Oran7PlaybackState& m_playback;
    Oran7CloudMusicService& m_cloud;
    QPointer<Oran7TrackListModel> m_model;
    QString m_trackId, m_owner;
    int m_anchor = -1, m_failures = 0;
    bool m_enabled = false, m_waiting = false;
    bool m_musicSuppressed = false;
    bool m_closed = false;
    QVariantList m_roam;
    int m_roamIndex = 0;
    std::unique_ptr<Oran7RoamFilterStrategy> m_roamFilter;
    bool m_roamFilterLiked = false, m_waitingForLikes = false;
    int m_filteredBatches = 0;
    quint64 m_roamRequest = 0;
    quint64 m_commandRevision = 0;
};
