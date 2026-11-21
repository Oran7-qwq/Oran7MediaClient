#pragma once

#include "Oran7CaptureComposition.h"
#include <atomic>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

extern "C"
{
#include <libavutil/frame.h>
}

// 辅助显示器最多保留一帧 NV12。源停用时，旧线程不能重新写入已退役的邮箱。
class Oran7SceneFrameMailbox
{
  public:
    void submit(std::shared_ptr<AVFrame> frame)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_closed)
            m_frame = std::move(frame);
    }
    std::shared_ptr<AVFrame> latest() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_frame;
    }
    void close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closed = true;
        m_frame.reset();
    }

  private:
    mutable std::mutex m_mutex;
    std::shared_ptr<AVFrame> m_frame;
    bool m_closed = false;
};

// 同一显示器只采集一次。预览消费者持有独立帧头，共用纹理租约。
class Oran7CapturePreviewFanout
{
  public:
    using Consumer = std::function<void(AVFrame*)>;

    void setConsumer(int layerId, Consumer submit)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (submit)
            m_consumers[layerId] = std::move(submit);
        else
            m_consumers.erase(layerId);
    }
    void submit(AVFrame* frame)
    {
        if (!frame)
            return;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_consumers.begin(); it != m_consumers.end(); ++it)
        {
            if (std::next(it) == m_consumers.end())
            {
                it->second(frame);
                return;
            }
            if (auto* copy = av_frame_clone(frame))
                it->second(copy);
        }
        av_frame_free(&frame);
    }

  private:
    std::mutex m_mutex;
    std::map<int, Consumer> m_consumers;
};

class Oran7CaptureScene
{
  public:
    struct Layer
    {
        Oran7CaptureComposition composition;
        std::shared_ptr<Oran7SceneFrameMailbox> frames;
        bool primary = false;
        bool image = false; // 静态图像邮箱保存预乘 Alpha 的 BGRA。
    };
    void setLayers(std::vector<Layer> layers)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_layers = std::move(layers);
    }
    std::vector<Layer> layers() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_layers;
    }
    std::atomic_bool outputActive{false};

  private:
    mutable std::mutex m_mutex;
    std::vector<Layer> m_layers;
};
