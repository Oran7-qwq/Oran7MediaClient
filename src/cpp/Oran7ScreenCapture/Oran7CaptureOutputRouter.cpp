#include "Oran7CaptureOutputRouter.h"
#include <QElapsedTimer>
#include <algorithm>
#include <array>

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

    struct Oran7DueOutput
    {
        const Oran7RecordingFeed::Oran7Output* output;
        Route* route;
    };
    // Feed 最多同时保留两个订阅。
    std::array<Oran7DueOutput, 2> due{};
    std::size_t dueCount = 0;
    for (const auto& output : outputs)
    {
        if (!output.encoder->isRuning())
            continue;
        auto [it, inserted] = m_routes.try_emplace(output.id);
        auto& route = it->second;
        if (inserted)
            route.nextUs = output.startUs;
        if (nowUs + 1000 >= route.nextUs)
            due[dueCount++] = {&output, &route};
    }

    const bool sharePixels = dueCount == 2 && due[0].output->width == due[1].output->width
                             && due[0].output->height == due[1].output->height;
    Oran7CaptureFrameConverter::Frame secondFrame;
    for (std::size_t index = 0; index < dueCount; ++index)
    {
        const auto& output = *due[index].output;
        auto& route = *due[index].route;
        Oran7CaptureFrameConverter::Frame frame;
        if (sharePixels && index == 1)
        {
            frame = std::move(secondFrame);
        }
        else
        {
            if (!route.converter)
            {
                route.converter = std::make_unique<Oran7CaptureFrameConverter>(m_device, output.fps, m_download);
                route.converter->beginRecording();
            }
            QElapsedTimer conversion;
            conversion.start();
            frame = route.converter->recording(source, output.width, output.height);
            stats.conversionNs += conversion.nsecsElapsed();
            ++stats.conversions;

            // 先复制帧头再交付，编码线程各自修改时间戳和关键帧标记，NV12 像素通过 AVBufferRef 共享。
            if (sharePixels && frame)
                secondFrame.reset(av_frame_clone(frame.get()));
        }
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
