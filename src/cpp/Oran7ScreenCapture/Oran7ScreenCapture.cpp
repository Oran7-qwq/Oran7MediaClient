#include "Oran7ScreenCapture.h"
#include "D3d11DeviceProvider.h"

#include <dxgi.h>
#include <libavutil/log.h>


// ================= Controller (lives in main/GUI thread) =================
Oran7ScreenCaptureController::Oran7ScreenCaptureController(QObject *parent)
    : QObject(parent)
{
    av_log_set_level(AV_LOG_INFO);

#ifdef _WIN32
    auto* p = D3D11DeviceProvider::instance();

    connect(p, &D3D11DeviceProvider::deviceReady,
            this, &Oran7ScreenCaptureController::onProviderReady,
            Qt::UniqueConnection);

    connect(p, &D3D11DeviceProvider::deviceLost,
            this, &Oran7ScreenCaptureController::onProviderLost,
            Qt::UniqueConnection);
#endif
}

Oran7ScreenCaptureController::~Oran7ScreenCaptureController()
{
    stop();
}

void Oran7ScreenCaptureController::setVideoItem(QObject* item)
{
    auto* vi = qobject_cast<D3D11VideoItem*>(item);
    if (!vi) {
        emit errorOccurred("setVideoItem: object is not a D3D11VideoItem.");
        return;
    }

    if (m_item == vi) return;

    stop();
    INFO_LOG<<"Oran7ScreenCaptureController Successed set videoItem.";
    m_item = vi;
}

bool Oran7ScreenCaptureController::start()
{
    INFO_LOG<<"Oran7ScreenCaptureController request for start().";
    if (m_running.exchange(true)) return true;

    if (!m_item) {
        m_running.store(false);
        WARNING_LOG<<"start: video item not set.";
        emit errorOccurred("start: video item not set.");
        return false;
    }

// #ifdef _WIN32
//     // 直接从 provider 拿 device [acquireDevice() 做AddRef]
//     if (!m_d3dqtDev) {
//         m_d3dqtDev.Attach(D3D11DeviceProvider::instance()->acquireDevice());
//     }
//     m_deviceReady = (m_d3dqtDev != nullptr);

//     if (!m_deviceReady)
//     {
//         // provider 还没 ready-->保持 running=true，等 deviceReady 信号再启动
//         // 一般不会进入这个分支
//         WARNING_LOG << "Oran7ScreenCaptureController start: waiting for D3D11DeviceProvider::deviceReady...";
//         return true;
//     }
// #endif

    startWorkerIfPossible();
    return true;
}

/**
 * @brief Oran7ScreenCaptureController::stop
 * 调用此stop会把抓屏预览和编码线程全都停止运行
 */
void Oran7ScreenCaptureController::stop()
{
    if (!m_running.exchange(false)) return;
    teardownWorker();
    teardownEncoder();
    emit stopped();
}

void Oran7ScreenCaptureController::setOutputIndex(int idx)
{
    m_outputIndex = idx;
}

void Oran7ScreenCaptureController::setDrawMouse(bool on)
{
    m_drawMouse = on;
}

void Oran7ScreenCaptureController::setFps(int fps)
{
    m_fps = fps;
}

void Oran7ScreenCaptureController::setDebugLog(bool on)
{
    m_debugLog.store(on, std::memory_order_release);
    // 同步到正在运行的 worker
    auto *worker = static_cast<DdaGrabWorker*>(m_workerObj);
    if (worker)
        worker->setDebugLog(on);
    // 同步到 video item
    if (m_item)
        m_item->setDebugLog(on);
}

void Oran7ScreenCaptureController::onProviderReady()
{
    ID3D11Device* newDev = D3D11DeviceProvider::instance()->acquireDevice(); // AddRef
    if (!newDev) {
        m_deviceReady = false;
        return;
    }

    //如果已经是同一个 device，就不要重复替换/重建
    if (m_d3dqtDev.Get() == newDev) {
        // newDev 是 acquireDevice AddRef 出来的，需要释放掉这一次多出来的引用
        newDev->Release();

        m_deviceReady = true;
        if (m_running.load() && !m_thread) {
            startWorkerIfPossible();
        }
        return;
    }

    //device 发生变化-->先停抓屏，再切换 device，再按需重启
    teardownWorker();

    m_d3dqtDev.Reset();
    m_d3dqtDev.Attach(newDev); // 接管 newDev

    m_deviceReady = true;
    if (m_running.load()) {
        startWorkerIfPossible();
    }
}


void Oran7ScreenCaptureController::onProviderLost()
{
    // device 丢失-->停抓屏，释放 device
    HRESULT r = m_d3dqtDev ? m_d3dqtDev->GetDeviceRemovedReason() : S_OK;
    INFO_LOG << QString("GetDeviceRemovedReason=0x%1").arg(uint32_t(r), 8, 16, QChar('0'));

    teardownWorker();
    m_d3dqtDev.Reset();
    m_deviceReady = false;
    // -->renderBlackFrame
    if (m_item) QMetaObject::invokeMethod(m_item, "renderBlackFrame", Qt::QueuedConnection);
}

void Oran7ScreenCaptureController::onSharedHandlesReady(int w, int h,const QVector<quintptr>& hs)
{
    QMutexLocker locker(&m_texMutex);
    m_texW = w; m_texH = h;
    for (int i=0;i<kPool;i++) {
        m_handle[i] = (i < hs.size()) ? hs[i] : 0;
    }

    ID3D11Device* qtDev = m_d3dqtDev.Get();
    if (!qtDev) return;

    for (int i=0;i<kPool;i++) {
        m_qtTex[i].Reset();

        HANDLE hh = reinterpret_cast<HANDLE>(m_handle[i]);
        if (!hh) continue;

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = qtDev->OpenSharedResource(hh, IID_PPV_ARGS(&tex));
        if (FAILED(hr) || !tex) {
             qWarning() << "OpenSharedResource failed idx="<<i<<" hr="<<QString::number(hr,16);
            continue;
        }
        m_qtTex[i] = tex;
    }
}

void Oran7ScreenCaptureController::onNewFrameIndex(int idx)
{
    if (!m_item) return;
    if (idx < 0 || idx >= 3) return;

    // DirectConnection → 在 worker 线程执行，加锁读共享纹理池
    ComPtr<ID3D11Texture2D> tex;
    int w = 0, h = 0;
    {
        QMutexLocker locker(&m_texMutex);
        if (!m_qtTex[idx]) return;
        tex = m_qtTex[idx];  // AddRef
        w = m_texW;
        h = m_texH;
    }

    AVFrame* frame = makeD3D11FrameFromTexture(tex.Get(), w, h);
    if (!frame) return;

    m_item->submitFrame(frame);
}

#ifdef _WIN32
void Oran7ScreenCaptureController::setD3D11Device(ID3D11Device* dev)
{
    if (!dev) return;
    m_d3dqtDev = dev;
    m_deviceReady = true;
}

bool Oran7ScreenCaptureController::startRecording()
{
    auto* worker = static_cast<DdaGrabWorker*>(m_workerObj);

    // 等 grabLoop 首帧确定实际分辨率
    while (worker->grabW() == 0 || worker->grabH() == 0) {
        QThread::msleep(10);
    }

/** -----------------------------------------------
* @brief VideoEncoder
* -------------------------------------------------
*/

    Oran7VideoEncoder::EncoderConfig cfg;
    if (m_recordMode == RecordMode::WhipPush) {
        cfg.width       = 1280;
        cfg.height      = 720;
        cfg.fps         = 30;
        cfg.bitrateKbps = 5000;   // 720p30 VBR 推流 5Mbps
        cfg.gopSize     = 30;     // 1 秒 GOP
    } else {
        cfg.width       = worker->grabW();
        cfg.height      = worker->grabH();
        cfg.fps         = 60;
        cfg.bitrateKbps = 40000;  // 录屏 40Mbps
    }
    cfg.codecName  = "h264_mf";
    cfg.hwDevice   = worker->hwDeviceRef();

    m_encoder = std::make_shared<Oran7VideoEncoder>();
    ErrorCallback onError = [this](const std::string& msg) {
        ERROR_LOG<<"Oran7ScreenCaptureController_ERROR_LOG:"<<msg;
        QMetaObject::invokeMethod(this, [this, e =QString::fromStdString(msg)]() {
            emit errorOccurred(e);
        }, Qt::QueuedConnection);
    };
    m_encoder->configure(cfg, onError);
    m_encoder->start();

    // 等编码器在编码线程内 avcodec_open2 完成（h264_mf 需要 MTA COM）
    for (int i = 0; i < 300; ++i) {  // 最多等 3 秒
        if (m_encoder->isOpened()) break;
        if (!m_encoder->isRuning()) break;  // 编码线程已退出 = 失败
        QThread::msleep(10);
    }
    if (!m_encoder->isOpened()) {
        ERROR_LOG << "Video encoder failed to open (timeout or avcodec_open2 error)";
        m_encoder->stop();
        m_encoder.reset();
        return false;
    }

/** -----------------------------------------------
* @brief AudioEncoder + AudioCapture
* -------------------------------------------------
*/
    Oran7AudioEncoder::EncoderConfig acfg;
    acfg.codecName = m_recordMode==RecordMode::WhipPush ? "opus" : "libfdk_aac";// FFmpeg 原生 opus（libopus 未编入时用这个）
    acfg.sampleRate = 48000;
    acfg.channels   = 2;
    acfg.bitrateBps = 128000;

    m_audioEncoder = std::make_shared<Oran7AudioEncoder>();
    m_audioEncoder->configure(acfg, onError);
    m_audioEncoder->start();

    for (int i = 0; i < 300; ++i) {  // 最多等 3 秒
        if (m_audioEncoder->isOpened()) break;
        if (!m_audioEncoder->isRunning()) break;
        QThread::msleep(10);
    }

    WASAPIAudioWorker::WASAPIConfig wcfg;
    wcfg.sampleRate = 48000;
    wcfg.channels   = 2;
    wcfg.frameMs    = 10;

    m_audioWorker = std::make_unique<WASAPIAudioWorker>();
    m_audioWorker->start(wcfg);

    // 音频桥接线程：WASAPI PCM → AudioEncoder
    m_audioBridgeRunning.store(true);
    m_audioBridge = std::thread([this]() {
        while (m_audioBridgeRunning.load()) {
            AVFrame* frame = nullptr;
            if (m_audioWorker->getPcmFrame(&frame, 50)) {
                m_audioEncoder->pushFrame(frame);
            }
        }
    });

/** -----------------------------------------------
* @brief 消费端：Muxer → MP4  或  WhipPublisher → 推流
* -------------------------------------------------
*/
    if (m_recordMode == RecordMode::WhipPush) {
        // ============================================================
        // WHIP/WebRTC 推流路径
        // ============================================================
        Oran7WhipPublisher::Config whipCfg;
        whipCfg.serverUrl       = "http://127.0.0.1:8090";
        whipCfg.roomId          = "demo";
        whipCfg.publisherId     = "pub2";
        whipCfg.stunUrl         = "stun:127.0.0.1:3478";
        whipCfg.videoBitrateKbps = 5000;  // 720p30 5Mbps

        // 告诉 grabLoop 把 1440p 帧下采样到 720p 再喂编码器
        worker->setStreamTargetRes(1280, 720);

        m_whipPublisher = std::make_shared<Oran7WhipPublisher>();
        m_whipPublisher->setErrorCallback(onError);

        if (!m_whipPublisher->open(whipCfg, m_encoder, m_audioEncoder)) {
            ERROR_LOG << "WhipPublisher::open() failed";
            m_whipPublisher.reset();
            m_audioBridgeRunning.store(false);
            if (m_audioBridge.joinable()) m_audioBridge.join();
            m_encoder->stop();
            m_encoder.reset();
            if (m_audioEncoder) { m_audioEncoder->stop(); m_audioEncoder.reset(); }
            m_audioWorker->stop();
            m_audioWorker.reset();
            return false;
        }

        // WHIP 成功后 → 通知信令服务器，web 端才能发现这个流
        m_signaling = std::make_unique<Oran7SignalingClient>();
        m_signaling->start("ws://127.0.0.1:8080", "user3", "demo");
        // 注：login + join_room 是异步的，stream_published 会在
        //     room_joined 回调里自动发出（或者直接发，服务器会忽略非房间内的消息）
        // 这里用 notifyStreamPublished，SignalingClient 内部处理时序
        m_signaling->notifyStreamPublished(m_whipPublisher->streamId());

    } else {
        // ============================================================
        // MP4 文件录制路径
        // ============================================================
        m_muxer = std::make_shared<Oran7Mp4Muxer>();
        m_muxer->setErrorCallback(onError);

        m_muxer->open("C:/Users/funny/Videos/test.mp4",
                      cfg.width, cfg.height,
                      AVRational{1, cfg.fps},
                      m_encoder,
                      m_audioEncoder);
    }

    /*! @note grabLoop开闸,从pushFrame分支流出 */
    worker->setEncoder(m_encoder);
    worker->m_recordingEnabled.store(true);

    emit recordingStarted();
    return true;
}

void Oran7ScreenCaptureController::stopRecording()
{
    auto* worker = static_cast<DdaGrabWorker*>(m_workerObj);
    if (worker)
        worker->m_recordingEnabled.store(false);
    // 不调 unRefEncoder——grabLoop 可能还在录制分支内，shared_ptr 保活防崩
    teardownEncoder();
    emit recordingStopped();
}

bool Oran7ScreenCaptureController::isRecording() const
{
    return m_encoder && m_encoder->isRuning();
}
#endif

void Oran7ScreenCaptureController::startWorkerIfPossible()
{
#ifdef _WIN32
    if (!m_item /*|| !m_d3dqtDev*/) {
        emit errorOccurred("startWorker: item missing.");
        return;
    }
    else
        INFO_LOG<<"Suuccessed get d3d11Dev and renderVideoItem.";
#endif

    if (m_thread) return; // already running

    this->m_thread = new QThread();
#ifdef _WIN32
    DdaGrabWorker* worker = new DdaGrabWorker(m_item, m_outputIndex, m_drawMouse, m_fps);
#else
    auto* worker = nullptr;
#endif
    //worker->bind_DdaGrab_d3dqtDev(m_d3dqtDev.Get());
    worker->setDebugLog(m_debugLog.load(std::memory_order_acquire));
    m_workerObj = worker;
    worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, worker, &DdaGrabWorker::start);
    connect(this, &Oran7ScreenCaptureController::stopped, worker, &DdaGrabWorker::stop);
    connect(worker, &DdaGrabWorker::started, this, &Oran7ScreenCaptureController::started);//只做信号发送给外层，不做启动逻辑
    connect(worker, &DdaGrabWorker::stopped,  this, [this]() {
        // worker 自然结束时也清理
        teardownWorker();
        emit stopped();
    });
    connect(worker, &DdaGrabWorker::errorOccurred, this, [this](const QString& e){
        emit errorOccurred(e);
        stop();
    });

    connect(worker, &DdaGrabWorker::sharedHandlesReady,
            this, &Oran7ScreenCaptureController::onSharedHandlesReady,
            Qt::QueuedConnection);
    connect(worker, &DdaGrabWorker::newFrameReady,
            this, &Oran7ScreenCaptureController::onNewFrameIndex,
            Qt::DirectConnection);  // 串行化：避免 GUI 线程批处理两个帧导致单槽覆盖

    connect(m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
}

void Oran7ScreenCaptureController::teardownWorker()
{
    if (!m_thread) return;

    // 先请求 worker stop
    DdaGrabWorker *worker  = static_cast<DdaGrabWorker*>(m_workerObj);
    worker->stop();

    m_thread->quit();
    m_thread->wait();

    m_thread->deleteLater();

    m_thread = nullptr;
    m_workerObj = nullptr;
}

void Oran7ScreenCaptureController::teardownEncoder()
{
    // 1. 停止音频桥接线程
    m_audioBridgeRunning.store(false);
    if (m_audioBridge.joinable())
        m_audioBridge.join();

    // 2. 停止音频采集（不再产 PCM）
    if (m_audioWorker) {
        m_audioWorker->stop();
        m_audioWorker.reset();
    }

    // 3. flush 音频编码器缓冲
    if (m_audioEncoder) {
        m_audioEncoder->stop();
    }

    // 4. flush 视频编码器缓冲
    if (m_encoder) {
        m_encoder->stop();
    }

    // 5. 关消费端（Muxer 或 WhipPublisher）+ 信令
    if (m_signaling) {
        m_signaling->stop();
        m_signaling.reset();
    }
    if (m_muxer) {
        m_muxer->close();
        m_muxer.reset();
    }
    if (m_whipPublisher) {
        m_whipPublisher->close();
        m_whipPublisher.reset();
    }

    // 6. 销毁编码器
    m_audioEncoder.reset();
    m_encoder.reset();
}

/**
 * ======================================================
 * @note Windows桌面画面DDa抓取器 - DdaGrabWorker
 * ======================================================
 */

void DdaGrabWorker::start()
{
    if (m_running.exchange(true)) return;

    const QString err = initGraph();
    INFO_LOG<<err;
    if (!err.isEmpty()) {
        m_running.store(false);
        emit errorOccurred(err);
        return;
    }
    else
        INFO_LOG<<"Successed ScreenCapture initGraph.";

    emit started();
    grabLoop();
    cleanup();
    emit stopped();
}

// 枚举所有 DXGI adapter，返回第一个输出 attached 到桌面的 adapter 索引。
// 笔记本双显卡场景：默认 adapter 可能是独显，而显示器接在核显上，
// 用独显设备做 DDA 会返回 DXGI_ERROR_UNSUPPORTED；必须选显示器实际所在的 GPU。
int DdaGrabWorker::findDesktopAdapterIndex()
{
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return -1;

    for (UINT i = 0; i < 8; ++i) {
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
        for (UINT o = 0; o < 16; ++o) {
            ComPtr<IDXGIOutput> out;
            if (FAILED(adapter->EnumOutputs(o, &out)))
                break;
            DXGI_OUTPUT_DESC odesc{};
            if (SUCCEEDED(out->GetDesc(&odesc)) && odesc.AttachedToDesktop) {
                hasDesktop = true;
                break;
            }
        }

        if (hasDesktop) {
            INFO_LOG << "[CaptureDiag] selected adapter[" << i << "] for DDA: "
                     << QString::fromWCharArray(desc.Description);
            return (int)i;
        }
    }
    return -1; // 未找到，回退到默认
}

QString DdaGrabWorker::initGraph()
{
    int ret  = 0;
    if (!m_item) return "D3D11VideoItem is null.";

    // 诊断：枚举 GPU/显示器（不改变设备创建方式）。
    // 注意：显式传 adapter 索引给 av_hwdevice_ctx_create 会用 D3D_DRIVER_TYPE_UNKNOWN
    // 创建设备，导致 DuplicateOutput1 返回 E_INVALIDARG；传 nullptr 让 FFmpeg 自动选
    // 主 GPU（D3D_DRIVER_TYPE_HARDWARE），行为正确。故这里仍传 nullptr。
    const int adapterIdx = findDesktopAdapterIndex();
    INFO_LOG << "[CaptureDiag] findDesktopAdapterIndex=" << adapterIdx
             << " (仅诊断，不影响设备创建)";

    ret = av_hwdevice_ctx_create(&m_hwdev, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
    if (ret < 0) return "av_hwdevice_ctx_create(D3D11VA) failed: " + ffErrStr(ret);

    //m_hwdev = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!m_hwdev) return "av_hwdevice_ctx_alloc failed.";

    // 从 hwdev 取出 capture device 指针（后续创建共享纹理、CopyResource 都用它）
    AVHWDeviceContext* hw = reinterpret_cast<AVHWDeviceContext*>(m_hwdev->data);
    AVD3D11VADeviceContext* d3d11 = reinterpret_cast<AVD3D11VADeviceContext*>(hw->hwctx);

    // ---- 诊断：打印抓屏用 D3D11 设备对应的 GPU 与可用显示器输出 ----
    {
        IDXGIDevice* dxgiDev = nullptr;
        if (d3d11->device && SUCCEEDED(d3d11->device->QueryInterface(IID_IDXGIDevice, (void**)&dxgiDev))) {
            IDXGIAdapter* adapter = nullptr;
            if (SUCCEEDED(dxgiDev->GetAdapter(&adapter))) {
                DXGI_ADAPTER_DESC adesc{};
                if (SUCCEEDED(adapter->GetDesc(&adesc)))
                    INFO_LOG << "[CaptureDiag] D3D11 capture device GPU: "
                             << QString::fromWCharArray(adesc.Description)
                             << " (Vendor=0x" << QString::number(adesc.VendorId, 16)
                             << " Device=0x" << QString::number(adesc.DeviceId, 16) << ")";
                for (UINT i = 0; i < 16; ++i) {
                    IDXGIOutput* out = nullptr;
                    if (FAILED(adapter->EnumOutputs(i, &out))) break;
                    DXGI_OUTPUT_DESC odesc{};
                    if (SUCCEEDED(out->GetDesc(&odesc)))
                        INFO_LOG << "[CaptureDiag]   output[" << i << "] attached="
                                 << (odesc.AttachedToDesktop ? "true" : "false")
                                 << " device=" << QString::fromWCharArray(odesc.DeviceName);
                    out->Release();
                }
                adapter->Release();
            }
            dxgiDev->Release();
        } else {
            WARNING_LOG << "[CaptureDiag] Failed to query IDXGIDevice from capture hw ctx";
        }
    }

    m_capDev = d3d11->device;
    if (!m_capDev) return "capture d3d11 device is null";
    //d3d11->device = m_capDev.Get();
    d3d11->device->AddRef();

    // ret  = av_hwdevice_ctx_init(m_hwdev);
    // if(ret<0) return "av_hwdevice_ctx_init Failed.";

    m_capDev->GetImmediateContext(&m_capCtx);
    m_capDev->QueryInterface(IID_ID3D11VideoDevice, (void**)&m_capVideoDev);
    m_capCtx->QueryInterface(IID_ID3D11VideoContext, (void**)&m_capVideoCtx);

    // 2) Build filter graph: ddagrab -> buffersink（去掉 fps filter，源头直接设置帧率）
    m_graph = avfilter_graph_alloc();
    if (!m_graph) return "avfilter_graph_alloc failed.";

    const AVFilter* f_src  = avfilter_get_by_name("ddagrab");
    const AVFilter* f_sink = avfilter_get_by_name("buffersink");
    if (!f_src || !f_sink) return "ddagrab/buffersink filter not found (check FFmpeg build).";

    // ddagrab options: 显式指定 framerate，不依赖 fps filter 补帧
    // output_fmt=bgra：DDA 输出 8bit BGRA（B8G8R8A8），与后续 BGRA→NV12 处理一致。
    // allow_fallback=1：请求格式不可用时降级到默认 8bit，避免直接配置失败。
    char srcArgs[256];
    snprintf(srcArgs, sizeof(srcArgs),
             "output_idx=%d:draw_mouse=%d:framerate=%d:output_fmt=bgra:allow_fallback=1",
             m_outputIndex, m_drawMouse ? 1 : 0,
             m_fps > 0 ? m_fps : 60);
    INFO_LOG << "[CaptureDiag] ddagrab args: " << srcArgs;

    ret = avfilter_graph_create_filter(&m_src, f_src, "src", srcArgs, nullptr, m_graph);
    if (ret < 0) return "create ddagrab failed: " + ffErrStr(ret);
    INFO_LOG << "[CaptureDiag] ddagrab filter created, output_idx=" << m_outputIndex;

    //m_src绑定 hwdevice
    m_src->hw_device_ctx = av_buffer_ref(m_hwdev);

    ret = avfilter_graph_create_filter(&m_sink, f_sink, "sink", nullptr, nullptr, m_graph);
    if (ret < 0) return "create buffersink failed: " + ffErrStr(ret);
    //m_sink 绑定 hwdevice
    m_sink->hw_device_ctx = av_buffer_ref(m_hwdev);

    // 直接 src -> sink，不用 fps filter 补帧
    ret = avfilter_link(m_src, 0, m_sink, 0);
    if (ret < 0) return "link src->sink failed: " + ffErrStr(ret);

    // 诊断：配置前打开 FFmpeg 详细日志，便于捕获 ddagrab 内部失败原因
    {
        const int oldLevel = av_log_get_level();
        av_log_set_level(AV_LOG_DEBUG);
        INFO_LOG << "[CaptureDiag] --- avfilter_graph_config start (output_idx="
                 << m_outputIndex << ") ---";
        ret = avfilter_graph_config(m_graph, nullptr);
        av_log_set_level(oldLevel);
        if (ret < 0) {
            WARNING_LOG << "[CaptureDiag] --- avfilter_graph_config FAILED ret="
                        << ret << " ---";
            return "avfilter_graph_config failed: " + ffErrStr(ret);
        }
        INFO_LOG << "[CaptureDiag] --- avfilter_graph_config OK ---";
    }

    return {};
}

bool DdaGrabWorker::ensureSharedPoolRGBA(int w, int h)
{
    if (m_poolW == w && m_poolH == h && m_sharedTex[0]) return true;

    // 释放旧的
    for (int i=0;i<kPool;i++) {
        m_sharedHandle[i] = nullptr;
        m_sharedTex[i].Reset();
    }

    m_poolW = w; m_poolH = h;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;//<--
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    for (int i=0;i<kPool;i++) {
        HRESULT hr = m_capDev->CreateTexture2D(&desc, nullptr, &m_sharedTex[i]);
        if (FAILED(hr) || !m_sharedTex[i]) return false;

        ComPtr<IDXGIResource> res;
        hr = m_sharedTex[i].As(&res);
        if (FAILED(hr) || !res) return false;

        HANDLE hShared = nullptr;
        hr = res->GetSharedHandle(&hShared);
        if (FAILED(hr) || !hShared) return false;

        m_sharedHandle[i] = hShared;
    }

    QVector<quintptr> hs;
    for (int i=0;i<kPool;i++) hs.push_back((quintptr)m_sharedHandle[i]);
    hs.reserve(kPool);
    emit sharedHandlesReady(m_poolW, m_poolH, hs);

    return true;
}

void DdaGrabWorker::grabLoop()
{
    int idx = 0;
    // pts 检测：确认源头是否真 60fps
    static int64_t lastPts = AV_NOPTS_VALUE;
    static int rawCount = 0;
    static qint64 rawT0 = QDateTime::currentMSecsSinceEpoch();

    while (m_running.load()) {
        //主动请求上游产一帧（驱动 ddagrab）
        int r = avfilter_graph_request_oldest(m_graph);
        //INFO_LOG << "After avfilter_graph_request_oldest, ret=" << r;
        //INFO_LOG<<"avfilter_graph_request_oldest of ret:"<<r;
        if (r < 0 && r != AVERROR(EAGAIN))
        {
            if (m_running.load())
                emit errorOccurred("request_oldest failed: " + ffErrStr(r));
            return;
        }

        // 从 sink 非阻塞取出“已经到达”的帧（可能一次取多帧）
        AVFrame* f = av_frame_alloc();
        if (!f) {
            emit errorOccurred("av_frame_alloc failed");
            return;
        }
        while (m_running.load())
        {
            av_frame_unref(f);//unref old frame
            if (!f) { emit errorOccurred("av_frame_alloc failed"); return; }

            int ret = av_buffersink_get_frame_flags(m_sink, f, AV_BUFFERSINK_FLAG_NO_REQUEST);
            //INFO_LOG<<"RUN av_buffersink_get_frame_flags of RET:"<<ret;
            if (ret == AVERROR(EAGAIN)) {
                av_frame_free(&f);
                //INFO_LOG<<"av_buffersink_get_frame_flags(m_sink, f, AV_BUFFERSINK_FLAG_NO_REQUEST) of RET:EAGAIN.";
                break; // 本轮没有帧可取，回到上面继续 request
            }
            if (ret < 0)
            {
                av_frame_free(&f);
                if (m_running.load())
                    emit errorOccurred("get_frame failed: " + ffErrStr(ret));
                return;
            }

            // === Copy/VP 到 sharedTex===
            auto* srcTex = reinterpret_cast<ID3D11Texture2D*>(f->data[0]);
            if (srcTex)
            {
                //------- pts 检测：确认源头真帧率 -------//
                if (m_debugLog.load(std::memory_order_relaxed) && false) {
                    rawCount++;
                    if (f->pts == lastPts && lastPts != AV_NOPTS_VALUE) {
                        INFO_LOG << "DUP frame pts=" << f->pts << " rawCount=" << rawCount;
                    }
                    lastPts = f->pts;

                    auto rawNow = QDateTime::currentMSecsSinceEpoch();
                    if (rawNow - rawT0 >= 1000) {
                        INFO_LOG << "sink raw fps=" << (rawCount * 1000 / (rawNow - rawT0))
                                 << " pts=" << f->pts << " rawCount=" << rawCount <<",persecond.";
                        rawCount = 0;
                        rawT0 = rawNow;
                    }
                }
                //------------------------------------------//

                D3D11_TEXTURE2D_DESC sdesc{};
                srcTex->GetDesc(&sdesc);
                int w = (int)sdesc.Width;
                int h = (int)sdesc.Height;

                if (!ensureSharedPoolRGBA(w, h)) {
                    av_frame_free(&f);
                    WARNING_LOG<<"ensureSharedPoolRGBA failed";
                    emit errorOccurred("ensureSharedPoolRGBA failed");
                    return;
                }

                // 首帧/尺寸变化时同步可见尺寸（f->width/height），非 D3D 分配尺寸
                int visW = f->width;
                int visH = f->height;
                if (m_grabW.load(std::memory_order_relaxed) != visW ||
                    m_grabH.load(std::memory_order_relaxed) != visH) {
                    m_grabW.store(visW, std::memory_order_release);
                    m_grabH.store(visH, std::memory_order_release);
                }

                idx = (idx + 1) % kPool;
                int slice = 0;
                if (sdesc.Format == DXGI_FORMAT_NV12 || sdesc.Format == DXGI_FORMAT_P010)
                    slice = (int)(intptr_t)f->data[1];

                if (!blitToShared(idx, srcTex, sdesc, slice)) {
                    av_frame_free(&f);
                    WARNING_LOG<<"blitToShared failed";
                    emit errorOccurred("blitToShared failed");
                    return;
                }
                //INFO_LOG<<"newFrameReady -idx:"<<idx;
                emit newFrameReady(idx);

                // ---- 录制/推流路径 ----
                bool recEnabled = m_recordingEnabled.load(std::memory_order_acquire);
                if (recEnabled) {
                    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    if (!m_lastRecEnabled) {
                        m_recStartUs = nowUs;
                        m_lastPts = AV_NOPTS_VALUE;
                        m_lastRecEnabled = true;
                    }
                    int visW = f->width;
                    int visH = f->height;
                    int targetW = m_streamW.load(std::memory_order_acquire);
                    int targetH = m_streamH.load(std::memory_order_acquire);
                    int encW = (targetW > 0) ? targetW : visW;
                    int encH = (targetH > 0) ? targetH : visH;

                    // 用 av_hwframe_transfer_data 下载 D3D11 帧（正确处理 data[1] 子资源）
                    // 一趟 BGRA → NV12 到编码器分辨率
                    AVFrame* encFrame = downloadD3d11AndConvert(f, encW, encH);
                    if (encFrame && m_encoder && m_encoder->isRuning()) {
                        int64_t pts = (nowUs - m_recStartUs) * 90'000 / 1'000'000;
                        if (pts < 0) pts = 0;
                        if (m_lastPts != AV_NOPTS_VALUE && pts <= m_lastPts)
                            pts = m_lastPts + 1;
                        m_lastPts = pts;
                        encFrame->pts = pts;
                        if (pts == 1)
                            encFrame->pict_type = AV_PICTURE_TYPE_I;

                        // ---- 诊断：保存前 120 帧 NV12 到文件 ----
                        {
                            static FILE* fpNv12 = nullptr;
                            static int nv12Cnt = 0;
                            if (!fpNv12) {
                                fpNv12 = fopen("C:/temp/pre_encode_1920x1080.nv12","wb");
                            }
                            if (fpNv12 && nv12Cnt < 120) {
                                for (int y = 0; y < encFrame->height; ++y)
                                    fwrite(encFrame->data[0] + y * encFrame->linesize[0],
                                           1, encFrame->width, fpNv12);
                                for (int y = 0; y < encFrame->height / 2; ++y)
                                    fwrite(encFrame->data[1] + y * encFrame->linesize[1],
                                           1, encFrame->width, fpNv12);
                                nv12Cnt++;
                                if (nv12Cnt == 120) {
                                    fclose(fpNv12); fpNv12 = nullptr;
                                    INFO_LOG << "NV12 dump done: C:/temp/pre_encode_1920x1080.nv12";
                                }
                            }
                        }
                        // ----

                        m_encoder->pushFrame(encFrame);
                    } else {
                        av_frame_free(&encFrame);
                    }
                }
            }
        }
        av_frame_free(&f);
    }
    INFO_LOG<<"Oran7ScreenCapture grabLopp thread leave.";
}

AVFrame* DdaGrabWorker::downloadD3d11AndConvert(const AVFrame* hwFrame,
                                                 int dstW, int dstH)
{
    // hwFrame 来自 ddagrab → D3D11 硬件帧
    // av_hwframe_transfer_data 正确处理 data[1] 子资源索引
    if (!hwFrame || hwFrame->format != AV_PIX_FMT_D3D11) {
        WARNING_LOG << "downloadD3d11: invalid hwFrame";
        return nullptr;
    }

    AVFrame* bgra = av_frame_alloc();
    if (!bgra) return nullptr;

    int ret = av_hwframe_transfer_data(bgra, hwFrame, 0);
    if (ret < 0) {
        WARNING_LOG << "av_hwframe_transfer_data failed:" << ret;
        av_frame_free(&bgra);
        return nullptr;
    }
    av_frame_copy_props(bgra, hwFrame);

    // 校验下载后的软件像素格式；ddagrab output_fmt=bgra + allow_fallback
    // 正常情况下是 BGRA，个别 GPU/驱动可能降级为其他 8bit 格式，这里按实际格式转换
    const AVPixelFormat srcFmt = AVPixelFormat(bgra->format);
    if (srcFmt != AV_PIX_FMT_BGRA) {
        const char* name = av_get_pix_fmt_name(srcFmt);
        WARNING_LOG << "downloadD3d11: downloaded format:" << (name ? name : "?")
                    << " (expected BGRA), using it directly";
    }

    int srcW = bgra->width;
    int srcH = bgra->height;
    int outW = (dstW > 0) ? dstW : srcW;
    int outH = (dstH > 0) ? dstH : srcH;

    static SwsContext* sws = nullptr;
    static int sW, sH, dW, dH;
    static AVPixelFormat sFmt = AV_PIX_FMT_NONE;
    if (!sws || sW!=srcW || sH!=srcH || dW!=outW || dH!=outH || sFmt != srcFmt) {
        if (sws) sws_freeContext(sws);
        sws = sws_getContext(srcW, srcH, srcFmt,
                             outW, outH, AV_PIX_FMT_NV12,
                             SWS_BICUBIC, nullptr, nullptr, nullptr);
        sW=srcW; sH=srcH; dW=outW; dH=outH;
        sFmt = srcFmt;
    }
    if (!sws) { av_frame_free(&bgra); return nullptr; }

    AVFrame* dst = av_frame_alloc();
    dst->format = AV_PIX_FMT_NV12;
    dst->width  = outW;
    dst->height = outH;
    if (av_frame_get_buffer(dst, 32) < 0) {
        av_frame_free(&dst); av_frame_free(&bgra); return nullptr;
    }
    int rows = sws_scale(sws, bgra->data, bgra->linesize, 0, srcH,
                          dst->data, dst->linesize);
    av_frame_free(&bgra);
    if (rows != outH) {
        WARNING_LOG << "BGRA→NV12 incomplete:" << rows << "/" << outH;
        av_frame_free(&dst);
        return nullptr;
    }
    dst->pts = hwFrame->pts;
    return dst;
}

void DdaGrabWorker::cleanup()
{
    m_vp.Reset();
    m_vpEnum.Reset();
    m_capVideoCtx.Reset();
    m_capVideoDev.Reset();
    m_capCtx.Reset();
    m_capDev.Reset();

    if (m_graph) avfilter_graph_free(&m_graph);
    m_graph = nullptr;
    m_src = nullptr;
    m_sink = nullptr;

    if (m_hwdev) av_buffer_unref(&m_hwdev);
    m_hwdev = nullptr;
}

/*! @note Tools function*/

bool DdaGrabWorker::ensureVideoProcessor(int w, int h)
{
    if (!m_capVideoDev || !m_capVideoCtx) return false;

    if (m_vp && m_vpEnum && m_vpSize == QSize(w, h))
        return true;

    m_vp.Reset();
    m_vpEnum.Reset();
    m_vpSize = QSize(w, h);

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC c{};
    c.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    c.InputWidth  = (UINT)w;
    c.InputHeight = (UINT)h;
    c.OutputWidth  = (UINT)w;
    c.OutputHeight = (UINT)h;
    c.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr = m_capVideoDev->CreateVideoProcessorEnumerator(&c, &m_vpEnum);
    if (FAILED(hr) || !m_vpEnum) return false;

    hr = m_capVideoDev->CreateVideoProcessor(m_vpEnum.Get(), 0, &m_vp);
    return SUCCEEDED(hr) && m_vp;
}

bool DdaGrabWorker::vpNv12ToRgbaShared(int idx,
                                       ID3D11Texture2D* srcTex,
                                       int w, int h,
                                       int slice)
{
    if (!srcTex) return false;
    if (idx < 0 || idx >= kPool) return false;
    if (!m_sharedTex[idx]) return false;

    if (!ensureVideoProcessor(w, h)) return false;
    if (!m_vpEnum || !m_vp) return false;

    // 1) Input view: srcTex (NV12/P010), may be array texture; slice indicates which slice
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
    inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inDesc.Texture2D.ArraySlice = (UINT)slice;

    ComPtr<ID3D11VideoProcessorInputView> inView;
    HRESULT hr = m_capVideoDev->CreateVideoProcessorInputView(
        srcTex, m_vpEnum.Get(), &inDesc, &inView);
    if (FAILED(hr) || !inView) return false;

    // 2) Output view: sharedTex[idx] (RGBA8)
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{};
    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outDesc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11VideoProcessorOutputView> outView;
    hr = m_capVideoDev->CreateVideoProcessorOutputView(
        m_sharedTex[idx].Get(), m_vpEnum.Get(), &outDesc, &outView);
    if (FAILED(hr) || !outView) return false;

    RECT srcRect{0, 0, w, h};
    RECT dstRect{0, 0, w, h};

    m_capVideoCtx->VideoProcessorSetStreamSourceRect(m_vp.Get(), 0, TRUE, &srcRect);
    m_capVideoCtx->VideoProcessorSetStreamDestRect(m_vp.Get(), 0, TRUE, &dstRect);

    // 可选：色彩空间/范围设置（先不碰，默认通常可用）
    // m_capVideoCtx->VideoProcessorSetOutputColorSpace1(...)

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inView.Get();

    hr = m_capVideoCtx->VideoProcessorBlt(m_vp.Get(), outView.Get(), 0, 1, &stream);
    return SUCCEEDED(hr);
}

bool DdaGrabWorker::blitToShared(int idx,
                                 ID3D11Texture2D* srcTex,
                                 const D3D11_TEXTURE2D_DESC& sdesc,
                                 int slice)
{
    if (!srcTex){
        WARNING_LOG<<"blitToShared srcTex is nullptr.";
        return false;
    }
    if (idx < 0 || idx >= kPool) {
        WARNING_LOG<<"blitToShared idx is ERROR,idx:"<<idx;
        return false;
    }
    if (!m_capCtx) {
        WARNING_LOG<<"blitToShared m_capCtx is nullptr.";
        return false;
    }
    if (!m_sharedTex[idx]) {
        WARNING_LOG<<"blitToShared m_sharedTex[idx] is nullptr, idx:"<<idx;
        return false;
    }

    //BGRA -> RGBA
    // if (sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
    //     sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
    // {
    //     return blitBgraToRgbaShared(idx, srcTex, (int)sdesc.Width, (int)sdesc.Height);
    // }

    //BGRA -> BGRA  --- NOW
    if (sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
        sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
    {
        m_capCtx->CopyResource(m_sharedTex[idx].Get(), srcTex);
        return true;
    }

    //BGRA -> NV12
    if (sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
        sdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
    {
        return vpBgraToNv12Shared(idx,srcTex,(int)sdesc.Width,(int)sdesc.Height,slice);
    }

    //RGBA -> RGBA: 直接拷贝
    if (sdesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
        sdesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
    {
        m_capCtx->CopyResource(m_sharedTex[idx].Get(), srcTex);
        return true;
    }

    //NV12/P010 -> RGBA: 用 VideoProcessor
    if (sdesc.Format == DXGI_FORMAT_NV12 || sdesc.Format == DXGI_FORMAT_P010)
    {
        return vpNv12ToRgbaShared(idx, srcTex, (int)sdesc.Width, (int)sdesc.Height, slice);
    }

    WARNING_LOG<<"blitToShared Other formt not support:"<<dxgiFormatName(sdesc.Format);
    return false;
}


bool DdaGrabWorker::ensureTmpBgra(int w, int h)
{
    if (m_tmpBgra && m_tmpBgraSize == QSize(w,h)) return true;

    m_tmpBgra.Reset();
    m_tmpBgraSize = QSize(w,h);

    D3D11_TEXTURE2D_DESC d{};
    d.Width = w;
    d.Height = h;
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;//SRV
    HRESULT hr = m_capDev->CreateTexture2D(&d, nullptr, &m_tmpBgra);
    return SUCCEEDED(hr) && m_tmpBgra;
}

bool DdaGrabWorker::ensureSwizzlePipeline(QString* err)
{
    if (m_vs && m_psBgraToRgba && m_sampler && m_rs && m_dss && m_blendOff)
        return true;

    if (!m_capDev) { if (err) *err = "capture device is null"; return false; }

    QByteArray vsSrc, psSrc;
    QString e;

    if (!readTextResource(":/shaders/bgra2rgba_vs.hlsl", vsSrc, &e)) {
        if (err) *err = e; return false;
    }
    if (!readTextResource(":/shaders/bgra2rgba_ps.hlsl", psSrc, &e)) {
        if (err) *err = e; return false;
    }

    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (!compileHlslFromMemory(vsSrc, "main", "vs_5_0", vsBlob.GetAddressOf(), &e)) {
        if (err) *err = "Compile VS failed: " + e; return false;
    }
    if (!compileHlslFromMemory(psSrc, "main", "ps_5_0", psBlob.GetAddressOf(), &e)) {
        if (err) *err = "Compile PS failed: " + e; return false;
    }

    HRESULT hr = m_capDev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
    if (FAILED(hr)) { if (err) *err = "CreateVertexShader failed"; return false; }

    hr = m_capDev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_psBgraToRgba);
    if (FAILED(hr)) { if (err) *err = "CreatePixelShader failed"; return false; }

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_capDev->CreateSamplerState(&sd, &m_sampler);
    if (FAILED(hr)) { if (err) *err = "CreateSamplerState failed"; return false; }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    hr = m_capDev->CreateRasterizerState(&rd, &m_rs);
    if (FAILED(hr)) { if (err) *err = "CreateRasterizerState failed"; return false; }

    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = FALSE;
    dd.StencilEnable = FALSE;
    hr = m_capDev->CreateDepthStencilState(&dd, &m_dss);
    if (FAILED(hr)) { if (err) *err = "CreateDepthStencilState failed"; return false; }

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = FALSE;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = m_capDev->CreateBlendState(&bd, &m_blendOff);
    if (FAILED(hr)) { if (err) *err = "CreateBlendState failed"; return false; }

    return true;
}

bool DdaGrabWorker::blitBgraToRgbaShared(int idx, ID3D11Texture2D* srcTex, int w, int h)
{
    if (!ensureTmpBgra(w,h)) return false;

    QString err;
    if (!ensureSwizzlePipeline(&err)) {
        WARNING_LOG << "ensureSwizzlePipeline failed: " << err;
        return false;
    }

    // 1) copy src -> tmpBgra（保证可 SRV）
    m_capCtx->CopyResource(m_tmpBgra.Get(), srcTex);

    // 2) SRV for tmpBgra
    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = m_capDev->CreateShaderResourceView(m_tmpBgra.Get(), nullptr, &srv);
    if (FAILED(hr) || !srv) {
        WARNING_LOG << "CreateShaderResourceView failed hr=" << QString::number(hr,16);
        return false;
    }

    // 3) RTV for RGBA sharedTex[idx]
    ComPtr<ID3D11RenderTargetView> rtv;
    hr = m_capDev->CreateRenderTargetView(m_sharedTex[idx].Get(), nullptr, &rtv);
    if (FAILED(hr) || !rtv) {
        WARNING_LOG << "CreateRenderTargetView failed hr=" << QString::number(hr,16);
        return false;
    }

    // 4) set states & draw
    ID3D11RenderTargetView* rtvs[] = { rtv.Get() };
    m_capCtx->OMSetRenderTargets(1, rtvs, nullptr);
    m_capCtx->OMSetDepthStencilState(m_dss.Get(), 0);
    float bf[4] = {};
    m_capCtx->OMSetBlendState(m_blendOff.Get(), bf, 0xffffffff);
    m_capCtx->RSSetState(m_rs.Get());

    D3D11_VIEWPORT vp{};
    vp.Width = (float)w;
    vp.Height = (float)h;
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    m_capCtx->RSSetViewports(1, &vp);

    m_capCtx->VSSetShader(m_vs.Get(), nullptr, 0);
    m_capCtx->PSSetShader(m_psBgraToRgba.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[] = { srv.Get() };
    m_capCtx->PSSetShaderResources(0, 1, srvs);
    ID3D11SamplerState* samps[] = { m_sampler.Get() };
    m_capCtx->PSSetSamplers(0, 1, samps);

    m_capCtx->IASetInputLayout(nullptr);
    m_capCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_capCtx->Draw(3, 0);

    // 解绑 SRV，避免 D3D11 警告/冲突
    ID3D11ShaderResourceView* nullSrv[] = { nullptr };
    m_capCtx->PSSetShaderResources(0, 1, nullSrv);

    return true;
}

bool DdaGrabWorker::vpBgraToNv12Shared(int idx, ID3D11Texture2D* srcTex,int w, int h, int slice,ID3D11Texture2D* dstTex)
{
    if (!srcTex) return false;
    if (!m_capVideoDev || !m_capVideoCtx) return false;

    ID3D11Texture2D* target = dstTex ? dstTex : m_sharedTex[idx].Get();
    if (!target) return false;

    if (!dstTex) {
        if (idx < 0 || idx >= kPool) return false;
    }

    if (!ensureVideoProcessor(w, h)) return false;
    if (!m_vp || !m_vpEnum) return false;

    // input view
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
    inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inDesc.Texture2D.ArraySlice = (UINT)slice;

    ComPtr<ID3D11VideoProcessorInputView> inView;
    HRESULT hr = m_capVideoDev->CreateVideoProcessorInputView(
        srcTex, m_vpEnum.Get(), &inDesc, &inView);
    if (FAILED(hr) || !inView) return false;

    // output view → target
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{};
    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outDesc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11VideoProcessorOutputView> outView;
    hr = m_capVideoDev->CreateVideoProcessorOutputView(
        target, m_vpEnum.Get(), &outDesc, &outView);
    if (FAILED(hr) || !outView) return false;

    RECT srcRect{0, 0, w, h};
    RECT dstRect{0, 0, w, h};

    m_capVideoCtx->VideoProcessorSetStreamSourceRect(m_vp.Get(), 0, TRUE, &srcRect);
    m_capVideoCtx->VideoProcessorSetStreamDestRect(m_vp.Get(), 0, TRUE, &dstRect);

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inView.Get();

    hr = m_capVideoCtx->VideoProcessorBlt(m_vp.Get(), outView.Get(), 0, 1, &stream);
    return SUCCEEDED(hr);
}

bool DdaGrabWorker::ensureEncodeNv12Tex(int w, int h)
{
    if (m_encodeNv12Tex) {
        D3D11_TEXTURE2D_DESC d;
        m_encodeNv12Tex->GetDesc(&d);
        if (int(d.Width) == w && int(d.Height) == h)
            return true;
        m_encodeNv12Tex.Reset();
    }
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w; desc.Height = h;
    desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    return SUCCEEDED(m_capDev->CreateTexture2D(&desc, nullptr,&m_encodeNv12Tex));
}

/**
 * ======================================================
 * @note Windows桌面音频捕获器 - WASAPIAudioWorker
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

bool WASAPIAudioWorker::start(const WASAPIConfig &cfg)
{
    if(_running.exchange(true)) return true;
    _cfg = cfg;
    _thread = std::thread(&WASAPIAudioWorker::captureLoop,this);
    return true;
}

void WASAPIAudioWorker::stop()
{
    if(!_running.exchange(false)) return;
    _frameCond.notify_all();
    if(_thread.joinable())
        _thread.join();

    //清空帧队列
    while(!_frameQueue.empty()){
        av_frame_free(&_frameQueue.front());
        _frameQueue.pop();
    }
}

bool WASAPIAudioWorker::isRunning() const
{
    return _running.load();
}

bool WASAPIAudioWorker::getPcmFrame(AVFrame **frame, int timeoutMs)
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

void WASAPIAudioWorker::captureLoop()
{
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);  // WASAPI 要 MTA
    if(!initWASAPI())
    {
        CoUninitialize();
        return;
    }
    _audioClient->Start();

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

bool WASAPIAudioWorker::initWASAPI()
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
        ERROR_LOG << "WASAPIAudioWorker::initWASAPI() : " << e.what();

        // 释放已创建的部分资源（cleanupWASAPI 不碰本地变量）
        cleanupWASAPI();
        if (layoutOutInited) av_channel_layout_uninit(&outChLayout);
        if (layoutInited)       av_channel_layout_uninit(&inChLayout);
        if (enumerator)       enumerator->Release();
        if (device)                device->Release();
        return false;
    }
}

void WASAPIAudioWorker::cleanupWASAPI()
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
