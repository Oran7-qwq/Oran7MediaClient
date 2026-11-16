#include "Oran7WasapiPcmConverter.h"
#include <ks.h>
#include <ksmedia.h>
#include <cstring>
#include <limits>
extern "C"
{
#include <libavutil/mathematics.h>
}

Oran7WasapiPcmConverter::~Oran7WasapiPcmConverter()
{
    swr_free(&m_swr);
}

bool Oran7WasapiPcmConverter::configure(const WAVEFORMATEX* input, int sampleRate, int channels)
{
    swr_free(&m_swr);
    m_format = AV_SAMPLE_FMT_NONE;
    m_packed24 = false;
    if (!input || !input->nSamplesPerSec || input->nSamplesPerSec > 768000 || input->nChannels < 1 ||
        input->nChannels > 32 || sampleRate < 8000 || sampleRate > 192000 || channels < 1 || channels > 8)
        return false;
    bool floating = input->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    bool pcm = input->wFormatTag == WAVE_FORMAT_PCM;
    DWORD mask = 0;
    if (input->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        if (input->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
            return false;
        const auto* ex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(input);
        // 位深不能区分 PCM32 和 float32，必须严格检查 SubFormat GUID。
        const GUID pcmGuid = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
        const GUID floatGuid = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
        floating = IsEqualGUID(ex->SubFormat, floatGuid);
        pcm = IsEqualGUID(ex->SubFormat, pcmGuid);
        mask = ex->dwChannelMask;
        if (pcm && (!ex->Samples.wValidBitsPerSample || ex->Samples.wValidBitsPerSample > input->wBitsPerSample))
            return false;
    }
    if (floating && input->wBitsPerSample == 32)
        m_format = AV_SAMPLE_FMT_FLT;
    else if (pcm)
    {
        switch (input->wBitsPerSample)
        {
        case 8:
            m_format = AV_SAMPLE_FMT_U8;
            break;
        case 16:
            m_format = AV_SAMPLE_FMT_S16;
            break;
        case 24:
            m_format = AV_SAMPLE_FMT_S32;
            m_packed24 = true;
            break;
        case 32:
            m_format = AV_SAMPLE_FMT_S32;
            break;
        }
    }
    if (m_format == AV_SAMPLE_FMT_NONE || input->nBlockAlign != input->nChannels * (input->wBitsPerSample / 8))
        return false;
    AVChannelLayout in{}, out{};
    if (mask)
    {
        if (av_channel_layout_from_mask(&in, mask) < 0 || in.nb_channels != input->nChannels)
        {
            av_channel_layout_uninit(&in);
            return false;
        }
    }
    else
        av_channel_layout_default(&in, input->nChannels);
    av_channel_layout_default(&out, channels);
    const int ret = swr_alloc_set_opts2(&m_swr, &out, AV_SAMPLE_FMT_S16, sampleRate, &in, m_format,
                                        input->nSamplesPerSec, 0, nullptr);
    av_channel_layout_uninit(&in);
    av_channel_layout_uninit(&out);
    if (ret < 0 || !m_swr || swr_init(m_swr) < 0)
    {
        swr_free(&m_swr);
        return false;
    }
    m_inputRate = input->nSamplesPerSec;
    m_inputChannels = input->nChannels;
    m_outputRate = sampleRate;
    m_outputChannels = channels;
    return true;
}

AVFrame* Oran7WasapiPcmConverter::convert(const uint8_t* data, int samples, bool silent)
{
    if (!m_swr || samples < 1 || samples > m_inputRate * 2 || (!silent && !data))
        return nullptr;
    const size_t count = size_t(samples) * m_inputChannels;
    if (silent)
    {
        m_silence.assign(count * av_get_bytes_per_sample(m_format), m_format == AV_SAMPLE_FMT_U8 ? 128 : 0);
        data = m_silence.data();
    }
    else if (m_packed24)
    {
        m_unpacked.resize(count);
        for (size_t i = 0; i < count; ++i)
        {
            // Windows PCM24 为小端；左对齐到 S32，保留符号和满量程。
            const uint32_t value =
                uint32_t(data[i * 3]) << 8 | uint32_t(data[i * 3 + 1]) << 16 | uint32_t(data[i * 3 + 2]) << 24;
            std::memcpy(&m_unpacked[i], &value, sizeof(value));
        }
        data = reinterpret_cast<const uint8_t*>(m_unpacked.data());
    }
    const auto capacity =
        av_rescale_rnd(swr_get_delay(m_swr, m_inputRate) + samples, m_outputRate, m_inputRate, AV_ROUND_UP);
    if (capacity < 1 || capacity > std::numeric_limits<int>::max())
        return nullptr;
    auto* frame = av_frame_alloc();
    if (!frame)
        return nullptr;
    frame->format = AV_SAMPLE_FMT_S16;
    frame->sample_rate = m_outputRate;
    av_channel_layout_default(&frame->ch_layout, m_outputChannels);
    frame->nb_samples = int(capacity);
    if (av_frame_get_buffer(frame, 0) < 0)
    {
        av_frame_free(&frame);
        return nullptr;
    }
    const int actual = swr_convert(m_swr, frame->data, frame->nb_samples, &data, samples);
    if (actual < 0)
    {
        av_frame_free(&frame);
        return nullptr;
    }
    frame->nb_samples = actual;
    return frame;
}
