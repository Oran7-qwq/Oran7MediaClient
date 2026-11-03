#pragma once
#include "Oran7VideoEncoder.h"
#include <mutex>
#include <memory>

// 采集线程与录制任务之间的共享值/编码入口，生命周期独立于 QObject worker。
class Oran7RecordingFeed {
public:
    struct Oran7Snapshot {
        int width{}, height{}, targetWidth{}, targetHeight{}, fps{};
        bool alive = true;
        quint64 epoch{};
        std::shared_ptr<AVBufferRef> device;
        std::shared_ptr<Oran7VideoEncoder> encoder;
    };
    Oran7Snapshot snapshot() const { std::lock_guard<std::mutex> lock(m_mutex); return m_value; }
    void publish(int width, int height, AVBufferRef* device) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_value.width = width; m_value.height = height;
        if (!m_value.device && device) m_value.device = std::shared_ptr<AVBufferRef>(av_buffer_ref(device), [](auto* ref) { av_buffer_unref(&ref); });
    }
    bool attach(std::shared_ptr<Oran7VideoEncoder> encoder, int width, int height, int fps) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_value.alive) return false;
        ++m_value.epoch; m_value.targetWidth = width; m_value.targetHeight = height; m_value.fps = fps;
        m_value.encoder = std::move(encoder); return true;
    }
    void detach() { std::lock_guard<std::mutex> lock(m_mutex); m_value.encoder.reset(); ++m_value.epoch; }
    void close() { std::lock_guard<std::mutex> lock(m_mutex); m_value.alive = false; m_value.encoder.reset(); ++m_value.epoch; }
private:
    mutable std::mutex m_mutex;
    Oran7Snapshot m_value;
};
