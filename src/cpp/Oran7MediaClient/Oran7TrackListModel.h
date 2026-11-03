#pragma once
#include "Oran7Track.h"
#include <QAbstractListModel>
#include <QSet>
#include <QVector>

// GUI 线程持有的统一曲目模型。元数据只读，界面只通过命令修改选择/动画/顺序。
class Oran7TrackListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role {
        TrackIdRole = Qt::UserRole + 1, UrlRole, TitleRole, ArtistRole, AlbumRole,
        CoverRole, DurationMsRole, IsVipRole, IsLikedRole, IsIntegratedRole, PathRole,
        SongIdRole, SourceRole, IsSelectedRole, NeedsAnimateInRole,
        IconRole, MusicNameRole, MusicArtistRole, MusicAlbumRole, TimesizeRole,
        FilepathRole, IsLovedRole
    };
    Q_ENUM(Role)
    explicit Oran7TrackListModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_entries.size(); }
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE bool setSelected(int row, bool selected);
    Q_INVOKABLE bool setAnimationPending(int row, bool pending);
    Q_INVOKABLE bool moveTrack(int from, int to);
    Q_INVOKABLE int indexOfTrack(const QString& trackId) const;
    Q_INVOKABLE int indexOfSongId(qint64 songId) const;
    Q_INVOKABLE int indexOfSource(const QString& source) const;

    // C++ 数据入口，不向 QML 暴露通用 append/setProperty/remove。
    void replaceTracks(const QVariantList& tracks);
    void prependTracks(const QVariantList& tracks);
    bool removeMatching(qint64 songId, const QString& url, const QString& path);
    bool contains(qint64 songId, const QString& url = {}, const QString& path = {}) const;
    QVariantList serializedTracks() const;
    QVariantList paths() const;
    QSet<qint64> songIds() const;
    QSet<QString> identityKeys() const { return m_keys; }
    void updateMembership(const QSet<qint64>& liked, const QSet<QString>& integrated);
    static QStringList identityKeys(qint64 songId, const QString& url, const QString& path);
signals:
    void countChanged();
    void contentsChanged();
private:
    struct Oran7Entry {
        Oran7Track track;
        QString url, source, trackId;
        qint64 durationMs = -1;
        bool selected = false;
        bool animationPending = false;
        bool liked = false;
        bool integrated = false;
    };
    static std::optional<Oran7Entry> entryFromMap(const QVariantMap& fields);
    static QVariantMap serialize(const Oran7Entry& entry);
    void rebuildKeys();
    QVector<Oran7Entry> m_entries;
    QSet<QString> m_keys;
};
