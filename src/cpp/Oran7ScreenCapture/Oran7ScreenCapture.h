#ifndef ORAN7SCREENCAPTURE_H
#define ORAN7SCREENCAPTURE_H
#pragma once

#include <QObject>
#include <QPointer>
#include <QThread>
#include <QMetaObject>
#include <QDebug>
#include <atomic>
#include <QFile>
#include <QByteArray>

#include "D3d11Videoitem.h"
#include "Oran7VideoEncoder.h"
#include "Oran7Mp4Muxer.h"
#include "Oran7WhipPublisher.h"
#include "Oran7SignalingClient.h"
#include "Oran7AudioEncoder.h"

#ifdef _WIN32
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
#include <ksmedia.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <dxgi1_2.h>
using Microsoft::WRL::ComPtr;
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#endif

extern "C"{
#include <libavutil/frame.h>
#include <libavutil/buffer.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/pixdesc.h>
#include <libavfilter/buffersink.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

static QString ffErrStr(int err)
{
    char buf[256];
    av_strerror(err, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}

// ================= DDaGrabWorker (runs in its own QThread) =================
static constexpr int kPool = 3;

/**
 * @brief Windows 桌面画面抓取器
 */
class DdaGrabWorker : public QObject
{
    Q_OBJECT
public:
    DdaGrabWorker(D3D11VideoItem* item,
                  int outputIndex,
                  bool drawMouse,
                  int fps)
        : m_item(item),
        m_outputIndex(outputIndex),
        m_drawMouse(drawMouse),
        m_fps(fps)
    {

    }

    ~DdaGrabWorker() override
    {
        stop();
        cleanup();
    }

    // void bind_DdaGrab_d3dqtDev(ID3D11Device *dev){m_capDev = dev;}
    void setDebugLog(bool on) { m_debugLog.store(on, std::memory_order_release); }

    /// 录屏相关
    AVBufferRef* hwDeviceRef() const { return m_hwdev; }
    void setEncoder(std::shared_ptr<Oran7VideoEncoder>& enc) { m_encoder = enc; }
    void unRefEncoder(){m_encoder.reset();}
    int grabW() const { return m_grabW.load(std::memory_order_acquire); }
    int grabH() const { return m_grabH.load(std::memory_order_acquire); }
    /// 推流时把原生分辨率缩放到目标分辨率再喂编码器
    void setStreamTargetRes(int w, int h) { m_streamW.store(w); m_streamH.store(h); }

    // GrabOut-To-VideoEncoder
    std::atomic<bool> m_recordingEnabled{false};

public slots:
    void start();

    void stop(){
        INFO_LOG <<"Oran7ScreenCapture WorkerObject thread stop.";
        m_running.store(false);
    }

signals:
    void started();
    void stopped();
    void errorOccurred(const QString& err);

    void sharedHandlesReady(int w, int h, const QVector<quintptr>& hs);
    void newFrameReady(int idx);

private:
    QString initGraph();
    /// 枚举所有 DXGI adapter，返回第一个输出 attached 到桌面的 adapter 索引
    /// （用于在双显卡笔记本上选中显示器实际所在的 GPU，避免 DDA UNSUPPORTED）
    static int findDesktopAdapterIndex();
    bool ensureSharedPoolRGBA(int w, int h);

    bool ensureVideoProcessor(int w, int h);//Create NV12/P010 to RGBA Processor
    bool ensureTmpBgra(int w, int h);//Create temp BGRA Texture for transfor RGBA Texture
    bool ensureSwizzlePipeline(QString* err);//Create BGRA to RGBA of SwizzlePipeline Used by bgra2rgba_vs.hlsl and bgra2rgba_ps.hlsl

    bool vpNv12ToRgbaShared(int idx, ID3D11Texture2D* srcTex, int w, int h, int slice);//NV12/P010 to RGBA
    bool blitBgraToRgbaShared(int idx, ID3D11Texture2D* srcTex, int w, int h);//BGRA to RGBA

    bool vpBgraToNv12Shared(int idx,ID3D11Texture2D* srcTex,int w, int h, int slice,ID3D11Texture2D* dstTex = nullptr);//<------NOW MIAN USED

    /**
     * @brief blitToShared Make BGRA or NV12 or P010 Transfor RGBA
     * @note use in grabLoop()
     */
    bool blitToShared(int idx, ID3D11Texture2D* srcTex, const D3D11_TEXTURE2D_DESC& sdesc, int slice);

    void grabLoop();
    void cleanup();

    /// 录制相关

    /**
     * RGBA transform to NV12 for Encode
     */
    bool ensureEncodeNv12Tex(int w, int h);
    /// D3D11 帧→NV12：传完整 AVFrame 而非裸纹理指针，正确处理 data[1] 子资源
    AVFrame* downloadD3d11AndConvert(const AVFrame* hwFrame,
                                     int dstW, int dstH);

private:
    QPointer<D3D11VideoItem> m_item;

    //------FFmpeg created d3dDev Shared texture Pool------
    ComPtr<ID3D11Device> m_capDev;
    ComPtr<ID3D11DeviceContext> m_capCtx;
    ComPtr<ID3D11VideoDevice> m_capVideoDev;
    ComPtr<ID3D11VideoContext> m_capVideoCtx;
    ComPtr<ID3D11VideoProcessorEnumerator> m_vpEnum;
    ComPtr<ID3D11VideoProcessor> m_vp;
    QSize m_vpSize; // last (w,h) used to build vp
    ComPtr<ID3D11Texture2D> m_sharedTex[kPool];
    HANDLE m_sharedHandle[kPool]{};
    int m_poolW = 0, m_poolH = 0;

    //-------------BGRA temp (sampleable) ----------------
    ComPtr<ID3D11Texture2D> m_tmpBgra;
    QSize m_tmpBgraSize;

    // Swizzle pipeline
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader>  m_psBgraToRgba;
    ComPtr<ID3D11SamplerState> m_sampler;
    ComPtr<ID3D11RasterizerState> m_rs;
    ComPtr<ID3D11DepthStencilState> m_dss;
    ComPtr<ID3D11BlendState> m_blendOff;

    // Set Catch parameters
    int  m_outputIndex = 0;
    bool m_drawMouse = true;
    int  m_fps = 60;
    std::atomic_bool m_running{false};
    std::atomic_bool m_debugLog{false};  // 抓屏调试日志开关

    // FFmpeg AVFilterGraph (ddagrab -> buffersink，无 fps filter)
    AVFilterGraph*   m_graph = nullptr;
    AVFilterContext* m_src = nullptr;
    AVFilterContext* m_sink = nullptr;
    AVBufferRef*     m_hwdev = nullptr;

    //---------------------------------------------------

    /// 录制相关：私有成员
    std::atomic<int> m_grabW{0};
    std::atomic<int> m_grabH{0};
    std::atomic<int> m_streamW{0};   // 推流目标宽度（0=不缩放）
    std::atomic<int> m_streamH{0};   // 推流目标高度
    ComPtr<ID3D11Texture2D> m_encodeNv12Tex;
    std::shared_ptr<Oran7VideoEncoder> m_encoder;
    int64_t  m_recStartUs = 0;
    int64_t  m_lastPts = 0;
    bool m_lastRecEnabled = false;
};

// =============== WASPAIAudioWorker (runs in its own QThread) =================
/**
 *  @brief Windows桌面音频采集器
 */
class WASAPIAudioWorker{
public:
    struct WASAPIConfig
    {
        int sampleRate  = 48000;
        int channels = 2;
        int frameMs = 10; // WASAPI 帧周期
    };

    bool start(const WASAPIConfig& cfg);
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
    WASAPIConfig         _cfg;
    int64_t               _recStartUs = 0;  // wall clock
};

//  =====================  Oran7ScreenCaptureController  ==================

class  Oran7ScreenCaptureController : public QObject
{
    Q_OBJECT
public:
    explicit Oran7ScreenCaptureController(QObject *parent = nullptr);
    ~Oran7ScreenCaptureController() override;

    /*! @note 启停抓屏 */
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

    /*! @note 设置显示器 index（ddagrab 的 output_idx）*/
    Q_INVOKABLE void setOutputIndex(int idx);
    Q_INVOKABLE int outputIndex() const { return m_outputIndex; }

    /*! @note 设置是否抓鼠标 */
    Q_INVOKABLE void setDrawMouse(bool on);
    Q_INVOKABLE bool drawMouse() const { return m_drawMouse; }

    /*! @note 设置期望 fps（ddagrab 源头帧率） */
    Q_INVOKABLE void setFps(int fps);
    Q_INVOKABLE int fps() const { return m_fps; }

    /*! @note 开关抓屏调试日志（submitFrame fps / DUP 检测等） */
    Q_INVOKABLE void setDebugLog(bool on);

    // ========================= Render ==========================//
    ///*! @note 设置qml端QtQuickItem 渲染对象*/
    void setVideoItem(QObject* item);
    ///*! @note 绑定qml端QtQuickItem 渲染对象*/
    const QObject * bindVideoItem()const {return m_item;}

    void setD3D11Device(ID3D11Device* dev);//<-- Discard 现已改为在initGraph自动创建，非QtSceneGraphic的设置

    //========================= DataStreamCatch ===================//

    /** 录制输出模式 */
    enum class RecordMode {
        MuxerToMp4 = 0,  // 写 MP4 文件（默认，原行为）
        WhipPush    = 1,  // WHIP/WebRTC 推流到媒体服务器
    };
    Q_ENUM(RecordMode)

    Q_INVOKABLE bool startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE bool isRecording() const;

    /** 设置录制输出模式（必须在 startRecording 之前调用） */
    Q_INVOKABLE void setRecordMode(RecordMode mode) { m_recordMode = mode; }
    Q_INVOKABLE RecordMode recordMode() const { return m_recordMode; }

signals:
    void started();
    void stopped();
    void errorOccurred(const QString& err);
    void recordingStarted();
    void recordingStopped();

private slots:
    void onProviderReady(); //<-- Discard
    void onProviderLost(); //<-- Discard
    void onSharedHandlesReady(int w, int h,const QVector<quintptr>& hs); //Win8+D3DRHI
    void onNewFrameIndex(int idx);

private:
    void startWorkerIfPossible();
    void teardownWorker();
    void teardownEncoder();

private:
    /*! @note ddagrab config */
    int  m_outputIndex = 0;
    bool m_drawMouse = true;
    int  m_fps = 60;
    std::atomic_bool m_running{false};//running state
    std::atomic_bool m_debugLog{true};  // 抓屏调试日志开关

    // ========================= Render ==========================//
    QPointer<D3D11VideoItem> m_item;
#ifdef _WIN32
    ComPtr<ID3D11Device> m_d3dqtDev;
    //===  Qt hold d3dDev TexturePool  ===
    ComPtr<ID3D11Texture2D> m_qtTex[kPool];
    quintptr m_handle[kPool]{};
    int m_texW=0, m_texH=0;
    QMutex m_texMutex;  // 保护 m_qtTex / m_texW / m_texH  [跨线程读写]
    bool m_deviceReady = false;

    /*! @note ddagrab loop thread*/
    QThread* m_thread = nullptr;
    ///*! @note ddagrab loop worker object*/
    QObject* m_workerObj = nullptr;
#endif
    //=================== CaptureRecording ===================//
    // 共享录制起始壁钟（微秒），音频和视频 Worker 各自读取
    std::atomic<int64_t> m_recStartUs{0};
    RecordMode m_recordMode = RecordMode::MuxerToMp4;  // 默认写 MP4

    std::shared_ptr<Oran7VideoEncoder> m_encoder;
    std::shared_ptr<Oran7Mp4Muxer> m_muxer;
    std::shared_ptr<Oran7WhipPublisher> m_whipPublisher;  // WHIP 推流器
    std::unique_ptr<Oran7SignalingClient> m_signaling;     // 信令客户端
    std::unique_ptr<WASAPIAudioWorker> m_audioWorker;
    std::shared_ptr<Oran7AudioEncoder> m_audioEncoder;
    std::thread        m_audioBridge;
    std::atomic<bool>  m_audioBridgeRunning{false};
};

static void d3d11_tex_release(void* opaque, uint8_t* /*data*/)
{
    auto* tex = reinterpret_cast<ID3D11Texture2D*>(opaque);
    if (tex) tex->Release();
}

static AVFrame* makeD3D11FrameFromTexture(ID3D11Texture2D* tex, int w, int h)
{
    if (!tex) return nullptr;

    tex->AddRef(); // 让 frame 拥有一份引用

    AVFrame* f = av_frame_alloc();
    f->format = AV_PIX_FMT_D3D11;
    f->width  = w;
    f->height = h;

    // data[0] 存纹理指针
    f->data[0] = reinterpret_cast<uint8_t*>(tex);
    f->data[1] = nullptr; // BGRA 无 slice

    // 用 AVBufferRef 托管释放
    AVBufferRef* buf = av_buffer_create(
        /*data*/ nullptr,
        /*size*/ 0,
        /*free*/ d3d11_tex_release,
        /*opaque*/ tex,
        /*flags*/ 0
        );
    if (!buf) { tex->Release(); av_frame_free(&f); return nullptr; }
    f->buf[0] = buf;

    return f;
}

// =========================== Get Shaders =========================

static bool readTextResource(const QString& qrcPath, QByteArray& out, QString* err = nullptr)
{
    QFile f(qrcPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = "Failed to open " + qrcPath;
        return false;
    }
    out = f.readAll();
    if (out.isEmpty()) {
        if (err) *err = "Empty shader file: " + qrcPath;
        return false;
    }
    return true;
}

static bool compileHlslFromMemory(const QByteArray& srcUtf8,
                                  const char* entry,
                                  const char* target,
                                  ID3DBlob** blobOut,
                                  QString* errOut)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> code;
    ComPtr<ID3DBlob> err;

    HRESULT hr = D3DCompile(
        srcUtf8.constData(),
        srcUtf8.size(),
        nullptr,
        nullptr,
        nullptr,
        entry,
        target,
        flags,
        0,
        code.GetAddressOf(),
        err.GetAddressOf()
        );

    if (FAILED(hr) || !code) {
        const char* msg = err ? (const char*)err->GetBufferPointer() : "D3DCompile failed";
        if (errOut) *errOut = QString::fromUtf8(msg);
        return false;
    }

    *blobOut = code.Detach();
    return true;
}



#endif // ORAN7SCREENCAPTURE_H
