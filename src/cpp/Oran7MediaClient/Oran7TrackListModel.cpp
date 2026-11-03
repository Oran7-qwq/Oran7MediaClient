#include "Oran7TrackListModel.h"
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QUrl>
#include <cmath>
#include <limits>

namespace {
QString sourceKey(const QString& source)
{
    if (source.isEmpty()) return {};
    const QUrl url(source);
    if (url.isLocalFile() || !source.contains("://")) {
        const auto path = url.isLocalFile() ? url.toLocalFile() : source;
        return "p:" + QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toCaseFolded();
    }
    return "u:" + source;
}
}

Oran7TrackListModel::Oran7TrackListModel(QObject* parent) : QAbstractListModel(parent) {}
int Oran7TrackListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : count(); }
QHash<int, QByteArray> Oran7TrackListModel::roleNames() const
{
    return {{TrackIdRole,"trackId"},{UrlRole,"url"},{TitleRole,"title"},{ArtistRole,"artist"},
        {AlbumRole,"album"},{CoverRole,"cover"},{DurationMsRole,"durationMs"},{IsVipRole,"isVip"},
        {IsLikedRole,"isLiked"},{IsIntegratedRole,"isIntegrated"},{PathRole,"path"},{SongIdRole,"songId"},
        {SourceRole,"source"},{IsSelectedRole,"isSelected"},{NeedsAnimateInRole,"needsAnimateIn"},
        {IconRole,"icon"},{MusicNameRole,"music_name"},{MusicArtistRole,"music_artist"},
        {MusicAlbumRole,"music_album"},{TimesizeRole,"timesize"},{FilepathRole,"filepath"},{IsLovedRole,"isLoved"}};
}
QVariant Oran7TrackListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0 || index.row() >= count()) return {};
    const auto& entry = m_entries[index.row()];
    const auto& track = entry.track;
    switch (role) {
    case TrackIdRole: return entry.trackId;
    case UrlRole: return entry.url;
    case TitleRole: case MusicNameRole: return track.title;
    case ArtistRole: case MusicArtistRole: return track.artist;
    case AlbumRole: case MusicAlbumRole: return track.album;
    case CoverRole: case IconRole: return track.cover;
    case TimesizeRole: return track.timesize;
    case DurationMsRole: {
        if (entry.durationMs >= 0) return QVariant::fromValue(entry.durationMs);
        bool ok = false;
        const double seconds = track.timesize.toDouble(&ok);
        return QVariant::fromValue<qint64>(ok && std::isfinite(seconds) && seconds > 0
            && seconds < double(std::numeric_limits<qint64>::max() / 1000) ? qint64(seconds * 1000) : 0);
    }
    case IsVipRole: return track.isVip;
    case IsLikedRole: case IsLovedRole: return entry.liked;
    case IsIntegratedRole: return entry.integrated;
    case PathRole: case FilepathRole: return track.path;
    case SongIdRole: return track.songId;
    case SourceRole: return entry.source;
    case IsSelectedRole: return entry.selected;
    case NeedsAnimateInRole: return entry.animationPending;
    default: return {};
    }
}
int Oran7TrackListModel::indexOfSongId(qint64 songId) const
{
    if (songId <= 0) return -1;
    for (int row = 0; row < count(); ++row)
        if (m_entries[row].track.songId == songId) return row;
    return -1;
}
int Oran7TrackListModel::indexOfSource(const QString& source) const
{
    if (source.isEmpty()) return -1;
    for (int row = 0; row < count(); ++row)
        if (m_entries[row].track.path == source || m_entries[row].url == source) return row;
    return -1;
}
QVariantMap Oran7TrackListModel::get(int row) const
{
    if (row < 0 || row >= count()) return {};
    QVariantMap result;
    const auto roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        result.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    return result;
}
QStringList Oran7TrackListModel::identityKeys(qint64 songId, const QString& url, const QString& path)
{
    QStringList keys;
    if (songId > 0) keys.append("s:" + QString::number(songId));
    for (const auto& source : {url, path}) {
        const auto key = sourceKey(source);
        if (!key.isEmpty() && !keys.contains(key)) keys.append(key);
    }
    return keys;
}
std::optional<Oran7TrackListModel::Oran7Entry> Oran7TrackListModel::entryFromMap(const QVariantMap& fields)
{
    QVariantMap normalized = fields;
    const QPair<const char*, const char*> aliases[] = {{"path","filepath"},{"title","music_name"},
        {"artist","music_artist"},{"album","music_album"},{"cover","icon"}};
    for (const auto& pair : aliases)
        if (!normalized.contains(pair.first)) normalized.insert(pair.first, fields.value(pair.second));
    if (!normalized.contains("timesize") && fields.contains("durationMs"))
        normalized.insert("timesize", QString::number(fields.value("durationMs").toLongLong() / 1000.0, 'f', 3));
    const auto track = Oran7Track::fromVariantMap(normalized);
    if (!track) return std::nullopt;
    Oran7Entry entry;
    entry.track = *track;
    if (fields.contains("durationMs")) {
        bool valid = false;
        const auto duration = fields.value("durationMs").toLongLong(&valid);
        if (valid && duration >= 0) entry.durationMs = duration;
    }
    entry.url = fields.value("url").toString();
    entry.source = fields.value("source").toString();
    const auto keys = identityKeys(track->songId, entry.url, track->path);
    if (keys.isEmpty()) return std::nullopt;
    entry.trackId = keys.first();
    entry.animationPending = fields.value("needsAnimateIn", false).toBool();
    return entry;
}
QVariantMap Oran7TrackListModel::serialize(const Oran7Entry& entry)
{
    const auto& t = entry.track;
    return {{"icon",t.cover},{"music_name",t.title},{"music_artist",t.artist},{"music_album",t.album},
        {"timesize",t.timesize},{"url",entry.url},{"filepath",t.path},{"songId",t.songId},
        {"isVip",t.isVip},{"source",entry.source}};
}
QVariantList Oran7TrackListModel::serializedTracks() const
{
    QVariantList result;
    result.reserve(count());
    for (const auto& entry : m_entries) result.append(serialize(entry));
    return result;
}
QVariantList Oran7TrackListModel::paths() const
{
    QVariantList result;
    result.reserve(count());
    for (const auto& entry : m_entries) result.append(entry.track.path);
    return result;
}
void Oran7TrackListModel::rebuildKeys()
{
    m_keys.clear();
    for (const auto& entry : m_entries)
        for (const auto& key : identityKeys(entry.track.songId, entry.url, entry.track.path)) m_keys.insert(key);
}
bool Oran7TrackListModel::contains(qint64 songId, const QString& url, const QString& path) const
{
    for (const auto& key : identityKeys(songId, url, path)) if (m_keys.contains(key)) return true;
    return false;
}
void Oran7TrackListModel::replaceTracks(const QVariantList& tracks)
{
    Q_ASSERT(QThread::currentThread() == thread());
    QSet<QString> selected;
    for (const auto& entry : m_entries) if (entry.selected) selected.insert(entry.trackId);
    QVector<Oran7Entry> entries;
    QSet<QString> keys;
    for (const auto& value : tracks) {
        auto entry = entryFromMap(value.toMap());
        if (!entry) continue;
        const auto identities = identityKeys(entry->track.songId, entry->url, entry->track.path);
        bool duplicate = false;
        for (const auto& key : identities) if (keys.contains(key)) { duplicate = true; break; }
        if (duplicate) continue;
        for (const auto& key : identities) keys.insert(key);
        entry->selected = selected.contains(entry->trackId);
        entries.append(std::move(*entry));
    }
    beginResetModel();
    m_entries = std::move(entries);
    m_keys = std::move(keys);
    endResetModel();
    emit contentsChanged();
    emit countChanged(); // 同数量重建也需要按身份重新定位焦点。
}
void Oran7TrackListModel::prependTracks(const QVariantList& tracks)
{
    Q_ASSERT(QThread::currentThread() == thread());
    QVector<Oran7Entry> additions;
    auto keys = m_keys;
    for (const auto& value : tracks) {
        auto entry = entryFromMap(value.toMap());
        if (!entry) continue;
        const auto identities = identityKeys(entry->track.songId, entry->url, entry->track.path);
        bool duplicate = false;
        for (const auto& key : identities) if (keys.contains(key)) { duplicate = true; break; }
        if (duplicate) continue;
        for (const auto& key : identities) keys.insert(key);
        additions.append(std::move(*entry));
    }
    if (additions.isEmpty()) return;
    beginInsertRows({}, 0, additions.size() - 1);
    m_entries = additions + m_entries;
    m_keys = std::move(keys);
    endInsertRows();
    emit contentsChanged();
    emit countChanged();
}
bool Oran7TrackListModel::removeMatching(qint64 songId, const QString& url, const QString& path)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto keys = identityKeys(songId, url, path);
    for (int row = 0; row < count(); ++row) {
        bool hit = false;
        for (const auto& key : identityKeys(m_entries[row].track.songId, m_entries[row].url, m_entries[row].track.path))
            if (keys.contains(key)) { hit = true; break; }
        if (!hit) continue;
        beginRemoveRows({}, row, row);
        m_entries.removeAt(row);
        rebuildKeys();
        endRemoveRows();
        emit contentsChanged();
        emit countChanged();
        return true;
    }
    return false;
}
bool Oran7TrackListModel::setSelected(int row, bool selected)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (row < 0 || row >= count()) return false;
    if (m_entries[row].selected == selected) return true;
    m_entries[row].selected = selected;
    emit dataChanged(index(row), index(row), {IsSelectedRole});
    return true;
}
bool Oran7TrackListModel::setAnimationPending(int row, bool pending)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (row < 0 || row >= count()) return false;
    if (m_entries[row].animationPending == pending) return true;
    m_entries[row].animationPending = pending;
    emit dataChanged(index(row), index(row), {NeedsAnimateInRole});
    return true;
}
bool Oran7TrackListModel::moveTrack(int from, int to)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (from < 0 || to < 0 || from >= count() || to >= count()) return false;
    if (from == to) return true;
    if (!beginMoveRows({}, from, from, {}, from < to ? to + 1 : to)) return false;
    m_entries.move(from, to);
    endMoveRows();
    emit contentsChanged();
    return true;
}
int Oran7TrackListModel::indexOfTrack(const QString& trackId) const
{
    for (int row = 0; row < count(); ++row) if (m_entries[row].trackId == trackId) return row;
    return -1;
}
QSet<qint64> Oran7TrackListModel::songIds() const
{
    QSet<qint64> ids;
    for (const auto& entry : m_entries) if (entry.track.songId > 0) ids.insert(entry.track.songId);
    return ids;
}
void Oran7TrackListModel::updateMembership(const QSet<qint64>& liked, const QSet<QString>& integrated)
{
    Q_ASSERT(QThread::currentThread() == thread());
    for (int row = 0; row < count(); ++row) {
        auto& entry = m_entries[row];
        const bool isLiked = liked.contains(entry.track.songId);
        bool isIntegrated = false;
        for (const auto& key : identityKeys(entry.track.songId, entry.url, entry.track.path))
            if (integrated.contains(key)) { isIntegrated = true; break; }
        QList<int> roles;
        if (entry.liked != isLiked) { entry.liked = isLiked; roles << IsLikedRole << IsLovedRole; }
        if (entry.integrated != isIntegrated) { entry.integrated = isIntegrated; roles << IsIntegratedRole; }
        if (!roles.isEmpty()) emit dataChanged(index(row), index(row), roles);
    }
}
