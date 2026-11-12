#pragma once
#include "Oran7VideoFrameMailbox.h"
#include "Oran7VideoColorSpace.h"
#include <QSGRenderNode>
#include <QQuickWindow>
#include <rhi/qrhi.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
extern "C"
{
#include <libswscale/swscale.h>
}
using Microsoft::WRL::ComPtr;

// 唯一帧消费点为 prepare()；render() 只提交已准备的 QRhi 绘制命令。
class D3D11VideoRenderNode : public QSGRenderNode
{
  public:
    D3D11VideoRenderNode(QQuickWindow* window, std::shared_ptr<Oran7VideoFrameMailbox> mailbox);
    ~D3D11VideoRenderNode() override;
    void setRect(const QRectF& rect);
    void setContentRect(const QRectF& rect);
    void prepare() override;
    void render(const RenderState* state) override;
    void releaseResources() override;
    RenderingFlags flags() const override;
    StateFlags changedStates() const override;
    QRectF rect() const override { return m_rect; }

  private:
    struct Vertex
    {
        float x, y, u, v;
    };
    struct UbufData
    {
        float mvp[16] = {};
        float opacity = 1;
        float black = 0;
        float pad[2] = {};
        float mode[4] = {}; // YUV、传递函数、BT.2020、原生 HDR
        float range[4] = {};
        float coefficients[4] = {};
        float uvScale[4] = {1, 1, 1, 1};
    };
    static_assert(offsetof(UbufData, opacity) == 64 && offsetof(UbufData, mode) == 80 && sizeof(UbufData) == 144,
                  "视频 uniform 必须匹配着色器 std140 布局");
    struct Oran7RenderPass
    {
        QRhiRenderTarget* target = nullptr; // 只比较身份，不跨帧解引用。
        int samples = 1;
        std::unique_ptr<QRhiRenderPassDescriptor> descriptor;
        std::unique_ptr<QRhiBuffer> uniform;
        std::unique_ptr<QRhiShaderResourceBindings> bindings;
        std::unique_ptr<QRhiGraphicsPipeline> pipeline;
    };
    bool initD3D11Resources();
    bool ensureRgbaTarget(int w, int h, DXGI_FORMAT format);
    bool uploadCpuFrameToRgba(AVFrame* frame, bool rgbFallback = false);
    bool uploadHardwareFrame(AVFrame* frame);
    void clearBlack();
    void consumeFrame();
    bool ensureImportedTexture();
    Oran7RenderPass* preparePass();
    void doReleaseResources();
    QQuickWindow* m_window;
    std::shared_ptr<Oran7VideoFrameMailbox> m_mailbox;
    QMetaObject::Connection m_frameConnection;
    QMetaObject::Connection m_releaseConnection;
    quint64 m_releaseSerial = 0;
    quint64 m_frameSerial = 1, m_consumedSerial = 0;
    QRectF m_rect, m_contentRect;
    ComPtr<ID3D11Device> m_dev;
    ComPtr<ID3D11DeviceContext> m_ctx;
    ComPtr<ID3D11Texture2D> m_rgbaTex;
    ComPtr<ID3D11Texture2D> m_chromaTex;
    Oran7VideoFrame m_cpuFrame;
    Oran7VideoColorSpace m_color;
    QSize m_sourceSize;
    bool m_yuv = false;
    bool m_yuv16 = false;
    SwsContext* m_swsCtx = nullptr;
    DXGI_FORMAT m_currentDxgiFormat = DXGI_FORMAT_UNKNOWN;
    bool m_verticesDirty = true, m_importDirty = true, m_blackFrame = false;
    bool m_uploadFailureLogged = false;
    bool m_sharedTextureBusy = false;
    QRhi* m_rhi = nullptr; // Qt 拥有，只在渲染线程使用。
    std::unique_ptr<QRhiTexture> m_rhiTex;
    std::unique_ptr<QRhiTexture> m_rhiChroma;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::vector<std::unique_ptr<Oran7RenderPass>> m_passes;
    Oran7RenderPass* m_activePass = nullptr;
};
