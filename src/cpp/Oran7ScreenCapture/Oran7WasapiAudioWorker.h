#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
extern "C" {
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

// =============== WASPAIAudioWorker (runs in its own QThread) =================
/**
 *  @brief Windows桌面音频采集器
 */
class Oran7WasapiAudioWorker{
public:
    ~Oran7WasapiAudioWorker() { stop(); }
    struct Oran7WasapiConfig
    {
        int sampleRate  = 48000;
        int channels = 2;
        int frameMs = 10; // WASAPI 帧周期
    };

    bool start(const Oran7WasapiConfig& cfg);
    void stop();
    bool isRunning() const;

    /**
     * @brief 阻塞拉取 PCM 帧（S16 interleaved, 48000Hz stereo）
     * @note 调用方负责
     * @param timeoutMs 阻塞超时阈值
     * @return av_frame_free
     */
    bool getPcmFrame(AVFrame** frame, int timeoutMs = 100);
private:
    void captureLoop();
    bool initWASAPI();
    void cleanupWASAPI();

    // WASAPI COM 接口
    IAudioClient*          _audioClient   = nullptr;
    IAudioCaptureClient*   _captureClient = nullptr;
    WAVEFORMATEX*          _deviceFormat  = nullptr;  // 设备原生格式
    HANDLE                 _hEvent        = nullptr;  // loopback 事件

    // 格式转换：设备格式 → S16/48kHz/stereo
    SwrContext* _swr = nullptr;

    // PCM 帧队列
    std::mutex              _frameQueue_mtx;
    std::condition_variable _frameCond;
    std::queue<AVFrame*>    _frameQueue;
    static constexpr int    kMaxQueued = 10;

    std::thread                _thread;
    std::atomic<bool>  _running{false};
    Oran7WasapiConfig         _cfg;
    int64_t               _recStartUs = 0;  // wall clock
};

