#pragma once

#include <QObject>
#include <QString>

#include "Oran7Track.h"

// GUI 线程持有的播放快照；QML 只读，播放命令仍经 Oran7MediaClient 下发。
class Oran7PlaybackState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(int mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(Oran7Track currentTrack READ currentTrack NOTIFY currentTrackChanged)
public:
    enum State { Idle, Opening, Playing, Paused, Seeking, Ended, Error };
    Q_ENUM(State)
    explicit Oran7PlaybackState(QObject* parent = nullptr) : QObject(parent) {}

    State state() const { return m_state; }
    // playing 表示内核已开始播放；active 还包含打开意图，供旧播放按钮兼容。
    bool playing() const { return m_state == Playing
        || (m_state == Seeking && m_resumeState == Playing); }
    bool active() const { return m_state == Opening || playing(); }
    qint64 positionMs() const { return m_positionMs; }
    qint64 durationMs() const { return m_durationMs; }
    int volume() const { return m_volume; }
    int mode() const { return m_mode; }
    QString source() const { return m_source; }
    QString errorString() const { return m_error; }
    Oran7Track currentTrack() const { return m_currentTrack; }
    quint64 trackRevision() const { return m_trackRevision; }
    void selectTrack(Oran7Track track);
    void setCurrentTrack(Oran7Track track);
    bool completeTrackDetails(quint64 revision, const QString& album, const QString& timesize);

    void beginOpening(const QString& source);
    void setState(State state);
    void beginSeek(qint64 positionMs);
    void finishSeek();
    void setPosition(qint64 positionMs);
    void setDuration(qint64 durationMs);
    void setVolume(int volume);
    void setMode(int mode);
    void setError(const QString& error);

signals:
    void stateChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void modeChanged();
    void sourceChanged();
    void errorChanged();
    void currentTrackChanged();

private:
    State m_state = Idle;
    State m_resumeState = Playing;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    int m_volume = 25;
    int m_mode = 0;
    QString m_source;
    QString m_error;
    Oran7Track m_currentTrack;
    quint64 m_trackRevision = 0;
};
