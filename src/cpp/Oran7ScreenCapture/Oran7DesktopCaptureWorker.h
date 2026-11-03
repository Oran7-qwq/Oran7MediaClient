#pragma once
#include "Oran7RecordingFeed.h"
#include "Oran7CaptureControl.h"
#include "Oran7CaptureFrameConverter.h"
#include <QObject>
#include <atomic>
#include <functional>
extern "C"
{
#include <libavfilter/avfilter.h>
}

// GUI 只构造空壳并 moveToThread；图、设备和转换器只在采集线程建立/销毁。
// stop/setDebugLog 仅写原子值，可由 GUI 直接调用；其余方法归采集线程。
class Oran7DesktopCaptureWorker : public QObject
{
    Q_OBJECT
  public:
    Oran7DesktopCaptureWorker(std::function<void(AVFrame*)> submitFrame, int outputIndex, bool drawMouse, int fps);
    ~Oran7DesktopCaptureWorker() override;
    void setDebugLog(bool on) { m_control->debugLog.store(on, std::memory_order_release); }
    std::shared_ptr<Oran7CaptureControl> control() const { return m_control; }
    std::shared_ptr<Oran7RecordingFeed> recordingFeed() const { return m_recordingFeed; }
  public slots:
    void start();
    void stop();
  signals:
    void started();
    void stopped();
    void errorOccurred(const QString& err);
    void captureInvalidated(const QString& reason);
    void firstFrameReady();

  private:
    QString initGraph();
    static int findDesktopAdapterIndex();
    void grabLoop();
    void cleanup();
    std::function<void(AVFrame*)> m_submitFrame;
    int m_outputIndex;
    bool m_drawMouse;
    int m_fps;
    std::shared_ptr<Oran7CaptureControl> m_control = std::make_shared<Oran7CaptureControl>();
    bool m_started = false; // 仅采集线程访问。
    // graph 拥有 src/sink；hwdev 覆盖转换器生命周期，cleanup 可重复调用。
    struct Oran7GraphDeleter
    {
        void operator()(AVFilterGraph* graph) const { avfilter_graph_free(&graph); }
    };
    struct Oran7BufferDeleter
    {
        void operator()(AVBufferRef* buffer) const { av_buffer_unref(&buffer); }
    };
    std::unique_ptr<AVFilterGraph, Oran7GraphDeleter> m_graph;
    AVFilterContext* m_src = nullptr;
    AVFilterContext* m_sink = nullptr;
    std::unique_ptr<AVBufferRef, Oran7BufferDeleter> m_hwdev;
    std::unique_ptr<Oran7CaptureFrameConverter> m_converter;
    std::shared_ptr<Oran7RecordingFeed> m_recordingFeed = std::make_shared<Oran7RecordingFeed>();
    quint64 m_recordingEpoch = 0;
    int64_t m_nextRecordUs = 0, m_recStartUs = 0, m_lastPts = AV_NOPTS_VALUE;
    bool m_lastRecEnabled = false;
    int64_t m_debugLastPts = AV_NOPTS_VALUE;
    int m_rawCount = 0;
    qint64 m_rawT0 = 0;
};
