#pragma once
#include "Oran7VideoEncoder.h"
#include "Oran7CaptureComposition.h"
#include "Oran7CaptureScene.h"
#include <mutex>
#include <memory>
#include <map>
#include <vector>
#include <chrono>

// 一个采集源，最多两个独立输出订阅。源代际只随源尺寸/关闭改变。
class Oran7RecordingFeed
{
  public:
    struct Oran7Snapshot
    {
        int width{}, height{};
        bool alive = true;
        quint64 epoch{};
        std::shared_ptr<AVBufferRef> device;
    };
    struct Oran7Output
    {
        uint64_t id{};
        int width{}, height{}, fps{};
        int64_t startUs{};
        std::shared_ptr<Oran7VideoEncoder> encoder;
    };
    static int64_t clockUs()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    void setComposition(const Oran7CaptureComposition& value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_composition = value;
    }
    Oran7CaptureComposition composition() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_composition;
    }
    void setScene(std::shared_ptr<Oran7CaptureScene> scene) { m_scene = std::move(scene); } // 启动线程前设置。
    const std::shared_ptr<Oran7CaptureScene>& scene() const { return m_scene; }
    Oran7Snapshot snapshot() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_value;
    }
    std::vector<Oran7Output> outputs() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<Oran7Output> result;
        for (const auto& item : m_outputs)
            result.push_back(item.second);
        return result;
    }
    void publish(int width, int height, AVBufferRef* device)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_value.alive)
            return;
        if (m_value.width != width || m_value.height != height)
            ++m_value.epoch;
        m_value.width = width;
        m_value.height = height;
        if (!m_value.device && device)
            m_value.device =
                std::shared_ptr<AVBufferRef>(av_buffer_ref(device), [](auto* ref) { av_buffer_unref(&ref); });
    }
    uint64_t attach(std::shared_ptr<Oran7VideoEncoder> encoder, int width, int height, int fps)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_value.alive || !encoder || m_outputs.size() >= 2 || fps < 1)
            return 0;
        for (const auto& item : m_outputs)
            if (item.second.encoder == encoder)
                return 0;
        const auto id = ++m_generation;
        m_outputs.emplace(id, Oran7Output{id, width, height, fps, clockUs(), std::move(encoder)});
        if (m_scene)
            m_scene->outputActive.store(true);
        return id;
    }
    void detach(uint64_t id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outputs.erase(id);
        if (m_scene && m_outputs.empty())
            m_scene->outputActive.store(false);
    }
    // 交付时复核订阅，detach 返回后不会再向该编码器投递。
    bool deliver(uint64_t id, AVFrame* frame)
    {
        if (!frame)
            return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_outputs.find(id);
        if (!m_value.alive || it == m_outputs.end())
        {
            av_frame_free(&frame);
            return false;
        }
        it->second.encoder->pushFrame(frame);
        return true;
    }
    void close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_value.alive = false;
        m_outputs.clear();
        if (m_scene)
            m_scene->outputActive.store(false);
        ++m_value.epoch;
    }

  private:
    mutable std::mutex m_mutex;
    Oran7Snapshot m_value;
    Oran7CaptureComposition m_composition;
    std::shared_ptr<Oran7CaptureScene> m_scene;
    uint64_t m_generation = 0;
    std::map<uint64_t, Oran7Output> m_outputs;
};
