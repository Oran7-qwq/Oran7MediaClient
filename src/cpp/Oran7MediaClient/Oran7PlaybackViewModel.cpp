#include "Oran7PlaybackViewModel.h"
#include "Oran7TrackLibrary.h"
#include "Oran7PlaybackState.h"
#include "Oran7CloudMusicService.h"
#include <QRandomGenerator>
#include <algorithm>

Oran7PlaybackViewModel::Oran7PlaybackViewModel(Oran7TrackLibrary& tracks,
    Oran7PlaybackState& playback, Oran7CloudMusicService& cloud)
    : m_tracks(tracks), m_playback(playback), m_cloud(cloud), m_roamFilter(oran7MakeRoamFilterStrategy(false))
{
    connect(m_tracks.liked(), &Oran7TrackListModel::contentsChanged, this, [this] {
        if (m_roamFilterLiked) prunePendingRoam();
    });
    connect(&cloud, &Oran7CloudMusicService::likedStatusChanged, this, [this] {
        if (!m_waitingForLikes || m_cloud.likedFetching()) return;
        m_waitingForLikes = false;
        if (m_cloud.likedLoaded()) requestRoam(m_waiting);
        else if (m_waiting) { m_waiting = false; emit stopRequested(); }
    });
    connect(&playback, &Oran7PlaybackState::modeChanged, this, [this] {
        if (m_playback.mode() != 3) clearRoam();
    });
    connect(&cloud, &Oran7CloudMusicService::sessionChanged, this, [this] {
        ++m_commandRevision; clearRoam(); m_failures = 0;
    });
    connect(&cloud, &Oran7CloudMusicService::roamReady, this,
        [this](quint64 request, const QVariantList& songs) {
            if (!m_enabled || m_playback.mode() != 3 || request != m_roamRequest) return;
            m_roamRequest = 0;
            QVariantList accepted;
            accepted.reserve(songs.size());
            for (const auto& song : songs) if (acceptsRoam(song)) accepted.append(song);
            if (accepted.isEmpty()) {
                // 连续三批为空即停止补取，不放回已喜欢歌曲、不无限刷推荐接口。
                ++m_filteredBatches;
                requestRoam(m_waiting);
                return;
            }
            m_filteredBatches = 0;
            const int keepFrom = std::max(0, m_roamIndex - 2);
            m_roam = m_roam.mid(keepFrom); m_roamIndex -= keepFrom;
            m_roam.append(accepted);
            if (m_waiting) { m_waiting = false; nextRoam(); }
        });
    connect(&cloud, &Oran7CloudMusicService::roamFailed, this, [this](quint64 request) {
        if (request != m_roamRequest) return;
        m_roamRequest = 0;
        // 预取失败不打断当前歌曲；耗尽后才能停止。
        if (m_waiting) { m_waiting = false; emit stopRequested(); }
    });
}
bool Oran7PlaybackViewModel::acceptsRoam(const QVariant& song) const
{
    return m_roamFilter->accepts(song.toMap().value("songId").toLongLong(), m_tracks);
}
void Oran7PlaybackViewModel::prunePendingRoam()
{
    // 历史/当前曲目保留；只清理尚未消费的预缓存，不中断正在播放的歌。
    auto kept = m_roam.mid(0, m_roamIndex);
    for (int i = m_roamIndex; i < m_roam.size(); ++i)
        if (acceptsRoam(m_roam.at(i))) kept.append(m_roam.at(i));
    m_roam = std::move(kept);
}
void Oran7PlaybackViewModel::setRoamFilterLiked(bool enabled)
{
    if (m_closed || m_roamFilterLiked == enabled) return;
    m_roamFilterLiked = enabled;
    m_roamFilter = oran7MakeRoamFilterStrategy(enabled);
    m_filteredBatches = 0;
    prunePendingRoam();
    emit roamFilterLikedChanged();
}
bool Oran7PlaybackViewModel::owns(Oran7TrackListModel* model) const
{
    return model && (model == m_tracks.local() || model == m_tracks.integrated()
        || model == m_tracks.liked() || model == m_tracks.search());
}
void Oran7PlaybackViewModel::toggleCurrent()
{
    if (m_closed) return;
    const auto track = m_playback.currentTrack();
    if (track.path.isEmpty() && track.songId <= 0) return;
    const auto command = ++m_commandRevision;
    m_musicSuppressed = false;
    m_failures = 0;
    adoptCurrent();
    if (command != m_commandRevision) return;
    // 用户在耗尽等待时主动重新起播，旧推荐结果不能随后抢占；普通暂停保留漫游历史。
    if (m_waiting) clearRoam();
    emit musicActivated();
    if (command != m_commandRevision) return;
    playTrack({{"path",track.path},{"songId",track.songId},{"title",track.title},
        {"artist",track.artist},{"album",track.album},{"cover",track.cover},
        {"timesize",track.timesize},{"isVip",track.isVip}}, false);
}
bool Oran7PlaybackViewModel::activate(Oran7TrackListModel* model, int index)
{
    if (m_closed || !owns(model) || index < 0 || index >= model->count()) return false;
    const auto command = ++m_commandRevision;
    clearRoam(); m_failures = 0; m_enabled = true;
    m_musicSuppressed = false;
    m_owner = model == m_tracks.local() || model == m_tracks.integrated() ? "local" : "search";
    emit ownerChanged();
    if (command != m_commandRevision) return false;
    emit musicActivated();
    if (command != m_commandRevision) return false;
    playRow(model, index, false);
    return true;
}
void Oran7PlaybackViewModel::playRow(Oran7TrackListModel* model, int row, bool restart)
{
    if (!owns(model) || row < 0 || row >= model->count()) return;
    m_model = model; m_anchor = row;
    auto fields = model->get(row);
    m_trackId = fields.value("trackId").toString();
    const auto url = fields.value("url").toString();
    if (!url.isEmpty()) fields.insert("path", url);
    playTrack(fields, restart);
}
void Oran7PlaybackViewModel::playTrack(QVariantMap fields, bool restart)
{
    const auto track = Oran7Track::fromVariantMap(fields);
    if (!track || (track->path.isEmpty() && track->songId <= 0)) { emit stopRequested(); return; }
    emit trackRequested(fields, restart);
}
void Oran7PlaybackViewModel::clearRoam()
{
    m_cloud.cancelRoam(); m_roamRequest = 0; m_waiting = false;
    m_roam.clear(); m_roamIndex = 0;
    m_waitingForLikes = false; m_filteredBatches = 0;
}
void Oran7PlaybackViewModel::cancel(bool leaveMusic)
{
    ++m_commandRevision;
    clearRoam(); m_failures = 0;
    if (leaveMusic) { m_musicSuppressed = true; m_enabled = false; m_model = nullptr; m_trackId.clear(); m_owner.clear(); emit ownerChanged(); }
}
void Oran7PlaybackViewModel::requestRoam(bool waiting)
{
    m_waiting = waiting;
    if (m_roamRequest) return;
    if (m_filteredBatches >= 3) {
        if (waiting) { m_waiting = false; emit stopRequested(); }
        return;
    }
    if (m_roamFilterLiked && !m_cloud.likedLoaded()) {
        m_waitingForLikes = true;
        m_cloud.fetchLiked();
        // 拒绝提交/读取失败没有后续异步通知，结束本次等待。
        if (m_waitingForLikes && !m_cloud.likedFetching()) {
            m_waitingForLikes = false;
            if (waiting) { m_waiting = false; emit stopRequested(); }
        }
        return;
    }
    m_roamRequest = m_cloud.requestRoam(m_playback.currentTrack().songId);
    if (!m_roamRequest && waiting) { m_waiting = false; emit stopRequested(); }
}
void Oran7PlaybackViewModel::nextRoam()
{
    prunePendingRoam();
    if (m_roamIndex < m_roam.size()) {
        const auto command = m_commandRevision;
        const auto fields = m_roam.at(m_roamIndex++).toMap();
        playTrack(fields, true);
        if (command != m_commandRevision) return;
        if (m_roam.size() >= 2 && m_roam.size() - m_roamIndex <= m_roam.size() / 2)
            requestRoam(false);
    } else requestRoam(true);
}
void Oran7PlaybackViewModel::adoptCurrent()
{
    if (!m_enabled && !m_musicSuppressed) {
        // 启动恢复曲目尚未点击列表时，按稳定身份找回归属。
        const auto track = m_playback.currentTrack();
        for (auto* model : {m_tracks.local(), m_tracks.integrated(), m_tracks.liked(), m_tracks.search()}) {
            for (int row = 0; row < model->count(); ++row) {
                const auto fields = model->get(row);
                const bool matches = track.songId > 0 ? fields.value("songId").toLongLong() == track.songId
                    : !track.path.isEmpty() && (fields.value("path").toString() == track.path || fields.value("url").toString() == track.path);
                if (!matches) continue;
                m_model = model; m_anchor = row; m_trackId = fields.value("trackId").toString();
                m_owner = model == m_tracks.local() || model == m_tracks.integrated() ? "local" : "search";
                m_enabled = true;
                const auto command = m_commandRevision;
                emit ownerChanged();
                if (command != m_commandRevision) return;
                break;
            }
            if (m_enabled) break;
        }
    }
}
void Oran7PlaybackViewModel::advance(int direction)
{
    if (m_closed) return;
    adoptCurrent();
    if (!m_enabled) return;
    ++m_commandRevision;
    m_failures = 0;
    if (m_owner == "search" && !m_cloud.loggedIn()) { emit stopRequested(); return; }
    if (m_owner == "search" && m_playback.mode() == 3) {
        if (direction > 0) nextRoam();
        else if (m_roamIndex > 0 && !m_roam.isEmpty()) {
            m_roamIndex = std::max(0, m_roamIndex - 2);
            playTrack(m_roam.at(m_roamIndex++).toMap(), true);
        }
        return;
    }
    if (!m_model || !m_model->count()) return;
    const int count = m_model->count();
    const int current = m_model->indexOfTrack(m_trackId);
    int row;
    if (current < 0) row = std::clamp(m_anchor, 0, count - 1);
    else if (m_playback.mode() == 1) row = current;
    else if (m_playback.mode() == 2 && count > 1)
        row = (current + 1 + QRandomGenerator::global()->bounded(count - 1)) % count;
    else row = (current + (direction > 0 ? 1 : count - 1)) % count;
    playRow(m_model, row, true);
}
void Oran7PlaybackViewModel::resolveFailed(qint64 songId)
{
    if (!m_enabled || songId != m_playback.currentTrack().songId) return;
    ++m_commandRevision;
    if (m_owner == "search" && !m_cloud.loggedIn()) { emit stopRequested(); return; }
    const bool roam = m_owner == "search" && m_playback.mode() == 3;
    const int limit = roam ? 32 : (m_model ? m_model->count() : 0);
    if (++m_failures >= limit || limit <= 1) { clearRoam(); emit stopRequested(); return; }
    if (roam) { nextRoam(); return; }
    int row = m_model->indexOfTrack(m_trackId);
    row = row < 0 ? std::clamp(m_anchor, 0, m_model->count() - 1) : (row + 1) % m_model->count();
    playRow(m_model, row, true);
}
