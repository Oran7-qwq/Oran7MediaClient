#include "Oran7CloudMusicService.h"
#include "Oran7TrackLibrary.h"
#include "Oran7Async.h"
#include <utility>

namespace {
struct Oran7CloudResult { bool ok = false; QVariant value; };
// 后台只持有凭据快照和无 QObject 的请求函数，异常不携带凭据返回界面。
Oran7CloudResult requestValue(const Oran7CloudMusicService::Oran7Hooks& hooks,
                            const QString& operation, const QVariantMap& args)
{
    try { return {true, hooks.request(operation, args)}; } catch (...) { return {}; }
}
}
Oran7CloudMusicService::Oran7CloudMusicService(Oran7TrackLibrary& tracks, Oran7Hooks hooks)
    : m_tracks(tracks), m_hooks(std::move(hooks)) {}
Oran7CloudMusicService::~Oran7CloudMusicService() { shutdown(); }
bool Oran7CloudMusicService::loggedIn() const
{ return !m_closed && m_hooks.credentials().value("isLoggedIn").toBool(); }
bool Oran7CloudMusicService::sameSession(const QString& cookie) const
{
    const auto credentials = m_hooks.credentials();
    return !m_closed && credentials.value("isLoggedIn").toBool()
        && credentials.value("cookie").toString() == cookie;
}
bool Oran7CloudMusicService::updateSession()
{
    if (m_closed) return false;
    const auto credentials = m_hooks.credentials();
    const bool online = credentials.value("isLoggedIn").toBool();
    const auto cookie = online ? credentials.value("cookie").toString() : QString();
    if (cookie != m_cookie) {
        ++m_session; m_likedTask.cancel(); cancelRoam(); m_cookie = cookie;
        m_fetching = m_loaded = m_dirty = false;
        m_loadedAt.invalidate(); m_likeAt.invalidate();
        m_playlistId = 0; m_hooks.setPlaylistId(0);
        m_tracks.clearLikedTracks();
        emit likedStatusChanged(); emit sessionChanged();
    }
    return online;
}
void Oran7CloudMusicService::syncSession() { if (updateSession()) fetchLiked(); }
void Oran7CloudMusicService::shutdown()
{
    m_closed = true; ++m_session; m_likedTask.cancel(); cancelRoam();
}
void Oran7CloudMusicService::markLikedDirty()
{ if (!m_closed) { m_dirty = true; emit likedStatusChanged(); } }
bool Oran7CloudMusicService::fetchLiked(bool force)
{
    if (!updateSession() || m_fetching) return false;
    if (!force && m_loaded && !m_dirty && m_loadedAt.isValid() && m_loadedAt.elapsed() < 30000) return false;
    m_fetching = true; m_dirty = false;
    const auto session = m_session;
    const auto cookie = m_cookie;
    const auto args = m_hooks.credentials();
    // 不把 GUI 回调/会话读取函数复制到工作线程。
    Oran7Hooks worker; worker.request = m_hooks.request;
    emit likedStatusChanged();
    m_likedTask = Oran7Async::run(this,
        [worker, args] { return requestValue(worker, QStringLiteral("fetchLikedSongs"), args); },
        [this, session, cookie](const Oran7CloudResult& result) {
            if (session != m_session || !sameSession(cookie)) return;
            m_fetching = false;
            if (result.ok && result.value.isValid() && !result.value.isNull()) {
                const auto values = result.value.toMap();
                m_loaded = true; m_loadedAt.start();
                m_playlistId = values.value("playlistId").toLongLong();
                m_hooks.setPlaylistId(m_playlistId);
                // 拉取期间点赞会置脏，不能用快照覆盖新修改。
                if (!m_dirty) m_tracks.replaceLikedTracks(values.value("songs").toList());
                emit likedStatusChanged();
                emit likedFetchFinished(m_tracks.liked()->count());
            } else { m_dirty = true; emit likedStatusChanged(); }
        }, Oran7Async::Group::Network);
    if (!m_likedTask.isValid()) { m_fetching = false; m_dirty = true; emit likedStatusChanged(); return false; }
    return true;
}
bool Oran7CloudMusicService::toggleLike(const QVariantMap& track)
{
    if (!updateSession()) return false;
    const auto id = track.value("songId").toLongLong();
    if (id <= 0 || (m_likeAt.isValid() && m_likeAt.elapsed() < 2000)) return false;
    m_likeAt.start();
    const bool liked = !m_tracks.isLiked(id);
    markLikedDirty();
    if (liked) m_tracks.addLikedTrack(track); else m_tracks.removeLikedTrack(id);
    m_hooks.setLike(id, liked);
    emit likedChanged(id, liked);
    return true;
}
void Oran7CloudMusicService::cancelRoam() { ++m_roamRevision; m_roamTask.cancel(); }
quint64 Oran7CloudMusicService::requestRoam(qint64 seed, int mode)
{
    if (mode < 3 || mode > 6 || !updateSession() || (mode == 3 && seed <= 0)) return 0;
    cancelRoam();
    const auto revision = m_roamRevision, session = m_session;
    const auto cookie = m_cookie;
    auto args = m_hooks.credentials();
    args.insert("songId", seed); args.insert("playlistId", m_playlistId);
    args.insert("recommendationMode", mode);
    Oran7Hooks worker; worker.request = m_hooks.request;
    m_roamTask = Oran7Async::run(this,
        [worker, args] { return requestValue(worker, QStringLiteral("roamSongs"), args); },
        [this, revision, session, cookie](const Oran7CloudResult& result) {
            if (revision != m_roamRevision || session != m_session || !sameSession(cookie)) return;
            if (result.ok && !result.value.toList().isEmpty()) emit roamReady(revision, result.value.toList());
            else emit roamFailed(revision);
        }, Oran7Async::Group::Network);
    return m_roamTask.isValid() ? revision : 0;
}
