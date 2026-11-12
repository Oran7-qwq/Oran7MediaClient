#include "Oran7TrackLibrary.h"
#include <QQmlEngine>
#include <QScopedValueRollback>

Oran7TrackLibrary::Oran7TrackLibrary(QObject* parent) : QObject(parent),
    m_local(std::make_unique<Oran7TrackListModel>()),
    m_integrated(std::make_unique<Oran7TrackListModel>()),
    m_liked(std::make_unique<Oran7TrackListModel>()),
    m_search(std::make_unique<Oran7TrackListModel>()),
    m_recommendations(std::make_unique<Oran7TrackListModel>())
{
    for (auto* model : {local(), integrated(), liked(), search(), recommendations()}) {
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        connect(model, &Oran7TrackListModel::contentsChanged, this, &Oran7TrackLibrary::refreshMembership);
    }
    connect(integrated(), &Oran7TrackListModel::contentsChanged, this, [this] {
        if (!m_restoring) emit integratedMusicChanged(integratedTracks());
    });
}
void Oran7TrackLibrary::refreshMembership()
{
    const auto likes = liked()->songIds();
    const auto integrations = integrated()->identityKeys();
    for (auto* model : {local(), integrated(), liked(), search(), recommendations()}) model->updateMembership(likes, integrations);
}
void Oran7TrackLibrary::replaceLocalTracks(const QVariantList& tracks) { local()->replaceTracks(tracks); }
void Oran7TrackLibrary::restoreIntegratedTracks(const QVariantList& tracks)
{
    QScopedValueRollback<bool> restoring(m_restoring, true);
    integrated()->replaceTracks(tracks);
}
QVariantList Oran7TrackLibrary::integratedTracks() const { return integrated()->serializedTracks(); }
bool Oran7TrackLibrary::isIntegrated(qint64 songId, const QString& url, const QString& path) const
{
    return integrated()->contains(songId, url, path);
}
bool Oran7TrackLibrary::isLiked(qint64 songId) const { return songId > 0 && liked()->contains(songId); }
void Oran7TrackLibrary::addIntegratedTrack(const QVariantMap& track) { addIntegratedTracks({track}); }
void Oran7TrackLibrary::addIntegratedTracks(const QVariantList& tracks)
{
    QVariantList normalized;
    normalized.reserve(tracks.size());
    for (const auto& value : tracks) {
        auto fields = value.toMap();
        // VIP 临时链接不入库，恢复与播放仍按 songId 重新解析。
        if (fields.value("isVip").toBool()) {
            fields["url"] = QString(); fields["filepath"] = QString(); fields["path"] = QString();
        }
        normalized.append(fields);
    }
    integrated()->prependTracks(normalized);
}
bool Oran7TrackLibrary::removeIntegratedTrack(qint64 songId, const QString& url, const QString& path)
{
    return integrated()->removeMatching(songId, url, path);
}
void Oran7TrackLibrary::addLikedTrack(const QVariantMap& track)
{
    if (track.value("songId").toLongLong() > 0) liked()->prependTracks({track});
}
bool Oran7TrackLibrary::removeLikedTrack(qint64 songId) { return songId > 0 && liked()->removeMatching(songId, {}, {}); }
void Oran7TrackLibrary::replaceLikedTracks(const QVariantList& tracks) { liked()->replaceTracks(tracks); }
void Oran7TrackLibrary::clearLikedTracks() { liked()->replaceTracks({}); }
void Oran7TrackLibrary::replaceSearchResults(const QVariantList& tracks) { search()->replaceTracks(tracks); }
void Oran7TrackLibrary::clearSearchResults() { search()->replaceTracks({}); }
void Oran7TrackLibrary::replaceRecommendations(const QVariantList& tracks) { recommendations()->replaceTracks(tracks); }
