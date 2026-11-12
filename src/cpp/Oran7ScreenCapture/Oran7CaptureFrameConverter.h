#pragma once
#include <QString>
#include <memory>
#include <d3d11.h>
extern "C"
{
#include <libavutil/frame.h>
}

// 单个采集线程独占的转换资源，不持 QObject、界面或编码器，不参与启停状态管理。
class Oran7CaptureFrameConverter
{
  public:
    struct Oran7FrameDeleter
    {
        void operator()(AVFrame* frame) const { av_frame_free(&frame); }
    };
    using Frame = std::unique_ptr<AVFrame, Oran7FrameDeleter>;
    struct Oran7PreviewResult
    {
        Frame frame;
        QString error;
        bool invalidated = false;
    };
    // 默认使用 FFmpeg 硬件下载；离线检查可替换这个边界，不依赖硬件视频接口。
    using FrameTransfer = int (*)(AVFrame*, const AVFrame*, int);
    explicit Oran7CaptureFrameConverter(ID3D11Device* device, int fps, FrameTransfer download = nullptr);
    ~Oran7CaptureFrameConverter();
    Oran7CaptureFrameConverter(const Oran7CaptureFrameConverter&) = delete;
    Oran7CaptureFrameConverter& operator=(const Oran7CaptureFrameConverter&) = delete;
    // 空帧且无错误表示预览忙/不支持；不影响随后对原始帧的录制转换。
    Oran7PreviewResult preview(const AVFrame* source);
    Frame recording(const AVFrame* source, int width, int height);
    void beginRecording(); // 新录制重新尝试 GPU 转换。
    void endRecording(); // 归还空闲软件缓冲，编码器持有的在途帧自行延寿。
  private:
    struct Oran7Resources;
    std::unique_ptr<Oran7Resources> m_impl;
};
