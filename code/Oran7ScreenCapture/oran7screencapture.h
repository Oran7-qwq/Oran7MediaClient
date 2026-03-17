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

#include "d3d11videoitem.h"

#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>
#include <dxgi1_2.h>
using Microsoft::WRL::ComPtr;
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#endif

extern "C"{
#include <libavutil/frame.h>
#include <libavutil/buffer.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavfilter/buffersink.h>
}

static QString ffErrStr(int err)
{
    char buf[256];
    av_strerror(err, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}

static constexpr int kPool = 3;

//static QMutex g_ffmpegD3DMutex; //Discard 2026/2/24

// ================= Worker (runs in its own QThread) =================
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
        // m_d3dDev = d3dDev;
        // if (m_d3dDev) m_d3dDev->AddRef(); //Discard
    }

    ~DdaGrabWorker() override
    {
        stop();
        cleanup();
        // if (m_d3dDev) m_d3dDev->Release();
        // m_d3dDev = nullptr;//Discard
    }

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
    bool ensureSharedPoolRGBA(int w, int h);

    bool ensureVideoProcessor(int w, int h);//Create NV12/P010 to RGBA Processor
    bool ensureTmpBgra(int w, int h);//Create temp BGRA Texture for transfor RGBA Texture
    bool ensureSwizzlePipeline(QString* err);//Create BGRA to RGBA of SwizzlePipeline Used by bgra2rgba_vs.hlsl and bgra2rgba_ps.hlsl

    bool vpNv12ToRgbaShared(int idx, ID3D11Texture2D* srcTex, int w, int h, int slice);//NV12/P010 to RGBA
    bool blitBgraToRgbaShared(int idx, ID3D11Texture2D* srcTex, int w, int h);//BGRA to RGBA、

    bool vpBgraToNv12Shared(int idx,ID3D11Texture2D* srcTex,int w, int h,int slice);//BGRA ->NV12 <------NOW MIAN USED

    /**
     * @brief blitToShared Make BGRA or NV12 or P010 Transfor RGBA
     * @note use in grabLoop()
     */
    bool blitToShared(int idx, ID3D11Texture2D* srcTex, const D3D11_TEXTURE2D_DESC& sdesc, int slice);

    void grabLoop();
    void cleanup();

private:
    QPointer<D3D11VideoItem> m_item;

    // ComPtr<ID3D11Device> m_d3dqtDev = nullptr; //Discard 2026/2/24

    //=== FFmpeg created d3dDev Shared texture Pool ===
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
    // =================================

    // =======  BGRA temp (sampleable) =========
    ComPtr<ID3D11Texture2D> m_tmpBgra;
    QSize m_tmpBgraSize;

    // Swizzle pipeline
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader>  m_psBgraToRgba;
    ComPtr<ID3D11SamplerState> m_sampler;
    ComPtr<ID3D11RasterizerState> m_rs;
    ComPtr<ID3D11DepthStencilState> m_dss;
    ComPtr<ID3D11BlendState> m_blendOff;
    //==================================

    // Set Catch parameters
    int  m_outputIndex = 0;
    bool m_drawMouse = true;
    int  m_fps = 60;
    std::atomic_bool m_running{false};

    // FFmpeg AVFilterGraph
    AVFilterGraph*   m_graph = nullptr;
    AVFilterContext* m_src = nullptr;
    AVFilterContext* m_fpsCtx = nullptr;
    AVFilterContext* m_sink = nullptr;
    AVBufferRef*     m_hwdev = nullptr;
};

//  =====================  Oran7ScreenCaptureController  ==================

class  Oran7ScreenCaptureController : public QObject
{
    Q_OBJECT
public:
    explicit Oran7ScreenCaptureController(QObject *parent = nullptr);
    ~Oran7ScreenCaptureController() override;

    // 启停抓屏
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

    // 设置显示器 index（ddagrab 的 output_idx）
    Q_INVOKABLE void setOutputIndex(int idx);
    Q_INVOKABLE int outputIndex() const { return m_outputIndex; }

    // 设置是否抓鼠标
    Q_INVOKABLE void setDrawMouse(bool on);
    Q_INVOKABLE bool drawMouse() const { return m_drawMouse; }

    // 设置期望 fps（会插入 fps filter，尽量稳定输出）
    Q_INVOKABLE void setFps(int fps);
    Q_INVOKABLE int fps() const { return m_fps; }

    // ========================= Render ==========================//
    void setVideoItem(QObject* item);
    const QObject * bindVideoItem()const {return m_item;}
    void setD3D11Device(ID3D11Device* dev);

    //========================= DataStreamCatch ===================//

signals:
    void started();
    void stopped();
    void errorOccurred(const QString& err);

private slots:
    void onProviderReady();
    void onProviderLost();
    void onSharedHandlesReady(int w, int h,const QVector<quintptr>& hs); //Win8+D3DRHI
    void onNewFrameIndex(int idx);

private:
    void startWorkerIfPossible();
    void teardownWorker();

private:
    // config
    int  m_outputIndex = 0;
    bool m_drawMouse = true;
    int  m_fps = 60;
    std::atomic_bool m_running{false};//running state

    // ========================= Render ==========================//
    QPointer<D3D11VideoItem> m_item;
#ifdef _WIN32
    ComPtr<ID3D11Device> m_d3dqtDev;
    //===  Qt hold d3dDev TexturePool  ===
    ComPtr<ID3D11Texture2D> m_qtTex[kPool];
    quintptr m_handle[kPool]{};
    int m_texW=0, m_texH=0;
    bool m_deviceReady = false;

    // worker thread
    QThread* m_thread = nullptr;
    QObject* m_workerObj = nullptr;
    //==========================
#endif
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
