#pragma once
#include "Oran7RecordingFeed.h"
#include "Oran7CaptureFrameConverter.h"
#include <map>

// 采集线程独占。各路调度和时间轴独立，同一源帧的同尺寸输出共享转换后的像素。
class Oran7CaptureOutputRouter
{
  public:
    struct Statistics
    {
        int submitted = 0, conversions = 0;
        qint64 conversionNs = 0;
    };
    Oran7CaptureOutputRouter(ID3D11Device* device, std::shared_ptr<Oran7RecordingFeed> feed,
                             Oran7CaptureFrameConverter::FrameTransfer download = nullptr)
        : m_device(device), m_feed(std::move(feed)), m_download(download)
    {
    }
    Statistics submit(const AVFrame*, int64_t nowUs);

  private:
    struct Route
    {
        std::unique_ptr<Oran7CaptureFrameConverter> converter;
        int64_t nextUs = 0, lastPts = AV_NOPTS_VALUE;
    };
    ID3D11Device* m_device; // 借用 worker 的设备，路由器先于设备释放。
    std::shared_ptr<Oran7RecordingFeed> m_feed;
    Oran7CaptureFrameConverter::FrameTransfer m_download;
    std::map<uint64_t, Route> m_routes;
};
