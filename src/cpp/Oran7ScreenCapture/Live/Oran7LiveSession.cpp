#include "Oran7LiveSession.h"
#include "Oran7LiveBackend.h"
#include "Oran7RtmpPublisher.h"
#include <QDebug>
Oran7LiveSession::Oran7LiveSession() : m_task(std::make_unique<Oran7RecordingSession>())
{
    connect(m_task.get(), &Oran7RecordingSession::changed, this,
            [this]
            {
                const auto current = state();
                if (current != m_reportedState)
                {
                    m_reportedState = current;
                    qInfo().noquote() << "[RTMP] 会话状态：" << current;
                    if (!error().isEmpty())
                        qWarning().noquote() << "[RTMP] " << error();
                }
                emit changed();
            });
}
bool Oran7LiveSession::start(const Oran7LiveConfig& c, std::shared_ptr<Oran7RecordingFeed> feed,
                             std::shared_ptr<Oran7AudioCaptureFeed> audioFeed,
                             std::shared_ptr<Oran7LiveAudioControl> control)
{
    if (busy())
        return false;
    if (control) m_audioControl=std::move(control);
    else {m_audioControl=std::make_shared<Oran7LiveAudioControl>();setAudioSettings(c);}
    return m_task->start([c, feed, control = m_audioControl, audioFeed](const std::atomic_bool& cancel,
                                                                        const Oran7RecordingCallbacks& callbacks)
                         { return Oran7LiveBackend::run(c, feed, cancel, callbacks, control, audioFeed); });
}
QString Oran7LiveSession::state() const
{
    switch (m_task->state())
    {
    case Oran7RecordingSession::Oran7RecordingState::Preparing:
        return "Connecting";
    case Oran7RecordingSession::Oran7RecordingState::Recording:
        return "Streaming";
    case Oran7RecordingSession::Oran7RecordingState::Stopping:
        return "Stopping";
    case Oran7RecordingSession::Oran7RecordingState::Error:
        return "Error";
    default:
        return "Idle";
    }
}
QString Oran7LiveSession::validate(const Oran7LiveConfig& c, bool starting)
{
    if (c.systemVolume < 0 || c.systemVolume > 100 || c.microphoneVolume < 0 || c.microphoneVolume > 100)
        return QStringLiteral("直播音量范围为 0～100%");
    if (c.platform != "bilibili" && c.platform != "custom")
        return QStringLiteral("请选择有效的推流平台");
    if (c.width < 16 || c.height < 16 || c.width > 2560 || c.height > 1440 || c.width % 2 || c.height % 2 ||
        c.fps < 1 || c.fps > 60 || c.bitrateKbps < 100 || c.bitrateKbps > 50000)
        return QStringLiteral("直播上限为 2560×1440 / 60 FPS，码率范围 100～50000 kbps");
    if (starting || !c.server.isEmpty())
    {
        const auto error = Oran7RtmpPublisher::validate(c.server, c.streamKey);
        if (!error.isEmpty())
            return error;
    }
    else if (!c.streamKey.isEmpty())
        return QStringLiteral("请同时设置推流地址和推流码");
    return {};
}
