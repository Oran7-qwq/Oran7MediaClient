#include "Oran7DesktopCaptureWorker.h"
#include "Oran7CaptureTimerPeriod.h"
#include <QElapsedTimer>
#include <QDateTime>
#include <QThread>
#include <wrl/client.h>
#include <dxgi1_2.h>
extern "C"
{
#include <libavutil/hwcontext_d3d11va.h>
#include <libavfilter/buffersink.h>
}
using Microsoft::WRL::ComPtr;
namespace
{
QString ffErrStr(int err)
{
    char buf[256];
    av_strerror(err, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}
} // namespace
Oran7DesktopCaptureWorker::Oran7DesktopCaptureWorker(std::function<void(AVFrame*)> submitFrame, int outputIndex,
                                                     bool drawMouse, int fps)
    : m_submitFrame(std::move(submitFrame)), m_outputIndex(outputIndex), m_drawMouse(drawMouse), m_fps(fps)
{
}
Oran7DesktopCaptureWorker::~Oran7DesktopCaptureWorker()
{
    Q_ASSERT(thread() == QThread::currentThread());
    stop();
    cleanup();
}
void Oran7DesktopCaptureWorker::stop()
{
    m_control->stopRequested.store(true);
}
void Oran7DesktopCaptureWorker::start()
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (m_control->stopRequested.load())
    {
        emit stopped();
        return;
    }
    if (m_started)
        return;
    m_started = true;

    const QString err = initGraph();
    INFO_LOG << err;
    if (!err.isEmpty())
    {
        cleanup();
        if (!m_control->stopRequested.load())
            emit errorOccurred(err);
        emit stopped();
        return;
    }
    else
        INFO_LOG << "Successed ScreenCapture initGraph.";

    if (!m_control->stopRequested.load())
    {
        emit started();
        grabLoop();
    }
    cleanup();
    emit stopped();
}

// 枚举所有 DXGI adapter，返回第一个输出 attached 到桌面的 adapter 索引。
// 笔记本双显卡场景：默认 adapter 可能是独显，而显示器接在核显上，
// 用独显设备做 DDA 会返回 DXGI_ERROR_UNSUPPORTED；必须选显示器实际所在的 GPU。
int Oran7DesktopCaptureWorker::findDesktopAdapterIndex()
{
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return -1;

    for (UINT i = 0; i < 8; ++i)
    {
        ComPtr<IDXGIAdapter1> adapter;
        if (FAILED(factory->EnumAdapters1(i, &adapter)))
            break;

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc)))
            continue;

        // 软件适配器（Microsoft Basic Render Driver）跳过
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // 检查该 adapter 是否有输出 attached 到桌面
        bool hasDesktop = false;
        for (UINT o = 0; o < 16; ++o)
        {
            ComPtr<IDXGIOutput> out;
            if (FAILED(adapter->EnumOutputs(o, &out)))
                break;
            DXGI_OUTPUT_DESC odesc{};
            if (SUCCEEDED(out->GetDesc(&odesc)) && odesc.AttachedToDesktop)
            {
                hasDesktop = true;
                break;
            }
        }

        if (hasDesktop)
        {
            INFO_LOG << "[CaptureDiag] selected adapter[" << i
                     << "] for DDA: " << QString::fromWCharArray(desc.Description);
            return (int)i;
        }
    }
    return -1; // 未找到，回退到默认
}

QString Oran7DesktopCaptureWorker::initGraph()
{
    int ret = 0;
    if (!m_submitFrame)
        return "D3D11VideoItem is null.";

    // 主 GPU（D3D_DRIVER_TYPE_HARDWARE），行为正确。故这里仍传 nullptr。
    const int adapterIdx = findDesktopAdapterIndex();
    INFO_LOG << "[CaptureDiag] findDesktopAdapterIndex=" << adapterIdx << " (仅诊断，不影响设备创建)";

    AVBufferRef* device = nullptr;
    ret = av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
    m_hwdev.reset(device);
    if (ret < 0)
        return "av_hwdevice_ctx_create(D3D11VA) failed: " + ffErrStr(ret);

    if (!m_hwdev)
        return "av_hwdevice_ctx_alloc failed.";

    // 从 hwdev 取出 capture device 指针（后续创建共享纹理、CopyResource 都用它）
    AVHWDeviceContext* hw = reinterpret_cast<AVHWDeviceContext*>(m_hwdev->data);
    AVD3D11VADeviceContext* d3d11 = reinterpret_cast<AVD3D11VADeviceContext*>(hw->hwctx);

    // ---- 诊断：打印抓屏用 D3D11 设备对应的 GPU 与可用显示器输出 ----
    {
        ComPtr<IDXGIDevice> dxgiDev;
        if (d3d11->device && SUCCEEDED(d3d11->device->QueryInterface(IID_PPV_ARGS(&dxgiDev))))
        {
            ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(dxgiDev->GetAdapter(&adapter)))
            {
                DXGI_ADAPTER_DESC adesc{};
                if (SUCCEEDED(adapter->GetDesc(&adesc)))
                    INFO_LOG << "[CaptureDiag] D3D11 capture device GPU: " << QString::fromWCharArray(adesc.Description)
                             << " (Vendor=0x" << QString::number(adesc.VendorId, 16) << " Device=0x"
                             << QString::number(adesc.DeviceId, 16) << ")";
                for (UINT i = 0; i < 16; ++i)
                {
                    ComPtr<IDXGIOutput> out;
                    if (FAILED(adapter->EnumOutputs(i, &out)))
                        break;
                    DXGI_OUTPUT_DESC odesc{};
                    if (SUCCEEDED(out->GetDesc(&odesc)))
                        INFO_LOG << "[CaptureDiag]   output[" << i
                                 << "] attached=" << (odesc.AttachedToDesktop ? "true" : "false")
                                 << " device=" << QString::fromWCharArray(odesc.DeviceName);
                }
            }
        }
        else
        {
            WARNING_LOG << "[CaptureDiag] Failed to query IDXGIDevice from capture hw ctx";
        }
    }

    auto* captureDevice = d3d11->device;
    if (!captureDevice)
        return "capture d3d11 device is null";
    m_converter = std::make_unique<Oran7CaptureFrameConverter>(captureDevice, m_fps);

    // 2) Build filter graph: ddagrab -> buffersink（去掉 fps filter，源头直接设置帧率）
    m_graph.reset(avfilter_graph_alloc());
    if (!m_graph)
        return "avfilter_graph_alloc failed.";

    const AVFilter* f_src = avfilter_get_by_name("ddagrab");
    const AVFilter* f_sink = avfilter_get_by_name("buffersink");
    if (!f_src || !f_sink)
        return "ddagrab/buffersink filter not found (check FFmpeg build).";

    // ddagrab options: 显式指定 framerate，不依赖 fps filter 补帧
    // output_fmt=bgra：DDA 输出 8bit BGRA（B8G8R8A8），与后续 BGRA→NV12 处理一致。
    // allow_fallback=1：请求格式不可用时降级到默认 8bit，避免直接配置失败。
    char srcArgs[256];
    snprintf(srcArgs, sizeof(srcArgs), "output_idx=%d:draw_mouse=%d:framerate=%d:output_fmt=bgra:allow_fallback=1",
             m_outputIndex, m_drawMouse ? 1 : 0, m_fps > 0 ? m_fps : 60);
    INFO_LOG << "[CaptureDiag] ddagrab args: " << srcArgs;

    ret = avfilter_graph_create_filter(&m_src, f_src, "src", srcArgs, nullptr, m_graph.get());
    if (ret < 0)
        return "create ddagrab failed: " + ffErrStr(ret);
    INFO_LOG << "[CaptureDiag] ddagrab filter created, output_idx=" << m_outputIndex;

    // m_src绑定 hwdevice
    m_src->hw_device_ctx = av_buffer_ref(m_hwdev.get());

    ret = avfilter_graph_create_filter(&m_sink, f_sink, "sink", nullptr, nullptr, m_graph.get());
    if (ret < 0)
        return "create buffersink failed: " + ffErrStr(ret);
    // m_sink 绑定 hwdevice
    m_sink->hw_device_ctx = av_buffer_ref(m_hwdev.get());

    // 直接 src -> sink，不用 fps filter 补帧
    ret = avfilter_link(m_src, 0, m_sink, 0);
    if (ret < 0)
        return "link src->sink failed: " + ffErrStr(ret);

    // 诊断：配置前打开 FFmpeg 详细日志，便于捕获 ddagrab 内部失败原因
    {
        const int oldLevel = av_log_get_level();
        av_log_set_level(AV_LOG_DEBUG);
        INFO_LOG << "[CaptureDiag] --- avfilter_graph_config start (output_idx=" << m_outputIndex << ") ---";
        ret = avfilter_graph_config(m_graph.get(), nullptr);
        av_log_set_level(oldLevel);
        if (ret < 0)
        {
            WARNING_LOG << "[CaptureDiag] --- avfilter_graph_config FAILED ret=" << ret << " ---";
            return "avfilter_graph_config failed: " + ffErrStr(ret);
        }
        INFO_LOG << "[CaptureDiag] --- avfilter_graph_config OK ---";
    }

    return {};
}

void Oran7DesktopCaptureWorker::grabLoop()
{
    Oran7CaptureTimerPeriod timerPeriod(m_fps > 60);
    INFO_LOG << "[CaptureDiag] requested_fps=" << m_fps << " timer_1ms=" << timerPeriod.active();
    QElapsedTimer performance;
    performance.start();
    qint64 rawFrames = 0, submittedFrames = 0, previewNs = 0, conversionNs = 0, conversions = 0;
    QElapsedTimer stage;
    bool firstFrame = true;
    const auto reportCaptureError = [this](const char* operation, int error)
    {
        if (m_control->stopRequested.load())
            return;
        const auto message = QString::fromLatin1(operation) + ": " + ffErrStr(error);
        if (error == AVERROR_EXTERNAL || error == AVERROR_OUTPUT_CHANGED || error == AVERROR_INPUT_CHANGED)
            emit captureInvalidated(message);
        else
            emit errorOccurred(message);
    };
    // pts 检测：确认源头是否真 60fps
    m_debugLastPts = AV_NOPTS_VALUE;
    m_rawCount = 0;
    m_rawT0 = QDateTime::currentMSecsSinceEpoch();

    while (!m_control->stopRequested.load())
    {
        // 主动请求上游产一帧（驱动 ddagrab）
        int r = avfilter_graph_request_oldest(m_graph.get());
        // INFO_LOG << "After avfilter_graph_request_oldest, ret=" << r;
        // INFO_LOG<<"avfilter_graph_request_oldest of ret:"<<r;
        if (r < 0 && r != AVERROR(EAGAIN))
        {
            reportCaptureError("request_oldest failed", r);
            return;
        }

        // 从 sink 非阻塞取出“已经到达”的帧（可能一次取多帧）
        Oran7CaptureFrameConverter::Frame frame(av_frame_alloc());
        AVFrame* f = frame.get(); // 借用；循环取消/错误返回均由 frame 释放。
        if (!f)
        {
            emit errorOccurred("av_frame_alloc failed");
            return;
        }
        while (!m_control->stopRequested.load())
        {
            av_frame_unref(f); // unref old frame

            int ret = av_buffersink_get_frame_flags(m_sink, f, AV_BUFFERSINK_FLAG_NO_REQUEST);
            // INFO_LOG<<"RUN av_buffersink_get_frame_flags of RET:"<<ret;
            if (ret == AVERROR(EAGAIN))
            {
                // INFO_LOG<<"av_buffersink_get_frame_flags(m_sink, f, AV_BUFFERSINK_FLAG_NO_REQUEST) of RET:EAGAIN.";
                break; // 本轮没有帧可取，回到上面继续 request
            }
            if (ret < 0)
            {
                reportCaptureError("get_frame failed", ret);
                return;
            }

            // === Copy/VP 到 sharedTex===
            auto* srcTex = reinterpret_cast<ID3D11Texture2D*>(f->data[0]);
            if (srcTex)
            {
                //------- pts 检测：确认源头真帧率 -------//
                if (m_control->debugLog.load(std::memory_order_relaxed))
                {
                    m_rawCount++;
                    if (f->pts == m_debugLastPts && m_debugLastPts != AV_NOPTS_VALUE)
                    {
                        INFO_LOG << "DUP frame pts=" << f->pts << " rawCount=" << m_rawCount;
                    }
                    m_debugLastPts = f->pts;

                    auto rawNow = QDateTime::currentMSecsSinceEpoch();
                    if (rawNow - m_rawT0 >= 1000)
                    {
                        INFO_LOG << "sink raw fps=" << (m_rawCount * 1000 / (rawNow - m_rawT0)) << " pts=" << f->pts
                                 << " rawCount=" << m_rawCount << ",persecond.";
                        m_rawCount = 0;
                        m_rawT0 = rawNow;
                    }
                }
                //------------------------------------------//

                m_recordingFeed->publish(f->width, f->height, m_hwdev.get());
                ++rawFrames;
                stage.start();
                auto preview = m_converter->preview(f);
                previewNs += stage.nsecsElapsed();
                if (!preview.error.isEmpty())
                {
                    if (preview.invalidated)
                        emit captureInvalidated(preview.error);
                    else
                        emit errorOccurred(preview.error);
                    return;
                }
                if (preview.frame && m_submitFrame && !m_control->stopRequested.load())
                    m_submitFrame(preview.frame.release());
                if (firstFrame)
                {
                    firstFrame = false;
                    emit firstFrameReady();
                }

                // ---- 录制/推流路径 ----
                const auto recording = m_recordingFeed->snapshot();
                const auto encoder = recording.encoder;
                const bool recEnabled = bool(encoder);
                if (recEnabled)
                {
                    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count();
                    if (!m_lastRecEnabled || m_recordingEpoch != recording.epoch)
                    {
                        m_converter->beginRecording();
                        m_recordingEpoch = recording.epoch;
                        m_nextRecordUs = nowUs;
                        m_recStartUs = nowUs;
                        m_lastPts = AV_NOPTS_VALUE;
                        m_lastRecEnabled = true;
                    }
                    int visW = f->width;
                    int visH = f->height;
                    int targetW = recording.targetWidth;
                    int targetH = recording.targetHeight;
                    int encW = (targetW > 0) ? targetW : visW;
                    int encH = (targetH > 0) ? targetH : visH;

                    // 用 av_hwframe_transfer_data 下载 D3D11 帧（正确处理 data[1] 子资源）
                    // 一趟 BGRA → NV12 到编码器分辨率
                    Oran7CaptureFrameConverter::Frame encFrame;
                    if (nowUs + 1000 >= m_nextRecordUs)
                    {
                        stage.start();
                        encFrame = m_converter->recording(f, encW, encH);
                        conversionNs += stage.nsecsElapsed();
                        ++conversions;
                    }
                    if (encFrame && encoder && encoder->isRuning())
                    {
                        int64_t pts = (nowUs - m_recStartUs) * 90'000 / 1'000'000;
                        if (pts < 0)
                            pts = 0;
                        if (m_lastPts != AV_NOPTS_VALUE && pts <= m_lastPts)
                            pts = m_lastPts + 1;
                        m_lastPts = pts;
                        encFrame->pts = pts;
                        encFrame->duration =
                            av_rescale_q(1, AVRational{1, qMax(1, recording.fps)}, AVRational{1, 90000});
                        if (pts == 1)
                            encFrame->pict_type = AV_PICTURE_TYPE_I;

                        m_nextRecordUs = qMax(m_nextRecordUs + 1000000 / qMax(1, recording.fps), nowUs);
                        encoder->pushFrame(encFrame.release());
                        ++submittedFrames;
                    }
                }
                else
                {
                    if (m_lastRecEnabled)
                        m_converter->endRecording();
                    m_lastRecEnabled = false;
                }
                if (performance.elapsed() >= 3000)
                {
                    const double seconds = performance.nsecsElapsed() / 1e9;
                    if (Oran7GlobalHelper::developerMode())
                    {
                        INFO_LOG << "[CapturePerf] target_capture_fps=" << m_fps << "raw_fps=" << rawFrames / seconds
                                 << "submitted_fps=" << submittedFrames / seconds
                                 << "preview_avg_ms=" << (rawFrames ? previewNs / (rawFrames * 1e6) : 0.0)
                                 << "conversion_avg_ms=" << (conversions ? conversionNs / (conversions * 1e6) : 0.0)
                                 << "encoder_dropped_total=" << (encoder ? encoder->droppedFrameCount() : 0)
                                 << "packet_queue=" << (encoder ? encoder->queuedPktCount() : 0);
                    }
                    rawFrames = submittedFrames = previewNs = conversionNs = conversions = 0;
                    performance.restart();
                }
            }
        }
    }
    INFO_LOG << "Oran7ScreenCapture grabLopp thread leave.";
}

void Oran7DesktopCaptureWorker::cleanup()
{
    m_recordingFeed->close();
    // 先释放转换器及其设备引用，再释放过滤图和 FFmpeg 设备。
    m_converter.reset();
    m_debugLastPts = AV_NOPTS_VALUE;
    m_rawCount = 0;
    m_rawT0 = 0;
    m_recStartUs = 0;
    m_lastPts = AV_NOPTS_VALUE;
    m_lastRecEnabled = false;
    m_graph.reset();
    m_src = nullptr;
    m_sink = nullptr;
    m_hwdev.reset();
}
