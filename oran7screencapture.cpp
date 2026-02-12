#include "oran7screencapture.h"
#include <QGuiApplication>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QDebug>

static void release_d3d11_tex(void* opaque,uint8_t *data){
    auto *tex=reinterpret_cast<ID3D11Texture2D*>(opaque);
    if(tex) tex->Release();
}

Oran7ScreenCaptureController::Oran7ScreenCaptureController(QObject *parent)
    : QObject(parent)
{
    m_session.setScreenCapture(&m_screenCap);
    m_session.setVideoSink(&m_sink);

    connect(&m_sink,&QVideoSink::videoFrameChanged,
            this,&Oran7ScreenCaptureController::onFrame,
            Qt::QueuedConnection);
}

void Oran7ScreenCaptureController::setD3D11Device(ID3D11Device *dev)
{
    m_dev = dev;
    if(m_dev)
    {
        m_dev->GetImmediateContext(&m_ctx);
        INFO_LOG<<"Successed set ScreenCapture d3d11device.";
    }
    else
    {
        m_ctx.Reset();
    }
}

void Oran7ScreenCaptureController::startPreview(int screenIndex)
{
    QList<QScreen *> screens = QGuiApplication::screens();
    INFO_LOG << "Available screens: ";
    for (int i = 0; i < screens.size(); ++i) {
        INFO_LOG << "Screen " << i << ": " << screens[i]->name().toStdString();
    }

    // 确保屏幕索引有效
    if (screenIndex >= 0 && screenIndex < screens.size()) {
        INFO_LOG << "Setting screen: " << screens[screenIndex]->name().toStdString();
        m_screenCap.setScreen(screens[screenIndex]);

        INFO_LOG << "Setting QScreenCapture active status...";
        m_screenCap.setActive(true);
        QCoreApplication::processEvents();  // 确保异步操作完成

        INFO_LOG << "QScreenCapture active status after setActive(true): " << m_screenCap.isActive();
    } else {
        INFO_LOG << "Invalid screen index: " << screenIndex;
    }


    m_previewing = true;
    emit previewingChanged();

    INFO_LOG << "Preview started.";
}


void Oran7ScreenCaptureController::stopPreview()
{
    m_screenCap.setActive(false);
    m_previewing = false;
    emit previewingChanged();
}

void Oran7ScreenCaptureController::onFrame(const QVideoFrame &f)
{
    INFO_LOG << "Received frame: " << f.pixelFormat() << ", " << f.width() << "x" << f.height();
    if (!m_item) {
        WARNNING_LOG <<"Oran7ScreenCaptureController of D3D11VideoItem is nullptr!";
        return;
    }
    if (!m_dev || !m_ctx) {
        WARNNING_LOG <<"Oran7ScreenCaptureController of device or conctext is nullptr!";
        return;
    }
    if (!f.isValid()) {
        WARNNING_LOG << "Oran7ScreenCaptureController received invalid frame!";
        return;
    }

    QVideoFrame frame(f);

    const int w = frame.width();
    const int h = frame.height();
    if (w <= 0 || h <= 0) return;

    // 只处理 CPU 可映射的 4通道格式（MVP）
    if (!frame.map(QVideoFrame::ReadOnly)) {
        // 有些情况下 frame 是 GPU 纹理，不可直接 map
        // MVP 先跳过，后面再做零拷贝/纹理提取
        return;
    }
    const auto fmt = frame.pixelFormat();
    const uint8_t* src = frame.bits(0);
    const int srcStride = frame.bytesPerLine(0);

    INFO_LOG<<"QScreenGetFrame Format:"<<(fmt == QVideoFrameFormat::Format_BGRA8888 ? "Format_BGRA8888" : "valid");

    // 准备 upload texture
    if (!ensureUploadTex(w, h)) {
        frame.unmap();
        return;
    }

    // 准备 RGBA buffer
    m_tmpRgba.resize(w * h * 4);
    uint8_t* rgba = reinterpret_cast<uint8_t*>(m_tmpRgba.data());

    bgraToRgba(rgba, src, w, h, srcStride);

    frame.unmap();

    // 上传到 D3D11 texture
    D3D11_BOX box{};
    box.left = 0; box.top = 0; box.front = 0;
    box.right = (UINT)w; box.bottom = (UINT)h; box.back = 1;

    m_ctx->UpdateSubresource(m_uploadTex.Get(), 0, &box, rgba, w * 4, 0);

    // 包装成 AVFrame 并提交给 D3D11VideoItem
    AVFrame* avf = wrapTexToAvFrame(m_uploadTex.Get(), w, h);
    m_item->submitFrame(avf); // item 内部会 av_frame_free
}

void Oran7ScreenCaptureController::bgraToRgba(uint8_t *dst, const uint8_t *src, int w, int h, int srcStride)
{
    for(int y = 0;y<h;y++){
        const uint8_t * s = src + y * srcStride;
        uint8_t * d = dst + y * w * 4;
        for(int x = 0; x<w ;x++){
            d[0] = s[2]; //R
            d[1] = s[1]; //G
            d[2] = s[0]; //B
            d[3] = s[3]; //A
            d += 4;
            s += 4;
        }
    }
}

bool Oran7ScreenCaptureController::ensureUploadTex(int w, int h)
{
    if(!m_dev)return false;
    if(m_uploadTex && m_uploadSize == QSize(w,h)) return true;

    m_uploadTex.Reset();
    m_uploadSize = QSize();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = (UINT)w;
    desc.Height = (UINT)h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //RGBA
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = true;

    HRESULT hr=m_dev->CreateTexture2D(&desc,nullptr,&m_uploadTex);
    if(FAILED(hr)|| !m_uploadTex){
        qWarning() << "CreateTexture2D uploadTex failed hr=" << Qt::hex << hr;
        return false;
    }
    return true;
}

AVFrame *Oran7ScreenCaptureController::wrapTexToAvFrame(ID3D11Texture2D *tex, int w, int h)
{
    AVFrame *f=av_frame_alloc();
    tex->AddRef();
    f->format = AV_PIX_FMT_D3D11;
    f->width = w;
    f->height = h;
    f->data[0] = reinterpret_cast<uint8_t*>(tex);
    f->data[1] = reinterpret_cast<uint8_t*>(0); // slice=0
    f->buf[0] = av_buffer_create(
        nullptr, 0,
        [](void* opaque, uint8_t* data){
            release_d3d11_tex(opaque, data);
        },
        tex,
        0
    );
    return f;
}
