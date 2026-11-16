#pragma once
#include "Oran7AudioEncoder.h"
#include "Oran7WasapiAudioWorker.h"
#include <algorithm>
#include <map>

// 持续采集音频流；输出会话只订阅新帧，不接管设备，也不回放历史声音。
class Oran7AudioCaptureFeed
{
  public:
    struct Snapshot
    {
        bool alive = true, ready = false;
        int error = 0;
        uint64_t captureDrops = 0, microphoneDrops = 0, discontinuities = 0;
    };
    void updateDiagnostics(uint64_t captureDrops, uint64_t microphoneDrops, uint64_t discontinuities)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.captureDrops = captureDrops;
        m_state.microphoneDrops = microphoneDrops;
        m_state.discontinuities = discontinuities;
    }
    Snapshot snapshot() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }
    void setOrigin(int64_t origin)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_origin = origin;
    }
    uint64_t attach(std::shared_ptr<Oran7AudioEncoder> encoder)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!encoder || !m_state.alive || !m_state.ready || m_outputs.size() >= 2)
            return 0;
        for (const auto& item : m_outputs)
            if (item.second->encoder == encoder)
                return 0;
        auto sink = std::make_shared<Sink>();
        sink->encoder = std::move(encoder);
        sink->origin = av_rescale(Oran7WasapiAudioWorker::clockNow100ns() - m_origin, 48000, 10000000);
        const auto id = ++m_generation;
        m_outputs.emplace(id, std::move(sink));
        return id;
    }
    bool finish(uint64_t subscription)
    {
        if (!subscription)
            return true;
        std::shared_ptr<Sink> sink;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto it = m_outputs.find(subscription);
            if (it == m_outputs.end())
                return false;
            sink = it->second;
            sink->end = av_rescale(Oran7WasapiAudioWorker::clockNow100ns() - m_origin, 48000, 10000000);
        }
        for (int i = 0; i < 100; ++i)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (sink->finished)
                    return true;
                if (!m_state.alive || sink->cancelled)
                    return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        detach(subscription);
        return false;
    }
    void detach(uint64_t subscription)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_outputs.find(subscription);
        if (it != m_outputs.end())
        {
            it->second->cancelled = true;
            m_outputs.erase(it);
        }
    }
    // 两个订阅各自克隆 PCM 并归零时间戳，慢输出只影响自己的有界编码队列。
    void submit(AVFrame* frame)
    {
        if (!frame)
            return;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_state.alive)
            {
                m_state.ready = true;
                for (auto it = m_outputs.begin(); it != m_outputs.end();)
                {
                    const auto& sink = it->second;
                    const int64_t skip = std::max<int64_t>(0, sink->origin - frame->pts);
                    const auto end = std::min(frame->pts + frame->nb_samples, sink->end);
                    if (frame->pts + skip < end)
                    {
                        auto* copy = av_frame_clone(frame);
                        if (copy)
                        {
                            copy->data[0] += skip * 4;
                            copy->extended_data[0] = copy->data[0];
                            copy->nb_samples = int(end - frame->pts - skip);
                            copy->pts = frame->pts + skip - sink->origin;
                            sink->encoder->pushFrame(copy);
                        }
                    }
                    if (frame->pts + frame->nb_samples >= sink->end)
                    {
                        sink->finished = true;
                        it = m_outputs.erase(it);
                    }
                    else
                        ++it;
                }
            }
        }
        av_frame_free(&frame);
    }
    void close(int error = 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.alive = false;
        m_state.ready = false;
        m_state.error = error;
        for (const auto& item : m_outputs)
            item.second->cancelled = true;
        m_outputs.clear();
    }

  private:
    mutable std::mutex m_mutex;
    Snapshot m_state;
    struct Sink
    {
        std::shared_ptr<Oran7AudioEncoder> encoder;
        int64_t origin = 0, end = INT64_MAX;
        bool finished = false, cancelled = false;
    };
    int64_t m_origin = 0;
    uint64_t m_generation = 0;
    std::map<uint64_t, std::shared_ptr<Sink>> m_outputs;
};
