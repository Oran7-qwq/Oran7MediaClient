#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QVariantMap>
#include "Oran7VideoThumbnailCache.h"
#include "Oran7PlaybackState.h"
#include <QPointer>
#include "Oran7LiveRoomInfoService.h"

class Oran7AppConfigManager;

// 视频空间的有界观看记录，GUI 线程持有，配置通过既有原子保存入口提交。
class Oran7VideoHistory : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap latest READ latest NOTIFY recordsChanged)
    Q_PROPERTY(int localCount READ localCount NOTIFY recordsChanged)
    Q_PROPERTY(int liveCount READ liveCount NOTIFY recordsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY recordsChanged)
public:
    enum Role { MediaKind = Qt::UserRole + 1, Target, Title, LastOpenedAt, Thumbnail, PositionMs, DurationMs, Completed,
        AnchorName, AvatarUrl, LiveStatus, RoomInfoState };
    explicit Oran7VideoHistory(Oran7AppConfigManager& config, QString cacheDirectory = {}, QString ffmpegExecutable = {},
        std::unique_ptr<QNetworkAccessManager> roomNetwork = {});
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariantMap latest() const;
    int localCount() const;
    int liveCount() const;
    QString errorString() const { return m_error; }
    Q_INVOKABLE void rememberLocal(const QString& path);
    Q_INVOKABLE void rememberRoom(const QString& roomNumber);
    Q_INVOKABLE void remove(const QString& kind, const QString& target);
    Q_INVOKABLE void requestThumbnail(const QString& path);
    Q_INVOKABLE void requestRoomInfo(const QString& room, bool force = false);
    bool flush();
    void bindPlayback(Oran7PlaybackState& playback);
    void beginLocalPlayback(const QString& path);
    void cancelResume() { m_resumePending = false; }
    void captureProgress();
    void shutdown() { m_thumbnails.shutdown(); m_roomInfo.shutdown(); }
signals:
    void recordsChanged();
    void resumeRequested(int seconds);
private:
    static QVariantMap normalized(const QVariantMap& record);
    static QString identity(const QVariantMap& record);
    void remember(QVariantMap record);
    void publish(QVariantList records);
    void restoreProgress();
    Oran7AppConfigManager& m_config;
    QVariantList m_records;
    QTimer m_saveTimer;
    QString m_error;
    bool m_dirty = false;
    Oran7VideoThumbnailCache m_thumbnails;
    QHash<QString,QString> m_thumbnailUrls;
    Oran7LiveRoomInfoService m_roomInfo;
    QHash<QString,int> m_roomInfoState;
    QPointer<Oran7PlaybackState> m_playback;
    QString m_activeSource;
    qint64 m_resumeMs = 0;
    bool m_resumePending = false, m_started = false;
    QTimer m_progressSaveTimer;
};
