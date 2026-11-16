#pragma once
#include <windows.h>
#include <mmreg.h>
#include <vector>
extern "C"
{
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

// 仅转换值数据，不访问音频设备；静音和普通包共用同一重采样状态。
class Oran7WasapiPcmConverter
{
  public:
    Oran7WasapiPcmConverter() = default;
    Oran7WasapiPcmConverter(const Oran7WasapiPcmConverter&) = delete;
    Oran7WasapiPcmConverter& operator=(const Oran7WasapiPcmConverter&) = delete;
    ~Oran7WasapiPcmConverter();
    bool configure(const WAVEFORMATEX* input, int sampleRate, int channels);
    // 返回新帧，调用者释放；失败返回 nullptr，不提交未初始化的 PCM。
    AVFrame* convert(const uint8_t* data, int samples, bool silent);
    AVSampleFormat inputFormat() const { return m_format; }
    int64_t delaySamples() const { return m_swr ? swr_get_delay(m_swr, m_outputRate) : 0; }

  private:
    SwrContext* m_swr = nullptr;
    AVSampleFormat m_format = AV_SAMPLE_FMT_NONE;
    int m_inputRate = 0, m_outputRate = 0, m_inputChannels = 0, m_outputChannels = 0;
    bool m_packed24 = false;
    std::vector<uint8_t> m_silence;
    std::vector<int32_t> m_unpacked;
};
