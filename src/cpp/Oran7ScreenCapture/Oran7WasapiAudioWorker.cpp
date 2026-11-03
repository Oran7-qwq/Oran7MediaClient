// MinGW 的 WASAPI GUID 在实际使用的翻译单元定义，不依赖采集控制器头间接生成。
#include <initguid.h>
#include "Oran7WasapiAudioWorker.h"
#include "Oran7GlobalHelper.h"
#include <chrono>
#include <stdexcept>
#include <cstring>
extern "C" {
#include <libavutil/opt.h>
}

/**
 * ======================================================
 * @note Windows桌面音频捕获器 - Oran7WasapiAudioWorker
 * ======================================================
 */

// WAVEFORMATEX wFormatTag → AVSampleFormat
static AVSampleFormat waveToSampleFmt(const WAVEFORMATEX* wf)
{
    if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return AV_SAMPLE_FMT_FLT;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        // WAVE_FORMAT_EXTENSIBLE：从 SubFormat GUID 判断
        // MinGW 链接 GUID 有限，这里用 wBitsPerSample + cbSize 推断：
        //   cbSize >= 22 且 wBitsPerSample == 32 → 大概率 float
        //   否则 PCM，按位深走
        if (wf->wBitsPerSample == 32 && wf->cbSize >= 22)
            return AV_SAMPLE_FMT_FLT;
        return wf->wBitsPerSample == 32 ? AV_SAMPLE_FMT_S32 : AV_SAMPLE_FMT_S16;
    }
    // 普通 PCM
    return wf->wBitsPerSample == 32 ? AV_SAMPLE_FMT_S32 : AV_SAMPLE_FMT_S16;
}

// WAVEFORMATEX → AVChannelLayout
static void waveToChannelLayout(const WAVEFORMATEX* wf,AVChannelLayout* out)
{
    av_channel_layout_default(out, wf->nChannels);
}

bool Oran7WasapiAudioWorker::start(const Oran7WasapiConfig &cfg)
{
    if(_running.exchange(true)) return true;
    _cfg = cfg;
    _thread = std::thread(&Oran7WasapiAudioWorker::captureLoop,this);
    return true;
}

void Oran7WasapiAudioWorker::stop()
{
    _running.store(false);
    _frameCond.notify_all();
    if(_thread.joinable())
        _thread.join();

    //清空帧队列
    while(!_frameQueue.empty()){
        av_frame_free(&_frameQueue.front());
        _frameQueue.pop();
    }
}

bool Oran7WasapiAudioWorker::isRunning() const
{
    return _running.load();
}

bool Oran7WasapiAudioWorker::getPcmFrame(AVFrame **frame, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(_frameQueue_mtx);
    if(!_frameCond.wait_for(lock,std::chrono::milliseconds(timeoutMs),[this](){
            return !_frameQueue.empty() || !_running.load();
        }))
    {
        return false;
    }
    if (_frameQueue.empty()) return false;
    *frame = _frameQueue.front();
    _frameQueue.pop();
    return true;
}

void Oran7WasapiAudioWorker::captureLoop()
{
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);  // WASAPI 要 MTA
    if(!initWASAPI())
    {
        _running.store(false);
        _frameCond.notify_all();
        CoUninitialize();
        return;
    }
    if (FAILED(_audioClient->Start())) {
        _running.store(false); _frameCond.notify_all(); cleanupWASAPI(); CoUninitialize(); return;
    }

    /*! @note 事件驱动采集（WASAPI loopback 不支持轮询 GetNextPacketSize）*/
    _recStartUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    BYTE*    capBuf    = nullptr;
    UINT32   capFrames = 0;
    DWORD    capFlags  = 0;

    while (_running.load()) {
        DWORD waitRet = WaitForSingleObject(_hEvent, 100);  // 100ms 超时
        if (waitRet == WAIT_TIMEOUT)
            continue;
        if (waitRet != WAIT_OBJECT_0)
            break;

        // 一次可能有多个包，循环取完
        for (;;) {
            HRESULT hr = _captureClient->GetBuffer(&capBuf, &capFrames, &capFlags,
                                                    nullptr, nullptr);
            if (FAILED(hr)) {
                if (hr == AUDCLNT_S_BUFFER_EMPTY)
                    break;
                break;
            }
            if (capFrames == 0)
                break;

            bool silent = (capFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

            AVFrame *f = av_frame_alloc();
            f->format      = AV_SAMPLE_FMT_S16;
            f->sample_rate = _cfg.sampleRate;
            f->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;
            f->nb_samples  = av_rescale_rnd(capFrames, _cfg.sampleRate,
                                            _deviceFormat->nSamplesPerSec, AV_ROUND_UP);
            // PTS：wall clock 算到 48kHz 时间基，和视频同源
            auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            f->pts = (nowUs - _recStartUs) * _cfg.sampleRate / 1'000'000;
            av_frame_get_buffer(f, 0);

            if (silent) {
                av_samples_set_silence(f->data, 0, f->nb_samples,
                                       _cfg.channels, AV_SAMPLE_FMT_S16);
            } else {
                const uint8_t* src = capBuf;
                int actual = swr_convert(_swr, f->data, f->nb_samples,
                                         &src, capFrames);
                if (actual >= 0)
                    f->nb_samples = actual;
            }

            _captureClient->ReleaseBuffer(capFrames);
            capFrames = 0;

            // 入队
            {
                std::lock_guard<std::mutex> lock(_frameQueue_mtx);
                if (_frameQueue.size() >= kMaxQueued) {
                    av_frame_free(&_frameQueue.front());
                    _frameQueue.pop();
                }
                _frameQueue.push(f);
                _frameCond.notify_one();
            }
        }
    }

    //cleanup
    _audioClient->Stop();
    cleanupWASAPI();
    CoUninitialize();
}

bool Oran7WasapiAudioWorker::initWASAPI()
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    AVChannelLayout outChLayout, inChLayout;
    bool layoutInited = false, layoutOutInited = false;

    try {
        HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                      IID_IMMDeviceEnumerator, (void**)&enumerator);
        if (FAILED(hr))
            throw std::runtime_error("CoCreateInstance(MMDeviceEnumerator) failed");

        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr))
            throw std::runtime_error("GetDefaultAudioEndpoint failed");

        hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&_audioClient);
        if (FAILED(hr))
            throw std::runtime_error("Activate(IAudioClient) failed");

        hr = _audioClient->GetMixFormat(&_deviceFormat);
        if (FAILED(hr))
            throw std::runtime_error("GetMixFormat failed");

        REFERENCE_TIME hnsPeriod = _cfg.frameMs * 10000LL;  // 10ms → 100000ns
        hr = _audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                       AUDCLNT_STREAMFLAGS_LOOPBACK |
                                       AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                       hnsPeriod, 0, _deviceFormat, nullptr);
        if (FAILED(hr))
            throw std::runtime_error("IAudioClient::Initialize(LOOPBACK) failed");

        hr = _audioClient->GetService(IID_IAudioCaptureClient, (void**)&_captureClient);
        if (FAILED(hr))
            throw std::runtime_error("GetService(IAudioCaptureClient) failed");

        // loopback 必须用事件驱动，不能轮询 GetNextPacketSize
        _hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!_hEvent)
            throw std::runtime_error("CreateEvent failed");
        hr = _audioClient->SetEventHandle(_hEvent);
        if (FAILED(hr))
            throw std::runtime_error("SetEventHandle failed");

        // swr 上下文：设备格式 → S16/48kHz/stereo
        av_channel_layout_default(&outChLayout, _cfg.channels);
        layoutOutInited = true;

        waveToChannelLayout(_deviceFormat, &inChLayout);
        layoutInited = true;

        AVSampleFormat inFmt = waveToSampleFmt(_deviceFormat);

        int ret = swr_alloc_set_opts2(&_swr,
                                      &outChLayout, AV_SAMPLE_FMT_S16, _cfg.sampleRate,
                                      &inChLayout, inFmt, _deviceFormat->nSamplesPerSec,
                                      0, nullptr);
        if (ret < 0 || !_swr)
            throw std::runtime_error("swr_alloc_set_opts2 failed");

        ret = swr_init(_swr);
        if (ret < 0)
            throw std::runtime_error("swr_init failed");

        // 成功——先清理本地资源，再返回
        av_channel_layout_uninit(&outChLayout);
        av_channel_layout_uninit(&inChLayout);
        enumerator->Release();
        device->Release();
        return true;
    }
    catch (const std::exception& e) {
        ERROR_LOG << "Oran7WasapiAudioWorker::initWASAPI() : " << e.what();

        // 释放已创建的部分资源（cleanupWASAPI 不碰本地变量）
        cleanupWASAPI();
        if (layoutOutInited) av_channel_layout_uninit(&outChLayout);
        if (layoutInited)       av_channel_layout_uninit(&inChLayout);
        if (enumerator)       enumerator->Release();
        if (device)                device->Release();
        return false;
    }
}

void Oran7WasapiAudioWorker::cleanupWASAPI()
{
    if (_captureClient) {
        _captureClient->Release();
        _captureClient = nullptr;
    }
    if (_audioClient) {
        _audioClient->Release();
        _audioClient = nullptr;
    }
    if (_deviceFormat) {
        CoTaskMemFree(_deviceFormat);
        _deviceFormat = nullptr;
    }
    if (_swr) {
        swr_free(&_swr);
        _swr = nullptr;
    }
    if (_hEvent) {
        CloseHandle(_hEvent);
        _hEvent = nullptr;
    }
}
