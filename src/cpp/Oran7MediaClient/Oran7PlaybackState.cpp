#include "Oran7PlaybackState.h"
#include <QThread>
#include <utility>

void Oran7PlaybackState::selectTrack(Oran7Track track)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const bool sameTrack = track.songId > 0 ? track.songId == m_currentTrack.songId
        : m_currentTrack.songId == 0 && !track.path.isEmpty() && track.path == m_currentTrack.path;
    // 同一首暂停/继续时，列表里的缺失字段不能清掉已经补全的详情。
    if (sameTrack) {
        if (track.title.isEmpty()) track.title = m_currentTrack.title;
        if (track.artist.isEmpty()) track.artist = m_currentTrack.artist;
        if (track.album.isEmpty()) track.album = m_currentTrack.album;
        if (track.cover.isEmpty()) track.cover = m_currentTrack.cover;
        if (track.timesize.isEmpty()) track.timesize = m_currentTrack.timesize;
    }
    setCurrentTrack(std::move(track));
}

void Oran7PlaybackState::setCurrentTrack(Oran7Track track)
{
    Q_ASSERT(QThread::currentThread() == thread());
    track.songId = qMax(qint64(0), track.songId);
    if (m_currentTrack == track) return;
    m_currentTrack = std::move(track);
    ++m_trackRevision;
    emit currentTrackChanged();
}

bool Oran7PlaybackState::completeTrackDetails(quint64 revision, const QString& album, const QString& timesize)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (revision != m_trackRevision) return false;
    auto track = m_currentTrack;
    if (track.album.isEmpty()) track.album = album;
    if (track.timesize.isEmpty()) track.timesize = timesize;
    setCurrentTrack(track);
    return true;
}

void Oran7PlaybackState::beginOpening(const QString& source)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_source != source) { m_source = source; emit sourceChanged(); }
    if (!m_error.isEmpty()) { m_error.clear(); emit errorChanged(); }
    setPosition(0);
    setDuration(0);
    setState(Opening);
}

void Oran7PlaybackState::setState(State state)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_state == state) return;
    m_state = state;
    emit stateChanged();
}

void Oran7PlaybackState::beginSeek(qint64 positionMs)
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_resumeState = active() ? Playing : Paused;
    setPosition(positionMs);
    setState(Seeking);
}

void Oran7PlaybackState::finishSeek()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_state == Seeking) setState(m_resumeState);
}

void Oran7PlaybackState::setPosition(qint64 positionMs)
{
    Q_ASSERT(QThread::currentThread() == thread());
    positionMs = qMax(qint64(0), positionMs);
    if (m_durationMs > 0) positionMs = qMin(positionMs, m_durationMs);
    if (m_positionMs == positionMs) return;
    m_positionMs = positionMs;
    emit positionChanged();
}

void Oran7PlaybackState::setDuration(qint64 durationMs)
{
    Q_ASSERT(QThread::currentThread() == thread());
    durationMs = qMax(qint64(0), durationMs);
    if (m_durationMs == durationMs) return;
    m_durationMs = durationMs;
    setPosition(m_positionMs);
    emit durationChanged();
}

void Oran7PlaybackState::setVolume(int volume)
{
    Q_ASSERT(QThread::currentThread() == thread());
    volume = qBound(0, volume, 100);
    if (m_volume == volume) return;
    m_volume = volume;
    emit volumeChanged();
}

void Oran7PlaybackState::setMode(int mode)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (mode < 0 || mode > 6 || m_mode == mode) return;
    m_mode = mode;
    emit modeChanged();
}

void Oran7PlaybackState::setError(const QString& error)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_error != error) { m_error = error; emit errorChanged(); }
    setState(Error);
}
