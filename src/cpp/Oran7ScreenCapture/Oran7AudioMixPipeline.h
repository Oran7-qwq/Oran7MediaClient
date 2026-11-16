#pragma once
#include "Oran7AudioEncoder.h"
#include "Live/Oran7LiveAudioMixer.h"
class Oran7AudioCaptureFeed;

// 本地录制和 RTMP 各自持有实例，共用采集/公共时钟/混音实现。
// stop 只停止输入和混音线程；调用方随后停止编码器、排空封装器。
class Oran7AudioMixPipeline
{
  public:
    enum class Error
    {
        None,
        System,
        Microphone,
        Mixing
    };
    ~Oran7AudioMixPipeline();
    bool start(std::shared_ptr<Oran7AudioEncoder>, std::shared_ptr<Oran7LiveAudioControl>);
    bool startSource(std::shared_ptr<Oran7LiveAudioControl>, std::shared_ptr<Oran7AudioCaptureFeed>);
    void stop(bool drain = false);
    Error error() const { return m_error.load(); }
    uint64_t captureDrops() const { return m_captureDrops.load(); }
    uint64_t microphoneDrops() const { return m_microphoneDrops.load(); }
    uint64_t discontinuities() const { return m_discontinuities.load() + m_microphoneDiscontinuities.load(); }

  private:
    void run();
    bool startThread(std::shared_ptr<Oran7LiveAudioControl>);
    std::shared_ptr<Oran7AudioCaptureFeed> m_feed;
    std::shared_ptr<Oran7AudioEncoder> m_encoder;
    std::shared_ptr<Oran7LiveAudioControl> m_control;
    std::atomic_bool m_running{false};
    std::atomic<Error> m_error{Error::None};
    std::atomic<uint64_t> m_captureDrops{0}, m_discontinuities{0}, m_microphoneDrops{0}, m_microphoneDiscontinuities{0};
    int64_t m_origin = 0;
    std::atomic<int64_t> m_finishPts{-1};
    std::thread m_thread;
};
