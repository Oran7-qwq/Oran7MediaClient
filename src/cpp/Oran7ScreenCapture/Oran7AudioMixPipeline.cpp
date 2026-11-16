#include "Oran7AudioMixPipeline.h"
#include "Oran7WasapiAudioWorker.h"
#include "Oran7AudioCaptureFeed.h"
#include <chrono>
#include <algorithm>

Oran7AudioMixPipeline::~Oran7AudioMixPipeline()
{
    stop();
}
bool Oran7AudioMixPipeline::start(std::shared_ptr<Oran7AudioEncoder> encoder,
                                  std::shared_ptr<Oran7LiveAudioControl> control)
{
    if (m_thread.joinable() || !encoder || !control)
        return false;
    m_encoder = std::move(encoder);
    return startThread(std::move(control));
}
bool Oran7AudioMixPipeline::startSource(std::shared_ptr<Oran7LiveAudioControl> control,
                                        std::shared_ptr<Oran7AudioCaptureFeed> feed)
{
    if (m_thread.joinable() || !control || !feed)
        return false;
    m_feed = std::move(feed);
    return startThread(std::move(control));
}
bool Oran7AudioMixPipeline::startThread(std::shared_ptr<Oran7LiveAudioControl> control)
{
    m_control = std::move(control);
    m_origin = Oran7WasapiAudioWorker::clockNow100ns();
    if (m_feed)
        m_feed->setOrigin(m_origin);
    m_finishPts.store(-1);
    m_captureDrops.store(0);
    m_microphoneDrops.store(0);
    m_discontinuities.store(0);
    m_microphoneDiscontinuities.store(0);
    m_error.store(Error::None);
    m_running.store(true);
    m_thread = std::thread(
        [this]
        {
            try
            {
                run();
            }
            catch (...)
            {
                m_error.store(Error::Mixing);
            }
            m_running.store(false);
        });
    return true;
}
void Oran7AudioMixPipeline::stop(bool drain)
{
    if (drain && m_thread.joinable())
        m_finishPts.store(av_rescale(Oran7WasapiAudioWorker::clockNow100ns() - m_origin, 48000, 10000000));
    m_running.store(false);
    if (m_thread.joinable())
        m_thread.join();
    m_encoder.reset();
    m_control.reset();
    m_feed.reset();
}
void Oran7AudioMixPipeline::run()
{
    Oran7LiveAudioMixer mixer;
    std::array<std::unique_ptr<Oran7WasapiAudioWorker>, 2> inputs;
    std::array<uint64_t, 2> retiredDrops{}, retiredDiscontinuities{};
    while (m_running.load())
    {
        const auto settings = m_control->get();
        const std::array<bool, 2> enabled{settings.systemEnabled, settings.microphoneEnabled};
        for (int lane = 0; lane < 2; ++lane)
        {
            if (!enabled[lane] && inputs[lane])
            {
                inputs[lane]->stop();
                retiredDrops[lane] += inputs[lane]->droppedFrames();
                retiredDiscontinuities[lane] += inputs[lane]->discontinuities();
                inputs[lane].reset();
            }
            if (enabled[lane] && !inputs[lane])
            {
                mixer.clear(lane);
                inputs[lane] = std::make_unique<Oran7WasapiAudioWorker>();
                inputs[lane]->start({48000, 2, 10, lane == 1, m_origin});
            }
            if (inputs[lane])
            {
                if (!inputs[lane]->isRunning())
                {
                    m_error.store(lane == 0 ? Error::System : Error::Microphone);
                    return;
                }
                for (int batch = 0; batch < 32; ++batch)
                {
                    AVFrame* frame = nullptr;
                    if (!inputs[lane]->getPcmFrame(&frame, 0))
                        break;
                    const bool ok = mixer.push(lane, frame);
                    av_frame_free(&frame);
                    if (!ok)
                    {
                        m_error.store(Error::Mixing);
                        return;
                    }
                }
            }
            auto& drops = lane ? m_microphoneDrops : m_captureDrops;
            auto& discontinuities = lane ? m_microphoneDiscontinuities : m_discontinuities;
            drops.store(retiredDrops[lane] + (inputs[lane] ? inputs[lane]->droppedFrames() : 0));
            discontinuities.store(retiredDiscontinuities[lane] + (inputs[lane] ? inputs[lane]->discontinuities() : 0));
        }
        const auto elapsedSamples = av_rescale(Oran7WasapiAudioWorker::clockNow100ns() - m_origin, 48000, 10000000);
        // 留 60ms 采集/重采样余量；缺少任一路时补静音，绝不等待麦克风阻塞系统声。
        if (elapsedSamples - mixer.nextPts() > 24000)
        {
            m_error.store(Error::Mixing);
            return;
        }
        for (int batch = 0; batch < 8 && mixer.nextPts() + 480 + 2880 <= elapsedSamples; ++batch)
        {
            auto* mixed = mixer.render(settings);
            if (!mixed)
            {
                m_error.store(Error::Mixing);
                return;
            }
            if (m_feed)
                m_feed->submit(mixed);
            else
                m_encoder->pushFrame(mixed);
            m_control->publishLevels(mixer.levels());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // 文件录制补交停止时刻前的 60ms 等待区；RTMP 停止直接取消。
    const auto finish = m_finishPts.load();
    if (finish >= 0)
    {
        if (finish - mixer.nextPts() > 24000)
        {
            m_error.store(Error::Mixing);
            return;
        }
        for (int lane = 0; lane < 2; ++lane)
        {
            if (!inputs[lane])
                continue;
            for (int batch = 0; batch < 32; ++batch)
            {
                AVFrame* frame = nullptr;
                if (!inputs[lane]->getPcmFrame(&frame, 0))
                    break;
                const bool ok = mixer.push(lane, frame);
                av_frame_free(&frame);
                if (!ok)
                {
                    m_error.store(Error::Mixing);
                    return;
                }
            }
        }
        while (mixer.nextPts() < finish)
        {
            const auto remaining = finish - mixer.nextPts();
            auto* frame = mixer.render(m_control->get());
            if (!frame)
            {
                m_error.store(Error::Mixing);
                return;
            }
            frame->nb_samples = int(std::min<int64_t>(frame->nb_samples, remaining));
            if (m_feed)
                m_feed->submit(frame);
            else
                m_encoder->pushFrame(frame);
            m_control->publishLevels(mixer.levels());
        }
    }
}
