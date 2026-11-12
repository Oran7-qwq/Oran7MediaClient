#pragma once
#include <memory>
#include <rhi/qrhi.h>

// Qt 6.7 的普通 QML 以 sRGB 数值合成；只在最终 FP16 HDR 表面提交前线性化一次。
class Oran7HdrCompositor
{
  public:
    bool prepare(QRhi* rhi, QRhiRenderTarget* target, float whiteNits = 80.0f);
    bool render(QRhiCommandBuffer* cb, QRhiTexture* redirectedSource = nullptr);
    void release();

  private:
    QRhi* m_rhi = nullptr;
    QRhiRenderTarget* m_target = nullptr;
    QSize m_size;
    std::unique_ptr<QRhiTexture> m_copy;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_bindings;
    std::unique_ptr<QRhiBuffer> m_uniform;
    float m_whiteScale = 1.0f;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;
    std::unique_ptr<QRhiRenderPassDescriptor> m_descriptor;
};
