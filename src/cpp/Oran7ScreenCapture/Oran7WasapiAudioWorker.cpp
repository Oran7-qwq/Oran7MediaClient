// MinGW 的 WASAPI GUID 在实际使用的翻译单元定义，不依赖采集控制器头间接生成。
#include <initguid.h>
#include "Oran7WasapiAudioWorker.h"
#include "Oran7GlobalHelper.h"
#include <chrono>
#include <stdexcept>
#include <cstring>
extern "C"
{
#include <libavutil/opt.h>
}

/**
 * ======================================================
 * @note Windows桌面音频捕获器 - Oran7WasapiAudioWorker
 * ======================================================
 */

bool Oran7WasapiAudioWorker::start(const Oran7WasapiConfig& cfg)
{
    if (_running.exchange(true))
        return true;
    if (_thread.joinable())
        _thread.join();
    _droppedFrames.store(0);
    _discontinuities.store(0);
    _cfg = cfg;
    _thread = std::thread(&Oran7WasapiAudioWorker::captureLoop, this);
    return true;
}

void Oran7WasapiAudioWorker::stop()
{
    _running.store(false);
    _frameCond.notify_all();
    if (_thread.joinable())
        _thread.join();

    // 清空帧队列
    while (!_frameQueue.empty())
    {
        av_frame_free(&_frameQueue.front());
        _frameQueue.pop();
    }
}

bool Oran7WasapiAudioWorker::isRunning() const
{
    return _running.load();
}

bool Oran7WasapiAudioWorker::getPcmFrame(AVFrame** frame, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(_frameQueue_mtx);
    if (!_frameCond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                             [this]() { return !_frameQueue.empty() || !_running.load(); }))
    {
        return false;
    }
    if (_frameQueue.empty())
        return false;
    *frame = _frameQueue.front();
    _frameQueue.pop();
    return true;
}

void Oran7WasapiAudioWorker::captureLoop()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED); // WASAPI 要 MTA
    if (!initWASAPI())
    {
        _running.store(false);
        _frameCond.notify_all();
        CoUninitialize();
        return;
    }
    if (FAILED(_audioClient->Start()))
    {
        _running.store(false);
        _frameCond.notify_all();
        cleanupWASAPI();
        CoUninitialize();
        return;
    }

    // 事件驱动采集，每次唤醒取完已到达的音频包。
    _recStartUs =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();

    BYTE* capBuf = nullptr;
    UINT32 capFrames = 0;
    DWORD capFlags = 0;
    UINT64 devicePosition = 0, qpcPosition = 0, expectedDevicePosition = 0;
    int64_t nextOutputPts = AV_NOPTS_VALUE;

    while (_running.load())
    {
        DWORD waitRet = WaitForSingleObject(_hEvent, 100); // 100ms 超时
        if (waitRet == WAIT_TIMEOUT)
            continue;
        if (waitRet != WAIT_OBJECT_0)
            break;

        // 一次可能有多个包，循环取完
        for (;;)
        {
            HRESULT hr = _captureClient->GetBuffer(&capBuf, &capFrames, &capFlags, &devicePosition, &qpcPosition);
            if (hr == AUDCLNT_S_BUFFER_EMPTY)
                break;
            if (FAILED(hr))
            {
                ERROR_LOG << "[WASAPIAudio] GetBuffer failed, hr=" << quint32(hr);
                _running.store(false);
                break;
            }
            if (capFrames == 0)
            {
                _captureClient->ReleaseBuffer(0);
                break;
            }

            bool silent = (capFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
            if (capFlags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
                ++_discontinuities;

            if (_cfg.clockStart100ns && (nextOutputPts == AV_NOPTS_VALUE || devicePosition != expectedDevicePosition))
            {
                // GetBuffer 的 QPC 已是 100ns 单位，不是原始计数器 tick。
                const int64_t packetTime =
                    (capFlags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
                        ? clockNow100ns() - av_rescale(capFrames, 10000000, _deviceFormat->nSamplesPerSec)
                        : int64_t(qpcPosition);
                nextOutputPts = av_rescale(packetTime - _cfg.clockStart100ns, _cfg.sampleRate, 10000000) -
                                _converter.delaySamples();
            }
            expectedDevicePosition = devicePosition + capFrames;

            AVFrame* f = _converter.convert(capBuf, int(capFrames), silent);
            _captureClient->ReleaseBuffer(capFrames);
            capFrames = 0;
            if (!f)
            {
                ERROR_LOG << "[WASAPIAudio] PCM conversion failed; stopping capture";
                _running.store(false);
                break;
            }
            // 保留采集时钟；直播端由 AAC 样本时钟连续推进，不将回调抖动注入输出。
            const auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
            f->pts = _cfg.clockStart100ns ? nextOutputPts : (nowUs - _recStartUs) * _cfg.sampleRate / 1'000'000;
            if (_cfg.clockStart100ns)
                nextOutputPts += f->nb_samples;
            if (!f->nb_samples)
            {
                av_frame_free(&f);
                continue;
            }

            // 入队
            {
                std::lock_guard<std::mutex> lock(_frameQueue_mtx);
                if (_frameQueue.size() >= kMaxQueued)
                {
                    ++_droppedFrames;
                    av_frame_free(&_frameQueue.front());
                    _frameQueue.pop();
                }
                _frameQueue.push(f);
                _frameCond.notify_one();
            }
        }
    }

    _running.store(false);
    _frameCond.notify_all();
    // cleanup
    _audioClient->Stop();
    cleanupWASAPI();
    CoUninitialize();
}

bool Oran7WasapiAudioWorker::initWASAPI()
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;

    try
    {
        HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator,
                                      (void**)&enumerator);
        if (FAILED(hr))
            throw std::runtime_error("CoCreateInstance(MMDeviceEnumerator) failed");

        hr = enumerator->GetDefaultAudioEndpoint(_cfg.microphone ? eCapture : eRender, eConsole, &device);
        if (FAILED(hr))
            throw std::runtime_error("GetDefaultAudioEndpoint failed");

        hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&_audioClient);
        if (FAILED(hr))
            throw std::runtime_error("Activate(IAudioClient) failed");

        hr = _audioClient->GetMixFormat(&_deviceFormat);
        if (FAILED(hr))
            throw std::runtime_error("GetMixFormat failed");

        REFERENCE_TIME hnsPeriod = _cfg.frameMs * 10000LL; // 10ms → 100000ns
        hr = _audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                      (_cfg.microphone ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK) |
                                          AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                      hnsPeriod, 0, _deviceFormat, nullptr);
        if (FAILED(hr))
            throw std::runtime_error("IAudioClient::Initialize(LOOPBACK) failed");

        hr = _audioClient->GetService(IID_IAudioCaptureClient, (void**)&_captureClient);
        if (FAILED(hr))
            throw std::runtime_error("GetService(IAudioCaptureClient) failed");

        // 为事件驱动 loopback 注册唤醒句柄。
        _hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!_hEvent)
            throw std::runtime_error("CreateEvent failed");
        hr = _audioClient->SetEventHandle(_hEvent);
        if (FAILED(hr))
            throw std::runtime_error("SetEventHandle failed");

        if (!_converter.configure(_deviceFormat, _cfg.sampleRate, _cfg.channels))
            throw std::runtime_error("Unsupported WASAPI PCM format or resampler initialization failed");
        INFO_LOG << "[WASAPIAudio] source=" << (_cfg.microphone ? "microphone" : "system")
                 << "inputHz=" << _deviceFormat->nSamplesPerSec << "channels=" << _deviceFormat->nChannels
                 << "bits=" << _deviceFormat->wBitsPerSample
                 << "sampleFormat=" << av_get_sample_fmt_name(_converter.inputFormat())
                 << "outputHz=" << _cfg.sampleRate;

        enumerator->Release();
        device->Release();
        return true;
    }
    catch (const std::exception& e)
    {
        ERROR_LOG << "Oran7WasapiAudioWorker::initWASAPI() : " << e.what();

        // 释放已创建的部分资源（cleanupWASAPI 不碰本地变量）
        cleanupWASAPI();
        if (enumerator)
            enumerator->Release();
        if (device)
            device->Release();
        return false;
    }
}

void Oran7WasapiAudioWorker::cleanupWASAPI()
{
    if (_captureClient)
    {
        _captureClient->Release();
        _captureClient = nullptr;
    }
    if (_audioClient)
    {
        _audioClient->Release();
        _audioClient = nullptr;
    }
    if (_deviceFormat)
    {
        CoTaskMemFree(_deviceFormat);
        _deviceFormat = nullptr;
    }
    if (_hEvent)
    {
        CloseHandle(_hEvent);
        _hEvent = nullptr;
    }
}
