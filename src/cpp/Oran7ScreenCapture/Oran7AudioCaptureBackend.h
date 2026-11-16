#pragma once
#include "Oran7AudioCaptureFeed.h"
#include "Oran7AudioMixPipeline.h"
#include "Oran7RecordingSession.h"

class Oran7AudioCaptureBackend
{
  public:
    static QString run(std::shared_ptr<Oran7AudioCaptureFeed>, std::shared_ptr<Oran7LiveAudioControl>,
                       const std::atomic_bool&, const Oran7RecordingCallbacks&);
    static QString errorMessage(int error)
    {
        if (error == int(Oran7AudioMixPipeline::Error::Microphone))
            return QStringLiteral("麦克风采集失败，请检查默认输入设备和麦克风权限");
        return QStringLiteral("音频采集或混音失败，请检查默认音频设备");
    }
    static QString waitReady(const std::shared_ptr<Oran7AudioCaptureFeed>& feed, const std::atomic_bool& cancel)
    {
        for (int i = 0; i < 1000 && !cancel.load(); ++i)
        {
            const auto state = feed->snapshot();
            if (!state.alive)
                return errorMessage(state.error);
            if (state.ready)
                return {};
            QThread::msleep(5);
        }
        return cancel.load() ? QString() : QStringLiteral("等待音频采集就绪超时");
    }
};
