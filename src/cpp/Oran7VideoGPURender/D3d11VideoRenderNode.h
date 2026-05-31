#ifndef D3D11VIDEORENDERNODE_H
#define D3D11VIDEORENDERNODE_H

#include <QSGRenderNode>
#include <QQuickWindow>
#include <QFile>
#include <QHash>
// #include <QShader>

#include <atomic>

#include "GlobalHelper.h"

extern "C" {
#include <libavformat/avformat.h>
}

#ifdef WIN32
#include <d3d11.h>
#include <wrl/client.h>
#include <rhi/qrhi.h>   // GuiPrivate
using Microsoft::WRL::ComPtr;
#endif

class D3D11VideoRenderNode : public QSGRenderNode
{
public:
    explicit D3D11VideoRenderNode(QQuickWindow *w);
    ~D3D11VideoRenderNode() override = default;

    void setFrame(AVFrame *f);
    void setRect(const QRectF &rect);
    void setContentRect(const QRectF &rect);
    void setNeedBlack(bool on);

    bool takePendingSourceSizeChanged(int &w, int &h);
    bool takePendingDxgifromat(DXGI_FORMAT &fmt);

    void prepare() override;
    void render(const RenderState *state) override;
    void releaseResources() override;

    RenderingFlags flags() const override;
    StateFlags changedStates() const override;
    QRectF rect() const override;

private:
    QQuickWindow *m_window = nullptr;
    QRectF m_rect;          // item 全区域（= videoHost），rect() 返回此值
    QRectF m_contentRect;   // Fit/Fill 后真实视频绘制区域，顶点用此值
    std::atomic<AVFrame *> m_latestFrame{nullptr};
    std::atomic<bool> m_needBlack{false};

    ComPtr<ID3D11Device> m_dev;
    ComPtr<ID3D11DeviceContext> m_ctx;
    ComPtr<ID3D11VideoDevice> m_videoDev;
    ComPtr<ID3D11VideoContext> m_videoCtx;
    ComPtr<ID3D11Texture2D> m_rgbaTex;
    ComPtr<ID3D11VideoProcessor> m_vp;
    ComPtr<ID3D11VideoProcessorEnumerator> m_vpEnum;
    QSize m_bgraSize;
    DXGI_FORMAT m_targetFmt = DXGI_FORMAT_UNKNOWN;

    int m_lastSrcW = 0;
    int m_lastSrcH = 0;

    bool m_hasPendingSizeChanged = false;
    int m_pendingW = 0;
    int m_pendingH = 0;
    bool m_hasPendingDXGIFormatChanged = false;
    DXGI_FORMAT m_pendingDxgiFmt = DXGI_FORMAT_UNKNOWN;

    bool initD3D11Resources();
    bool ensureRgbaTarget(int w, int h, DXGI_FORMAT fmt);
    bool ensureVideoProcessor(int w, int h);
    bool blitNv12ToRgba(ID3D11Texture2D *srcTex, int w, int h, int slice);
    void clearBlack();

    //QRhi
    bool m_rhiInited = false;
    bool m_verticesDirty = true;

    QRhi *m_rhi = nullptr;
    std::unique_ptr<QRhiTexture> m_rhiTex;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;

    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    // 按 render pass descriptor 缓存 pipeline。
    // 同一帧内 render() 会被调用两次（先 offscreen 后 main），
    // 每个 render target 需要独立的 pipeline，不能在 offscreen→main 切换时销毁 offscreen pipeline。
    QHash<QRhiRenderPassDescriptor *, QRhiGraphicsPipeline *> m_pipelineCache;

    bool m_importDirty = true;   // 需要重新 createFrom
    bool m_srbDirty = true;
    DXGI_FORMAT m_lastDxgiFormat = DXGI_FORMAT_UNKNOWN;
    ID3D11Texture2D *m_lastImportedTex = nullptr;

    bool ensureImportedTexture();
    void createSampler();
    struct Vertex {
        float x, y;
        float u, v;
    };
    struct UbufData {
        QMatrix4x4 mvp;
        float opacity = 1.0f;
        float pad[3] = {0, 0, 0}; // 16-byte 对齐
    };

    void createBuffers();
    void uploadStaticVerticesIfNeeded();
    QRhiGraphicsPipeline *createPipelineFor();
    void updateUniforms(const RenderState *state);
    void rebuildSrbAndPipelineIfNeeded();
    void doReleaseResources();

    bool createDebugGreenTexture();
};

static QShader loadShader(const QString &name)
{
    QFile f(name);
    if (!f.open(QIODevice::ReadOnly)) {
        WARNING_LOG << "Failed to open shader:" << name.toStdString();
        return {};
    }

    const QByteArray data = f.readAll();
    QShader shader = QShader::fromSerialized(data);
    if (!shader.isValid()) {
        WARNING_LOG << "Invalid serialized shader:" << name.toStdString();
    }
    return shader;
}


#endif // D3D11VIDEORENDERNODE_H
