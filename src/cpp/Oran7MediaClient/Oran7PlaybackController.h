#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <functional>
#include <memory>

#include "Audio/Oran7AudioParameters.h"

class Oran7MediaPlayer;
class Oran7PlaybackState;
class Oran7AudioParameterQueue;
struct Frame;
struct AVFrame;
struct ID3D11Device;

// GUI 线程独占播放器；消息线程只向共享邮箱投递值，不持有 Controller/Client。
class Oran7PlaybackController : public QObject
{
    Q_OBJECT
public:
    using FrameCallback = std::function<int(const Frame*, AVFrame*)>;
    struct Oran7RenderHooks {
        std::function<FrameCallback()> beginPlayback;
        std::function<void()> stopPlayback;
        std::function<ID3D11Device*()> device;
    };
    Oran7PlaybackController(Oran7PlaybackState& state, Oran7RenderHooks render);
    ~Oran7PlaybackController() override;

    void prepare(QString source, bool musicPlayback = true);
    void toggle();
    void stop();
    // 兼容 stopRequested：内部切歌停止旧实例时保留新目标和延迟起播。
    void stopCurrent();
    void shutdown();
    int seekTo(int seconds);
    void setVolume(int volume);
    void setDevice(ID3D11Device* device);
    void setFocusedSource(const QString& source);

    // 音效参数：GUI 线程发布到当前播放器的队列；播放器退役后旧队列不再接收新值。
    bool publishAudioParameters(const Oran7AudioParameters& parameters);
    std::shared_ptr<Oran7AudioParameterQueue> audioParameterQueue() const;
    using AudioParameterProvider = std::function<Oran7AudioParameters()>;
    void setAudioParameterProvider(AudioParameterProvider provider);

    QString source() const { return m_source; }
    QString playingSource() const;
    bool hasPlayer() const { return bool(m_player); }
    bool isShuttingDown() const { return m_shutdown; }
    void whenPlayersReleased(std::function<void()> callback);

signals:
    void mediaAccepted(const QString& source, bool musicPlayback);
    void playOrPauseRequested();
    void stopRequested();
    void stopped(bool switching);
    void stopIconUpdated();
    void playNextTriggered();
    void openFailed();
    void playProgressUpdated(int slider, double seconds);
    void totalDurationUpdated(int seconds);

private:
    struct Oran7EventMailbox;
    static int messageLoop(Oran7MediaPlayer* player,
                           const std::shared_ptr<Oran7EventMailbox>& mailbox, quint64 generation);
    void acceptEvent(quint64 generation, int event, qint64 value);
    void updatePosition();
    void retirePlayer(bool pauseBeforeRelease = true);
    void releaseCallbacks();
    void sampleMemory(const char* stage);

    Oran7PlaybackState& m_state; // Client 持有，生命周期覆盖本服务。
    Oran7RenderHooks m_render;
    std::unique_ptr<Oran7MediaPlayer> m_player;
    std::shared_ptr<Oran7AudioParameterQueue> m_audioQueue;
    AudioParameterProvider m_audioProvider;
    std::shared_ptr<Oran7EventMailbox> m_mailbox;
    QTimer m_progressTimer;
    QTimer m_switchTimer;
    QElapsedTimer m_callTimer;
    QElapsedTimer m_memoryTimer;
    QString m_source;
    quint64 m_generation = 0;
    quint64 m_commandRevision = 0;
    bool m_pendingSwitch = false;
    bool m_pauseWhenPrepared = false;
    bool m_shutdown = false;
    int m_retiringPlayers = 0;
    QList<std::function<void()>> m_afterPlayersReleased;
};
