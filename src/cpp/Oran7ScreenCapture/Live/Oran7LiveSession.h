#pragma once
#include "../Oran7RecordingSession.h"
#include "../../Oran7Config/Oran7AppConfig.h"
#include "Oran7LiveAudioMixer.h"
class Oran7RecordingFeed;
class Oran7AudioCaptureFeed;
// 复用经过验证的异步任务执行器，每个直播对象拥有独立状态和取消代际。
class Oran7LiveSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantMap statistics READ statistics NOTIFY changed)
  public:
    Oran7LiveSession();
    bool start(const Oran7LiveConfig&, std::shared_ptr<Oran7RecordingFeed>,
               std::shared_ptr<Oran7AudioCaptureFeed> = {}, std::shared_ptr<Oran7LiveAudioControl> = {});
    Q_INVOKABLE void stop() { m_task->stop(); }
    void shutdown() { m_task->shutdown(); }
    bool busy() const { return m_task->busy(); }
    QString state() const;
    QString error() const { return m_task->error(); }
    QVariantMap statistics() const { return m_task->statistics(); }
    std::array<Oran7AudioSourceLevel, 2> takeAudioLevels() { return m_audioControl->takeLevels(); }
    static QString validate(const Oran7LiveConfig&, bool starting);
    void setAudioSettings(const Oran7LiveConfig& c)
    {
        m_audioControl->set({c.audioEnabled, c.microphoneEnabled, c.systemVolume, c.microphoneVolume});
    }
  signals:
    void changed();

  private:
    QString m_reportedState;
    std::unique_ptr<Oran7RecordingSession> m_task;
    std::shared_ptr<Oran7LiveAudioControl> m_audioControl = std::make_shared<Oran7LiveAudioControl>();
};
