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
    struct SceneInput
    {
        Oran7CaptureComposition composition;
        std::shared_ptr<AVFrame> auxiliaryFrame;
        bool primary;
        bool image;
    };
    struct ImageOverlay
    {
        std::shared_ptr<AVFrame> source;
        QRect crop;
        QSize size;
        int quarterTurns = 0;
        bool mirrored = false;
        bool opaque = false;
        bool visible = false;
        std::vector<uint8_t> y, uv, alpha, chromaAlpha;
        void prepare(const SceneInput& layer, const Oran7CaptureComposition::Oran7Blit& blit);
        void blend(AVFrame& destination, const QPoint& origin) const;
    };
    struct Route
    {
        std::unique_ptr<Oran7CaptureFrameConverter> converter;
        std::map<const AVFrame*, ImageOverlay> images;
        int64_t nextUs = 0, lastPts = AV_NOPTS_VALUE;
    };
    static Oran7CaptureFrameConverter::Frame composeScene(Route& route, const AVFrame* source, const QSize& canvas,
        const Oran7CaptureComposition& fallback, const std::vector<SceneInput>& layers);
    ID3D11Device* m_device; // 借用 worker 的设备，路由器先于设备释放。
    std::shared_ptr<Oran7RecordingFeed> m_feed;
    Oran7CaptureFrameConverter::FrameTransfer m_download;
    std::map<uint64_t, Route> m_routes;
};
