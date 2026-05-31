#include "Oran7ScreenCapture.h"
#include "D3d11DeviceProvider.h"


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

#ifdef _WIN32
    // 直接从 provider 拿 device [acquireDevice() 做AddRef]
    if (!m_d3dqtDev) {
        m_d3dqtDev.Attach(D3D11DeviceProvider::instance()->acquireDevice());
    }
    m_deviceReady = (m_d3dqtDev != nullptr);

    if (!m_deviceReady)
    {
        // provider 还没 ready-->保持 running=true，等 deviceReady 信号再启动
        // 一般不会进入这个分支
        WARNING_LOG << "Oran7ScreenCaptureController start: waiting for D3D11DeviceProvider::deviceReady...";
        return true;
    }
#endif

    startWorkerIfPossible();
    return true;
}

void Oran7ScreenCaptureController::stop()
{
    if (!m_running.exchange(false)) return;
    teardownWorker();
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
#endif

void Oran7ScreenCaptureController::startWorkerIfPossible()
{
#ifdef _WIN32
    if (!m_item || !m_d3dqtDev) {
        emit errorOccurred("startWorker: item or d3d device missing.");
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
    worker->bind_DdaGrab_d3dqtDev(m_d3dqtDev.Get());
    worker->setDebugLog(m_debugLog.load(std::memory_order_acquire));
    m_workerObj = worker;
    worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, worker, &DdaGrabWorker::start);
    connect(this, &Oran7ScreenCaptureController::stopped, worker, &DdaGrabWorker::stop);

    connect(worker, &DdaGrabWorker::started, this, &Oran7ScreenCaptureController::started);
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

QString DdaGrabWorker::initGraph()
{
    int ret  = 0;
    if (!m_item) return "D3D11VideoItem is null.";

    ret = av_hwdevice_ctx_create(&m_hwdev, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
    if (ret < 0) return "av_hwdevice_ctx_create(D3D11VA) failed: " + ffErrStr(ret);

    //m_hwdev = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!m_hwdev) return "av_hwdevice_ctx_alloc failed.";

    // 从 hwdev 取出 capture device 指针（后续创建共享纹理、CopyResource 都用它）
    AVHWDeviceContext* hw = reinterpret_cast<AVHWDeviceContext*>(m_hwdev->data);
    AVD3D11VADeviceContext* d3d11 = reinterpret_cast<AVD3D11VADeviceContext*>(hw->hwctx);

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
    char srcArgs[256];
    snprintf(srcArgs, sizeof(srcArgs),
             "output_idx=%d:draw_mouse=%d:framerate=%d",
             m_outputIndex, m_drawMouse ? 1 : 0,
             m_fps > 0 ? m_fps : 60);

    ret = avfilter_graph_create_filter(&m_src, f_src, "src", srcArgs, nullptr, m_graph);
    if (ret < 0) return "create ddagrab failed: " + ffErrStr(ret);

    //m_src绑定 hwdevice
    m_src->hw_device_ctx = av_buffer_ref(m_hwdev);

    ret = avfilter_graph_create_filter(&m_sink, f_sink, "sink", nullptr, nullptr, m_graph);
    if (ret < 0) return "create buffersink failed: " + ffErrStr(ret);
    //m_sink 绑定 hwdevice
    m_sink->hw_device_ctx = av_buffer_ref(m_hwdev);

    // 直接 src -> sink，不用 fps filter 补帧
    ret = avfilter_link(m_src, 0, m_sink, 0);
    if (ret < 0) return "link src->sink failed: " + ffErrStr(ret);

    ret = avfilter_graph_config(m_graph, nullptr);
    if (ret < 0) return "avfilter_graph_config failed: " + ffErrStr(ret);

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
                if (m_debugLog.load(std::memory_order_relaxed)) {
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
            }
        }
        av_frame_free(&f);
    }
    INFO_LOG<<"Oran7ScreenCapture grabLopp thread leave.";
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

bool DdaGrabWorker::vpBgraToNv12Shared(int idx,ID3D11Texture2D* srcTex,
                                      int w, int h,int slice)
{
    if (!srcTex) return false;
    if (idx < 0 || idx >= kPool) return false;
    if (!m_sharedTex[idx]) return false;
    if (!m_capVideoDev || !m_capVideoCtx) return false;

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

    // output view (NV12)
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

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inView.Get();

    hr = m_capVideoCtx->VideoProcessorBlt(m_vp.Get(), outView.Get(), 0, 1, &stream);
    return SUCCEEDED(hr);
}

