#include "Oran7AudioCaptureBackend.h"
#include <QScopeGuard>

QString Oran7AudioCaptureBackend::run(std::shared_ptr<Oran7AudioCaptureFeed> feed,
                                      std::shared_ptr<Oran7LiveAudioControl> control, const std::atomic_bool& cancel,
                                      const Oran7RecordingCallbacks& cb)
{
    int error = 0;
    const auto closed = qScopeGuard([&] { feed->close(error); });
    Oran7AudioMixPipeline pipeline;
    if (cancel.load())
        return {};
    if (!pipeline.startSource(control, feed))
    {
        error = int(Oran7AudioMixPipeline::Error::Mixing);
        return errorMessage(error);
    }
    bool ready = false;
    int ticks = 0;
    while (!cancel.load())
    {
        error = int(pipeline.error());
        if (error)
            return errorMessage(error);
        if (++ticks % 20 == 0)
            feed->updateDiagnostics(pipeline.captureDrops(), pipeline.microphoneDrops(), pipeline.discontinuities());
        if (!ready && feed->snapshot().ready)
        {
            ready = true;
            cb.started({});
        }
        QThread::msleep(5);
    }
    return {};
}
