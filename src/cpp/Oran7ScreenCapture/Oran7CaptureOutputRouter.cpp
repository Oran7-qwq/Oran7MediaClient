#include "Oran7CaptureOutputRouter.h"
#include <QElapsedTimer>
#include <algorithm>

Oran7CaptureOutputRouter::Statistics Oran7CaptureOutputRouter::submit(const AVFrame* source, int64_t nowUs)
{
    Statistics stats;
    const auto outputs = m_feed->outputs();
    for (auto it = m_routes.begin(); it != m_routes.end();)
    {
        if (std::none_of(outputs.begin(), outputs.end(), [&](const auto& o) { return o.id == it->first; }))
            it = m_routes.erase(it);
        else
            ++it;
    }
    if (!source)
        return stats;
    for (const auto& output : outputs)
    {
        if (!output.encoder->isRuning())
            continue;
        auto& route = m_routes[output.id];
        if (!route.converter)
        {
            route.converter = std::make_unique<Oran7CaptureFrameConverter>(m_device, output.fps, m_download);
            route.converter->beginRecording();
            route.nextUs = output.startUs;
        }
        if (nowUs + 1000 < route.nextUs)
            continue;
        QElapsedTimer conversion;
        conversion.start();
        auto frame = route.converter->recording(source, output.width, output.height);
        stats.conversionNs += conversion.nsecsElapsed();
        ++stats.conversions;
        if (!frame)
            continue;
        auto pts = std::max<int64_t>(0, (nowUs - output.startUs) * 90000 / 1000000);
        if (route.lastPts != AV_NOPTS_VALUE && pts <= route.lastPts)
            pts = route.lastPts + 1;
        route.lastPts = pts;
        frame->pts = pts;
        frame->duration = av_rescale_q(1, {1, output.fps}, {1, 90000});
        if (pts == 0)
            frame->pict_type = AV_PICTURE_TYPE_I;
        route.nextUs = std::max(route.nextUs + 1000000 / output.fps, nowUs);
        if (m_feed->deliver(output.id, frame.release()))
            ++stats.submitted;
    }
    return stats;
}
