#include "Oran7PlaybackController.h"
#include "Oran7PlaybackState.h"
#include "Oran7MediaPlayer.h"
#include "Oran7Async.h"
#include "ffmsg.h"
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <cmath>
#include <thread>
#include <utility>

struct Oran7PlaybackController::Oran7EventMailbox {
    QMutex mutex;
    Oran7PlaybackController* receiver = nullptr; // mutex 保护投递与析构，不拥有对象。

    void post(quint64 generation, int event, qint64 value)
    {
        QMutexLocker lock(&mutex);
        if (!receiver) return;
        auto* target = receiver;
        QMetaObject::invokeMethod(target, [target, generation, event, value] {
            target->acceptEvent(generation, event, value);
        }, Qt::QueuedConnection);
    }
};

Oran7PlaybackController::Oran7PlaybackController(Oran7PlaybackState& state, Oran7RenderHooks render)
    : m_state(state), m_render(std::move(render)), m_mailbox(std::make_shared<Oran7EventMailbox>())
{
    m_mailbox->receiver = this;
    m_callTimer.start();
    m_switchTimer.setSingleShot(true);
    m_switchTimer.setInterval(300);
    connect(&m_switchTimer, &QTimer::timeout, this, [this] {
        if (m_shutdown || !m_pendingSwitch) return;
        if (m_retiringPlayers != 0) return; // 后台退役完成后再排队唤醒，不在 GUI 等待。
        m_pendingSwitch = false;
        emit playOrPauseRequested();
    });
    connect(&m_progressTimer, &QTimer::timeout, this, &Oran7PlaybackController::updatePosition);
    m_progressTimer.start(100);
}

Oran7PlaybackController::~Oran7PlaybackController()
{
    // 必须先失效邮箱，再 join 当前播放器；退役播放器可在服务销毁后继续释放。
    {
        QMutexLocker lock(&m_mailbox->mutex);
        m_mailbox->receiver = nullptr;
    }
    m_shutdown = true;
    m_progressTimer.stop();
    m_switchTimer.stop();
    m_afterPlayersReleased.clear();
    if (m_render.stopPlayback) m_render.stopPlayback();
    m_player.reset();
}

void Oran7PlaybackController::setFocusedSource(const QString& source)
{
    if (!m_shutdown) m_source = source;
}

QString Oran7PlaybackController::playingSource() const
{
    return m_player ? QString::fromStdString(m_player->dataSource()) : QString();
}

void Oran7PlaybackController::prepare(QString source, bool musicPlayback)
{
    if (m_shutdown || source.isEmpty()) return;
    if (!m_pendingSwitch && m_player && m_source == source) {
        emit playOrPauseRequested();
        return;
    }
    if (!m_pendingSwitch && m_player && m_callTimer.elapsed() < 100) return;
    m_callTimer.restart();
    const auto command = ++m_commandRevision;
    m_source = source;
    QPointer<Oran7PlaybackController> guard(this);
    m_state.beginOpening(source);
    if (!guard || m_shutdown || command != m_commandRevision) return;
    emit mediaAccepted(source, musicPlayback);
    if (!guard || m_shutdown || command != m_commandRevision) return;
    if (m_pendingSwitch) return; // 一轮延迟只创建一个实例，总是读取最新目标。
    if (!m_player) {
        emit playOrPauseRequested();
        return;
    }
    m_pendingSwitch = true;
    m_switchTimer.start();
    emit stopRequested();
}

void Oran7PlaybackController::toggle()
{
    if (m_shutdown || m_source.isEmpty()) return;
    ++m_commandRevision;
    if (!m_player && m_retiringPlayers != 0) {
        m_pendingSwitch = true;
        if (!m_switchTimer.isActive()) m_switchTimer.start();
        return;
    }
    // 用户直接播放命令可提前接管切歌，旧定时器不能随后把新实例暂停。
    m_pendingSwitch = false;
    m_switchTimer.stop();
    if (!m_player) {
        const quint64 generation = ++m_generation;
        const auto command = m_commandRevision;
        QPointer<Oran7PlaybackController> guard(this);
        m_state.beginOpening(m_source);
        if (!guard || m_shutdown || command != m_commandRevision) return;
        m_pauseWhenPrepared = false;
        m_player = std::make_unique<Oran7MediaPlayer>();
        int result = m_player->oran7mp_create([mailbox = m_mailbox, generation](void* player) {
            return messageLoop(static_cast<Oran7MediaPlayer*>(player), mailbox, generation);
        });
        if (result == 0) {
            if (m_render.beginPlayback) m_player->AddVideoRefreshCallback(m_render.beginPlayback());
            if (m_render.device) m_player->oran7mp_setD3D11Device(m_render.device());
            result = m_player->oran7mp_set_data_source(m_source.toUtf8().constData());
            if (result == 0) result = m_player->oran7mp_prepare_async();
        }
        if (result < 0) {
            // 失败的实例同样可能已启动消息线程，先失效其代际，再后台释放。
            ++m_generation;
            if (m_render.stopPlayback) m_render.stopPlayback();
            retirePlayer(false);
            m_state.setError(QStringLiteral("播放器初始化失败"));
        }
        return;
    }
    const int state = m_player->oran7mp_get_state();
    if (state == MP_STATE_ASYNC_PREPARING) {
        m_pauseWhenPrepared = !m_pauseWhenPrepared;
        m_state.setState(m_pauseWhenPrepared ? Oran7PlaybackState::Paused : Oran7PlaybackState::Opening);
    } else if (state == MP_STATE_STARTED) m_player->oran7mp_pause();
    else if (state == MP_STATE_PAUSED) m_player->oran7mp_start();
}

void Oran7PlaybackController::stop()
{
    if (m_shutdown) return;
    ++m_commandRevision;
    m_pendingSwitch = false;
    m_switchTimer.stop();
    QPointer<Oran7PlaybackController> guard(this);
    const auto command = m_commandRevision;
    m_state.setState(Oran7PlaybackState::Idle);
    if (guard && command == m_commandRevision) emit stopRequested();
}

void Oran7PlaybackController::stopCurrent()
{
    ++m_generation;
    if (m_render.stopPlayback) m_render.stopPlayback();
    retirePlayer();
    QPointer<Oran7PlaybackController> guard(this);
    const auto command = m_commandRevision;
    emit stopped(m_pendingSwitch);
    if (!guard || command != m_commandRevision) return;
    if (!m_pendingSwitch && m_state.state() != Oran7PlaybackState::Ended
        && m_state.state() != Oran7PlaybackState::Error) {
        m_state.setState(Oran7PlaybackState::Idle);
        if (guard && command == m_commandRevision) m_state.setPosition(0);
    }
}

void Oran7PlaybackController::shutdown()
{
    if (m_shutdown) return;
    m_shutdown = true;
    ++m_commandRevision;
    m_pendingSwitch = false;
    m_switchTimer.stop();
    m_progressTimer.stop();
    m_afterPlayersReleased.clear();
    emit stopRequested();
}

void Oran7PlaybackController::retirePlayer(bool pauseBeforeRelease)
{
    if (!m_player) return;
    auto player = std::move(m_player);
    if (Oran7Async::isShuttingDown()) return; // unique_ptr 在退出闸门关闭后同步释放。
    ++m_retiringPlayers;
    const auto task = Oran7Async::run(this,
        [player = std::move(player), pauseBeforeRelease]() mutable {
            if (pauseBeforeRelease) {
                player->oran7mp_pause();
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            player.reset();
        }, [this] {
            --m_retiringPlayers;
            if (!m_shutdown) {
                if (m_retiringPlayers == 0 && m_pendingSwitch && !m_player && !m_switchTimer.isActive())
                    m_switchTimer.start(0);
                releaseCallbacks(); // 回调可能重入停止或销毁控制器，之后不再访问 this。
            }
        });
    if (!task.isValid()) --m_retiringPlayers;
}

void Oran7PlaybackController::whenPlayersReleased(std::function<void()> callback)
{
    if (m_shutdown) return;
    m_afterPlayersReleased.append(std::move(callback));
    releaseCallbacks();
}

void Oran7PlaybackController::releaseCallbacks()
{
    if (m_retiringPlayers != 0) return;
    auto callbacks = std::exchange(m_afterPlayersReleased, {});
    QPointer<Oran7PlaybackController> guard(this);
    for (auto& callback : callbacks) {
        callback();
        if (!guard || m_shutdown) return;
    }
}

int Oran7PlaybackController::seekTo(int seconds)
{
    if (m_shutdown || !m_player) return -1;
    const auto generation = m_generation;
    QPointer<Oran7PlaybackController> guard(this);
    m_state.beginSeek(qMax(qint64(0), qint64(seconds) * 1000));
    if (guard && generation == m_generation && m_player)
        m_player->oran7mp_seek_to(long(m_state.positionMs()));
    return 0;
}

void Oran7PlaybackController::setVolume(int volume)
{
    if (m_shutdown) return;
    volume = qBound(0, volume, 100);
    // 保留内核新实例的默认音量入口。
    DEFAULT_VOLUM_SIZE = int(SDL_MIX_MAXVOLUME * volume / 100.0);
    if (m_player) m_player->oran7mp_set_playback_volume(volume);
    m_state.setVolume(volume);
}

void Oran7PlaybackController::setDevice(ID3D11Device* device)
{
    if (!m_shutdown && m_player) m_player->oran7mp_setD3D11Device(device);
}

void Oran7PlaybackController::updatePosition()
{
    if (m_shutdown || !m_player || m_state.state() == Oran7PlaybackState::Seeking) return;
    const double seconds = m_player->oran7mp_get_current_position_double();
    const qint64 duration = m_state.durationMs();
    if (!std::isfinite(seconds) || seconds < 0 || duration <= 0 || seconds * 1000 > duration) return;
    const auto generation = m_generation;
    const auto command = m_commandRevision;
    QPointer<Oran7PlaybackController> guard(this);
    m_state.setPosition(qint64(seconds * 1000));
    if (guard && !m_shutdown && generation == m_generation && command == m_commandRevision)
        emit playProgressUpdated(int(65536 * seconds * 1000.0 / duration), seconds);
}

int Oran7PlaybackController::messageLoop(Oran7MediaPlayer* player,
    const std::shared_ptr<Oran7EventMailbox>& mailbox, quint64 generation)
{
    for (;;) {
        AVMessage message;
        if (player->oran7mp_get_msg(&message, 1) < 0) break;
        switch (message.value) {
        case FFP_MSG_PREPARED:
        case FFP_MSG_PLAYBACK_STATE_CHANGED:
        case FFP_MSG_PLAY_FNISH:
        case FFP_MSG_FIND_STREAM_INFO:
        case FFP_MSG_SEEK_COMPLETE:
        case FFP_MSG_ERROR: {
            const qint64 value = message.value == FFP_MSG_FIND_STREAM_INFO
                ? player->oran7mp_get_duration() : message.arg1;
            mailbox->post(generation, message.value, value);
            break;
        }
        default:
            break;
        }
        msg_free_obj_res(&message);
    }
    return 0;
}

void Oran7PlaybackController::acceptEvent(quint64 generation, int event, qint64 value)
{
    if (m_shutdown || generation != m_generation) return;
    const auto command = m_commandRevision;
    QPointer<Oran7PlaybackController> guard(this);
    switch (event) {
    case FFP_MSG_PREPARED:
        if (m_player) {
            if (m_pauseWhenPrepared) m_player->oran7mp_pause();
            else m_player->oran7mp_start();
        }
        break;
    case FFP_MSG_PLAYBACK_STATE_CHANGED:
        if (value == MP_STATE_STARTED) m_state.setState(Oran7PlaybackState::Playing);
        else if (value == MP_STATE_PAUSED) m_state.setState(Oran7PlaybackState::Paused);
        break;
    case FFP_MSG_PLAY_FNISH:
        m_state.setState(Oran7PlaybackState::Ended);
        if (!guard || command != m_commandRevision) return;
        emit stopIconUpdated();
        if (!guard || command != m_commandRevision) return;
        emit stopRequested();
        if (guard && command == m_commandRevision && !m_shutdown) emit playNextTriggered();
        break;
    case FFP_MSG_FIND_STREAM_INFO:
        m_state.setDuration(value);
        if (guard && generation == m_generation && command == m_commandRevision)
            emit totalDurationUpdated(int(m_state.durationMs() / 1000));
        break;
    case FFP_MSG_SEEK_COMPLETE:
        m_state.finishSeek();
        break;
    case FFP_MSG_ERROR:
        m_state.setError(QStringLiteral("媒体打开失败（错误码 %1）").arg(value));
        if (guard && generation == m_generation && command == m_commandRevision) emit openFailed();
        break;
    default:
        break;
    }
}
