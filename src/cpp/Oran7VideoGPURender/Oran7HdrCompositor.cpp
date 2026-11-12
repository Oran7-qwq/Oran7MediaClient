#include "Oran7HdrCompositor.h"
#include <QFile>
#include <private/qrhid3d11_p.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace
{
QShader shader(const char* path)
{
    QFile file(QString::fromLatin1(path));
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader{};
}
} // namespace
void Oran7HdrCompositor::release()
{
    m_pipeline.reset();
    m_bindings.reset();
    m_uniform.reset();
    m_sampler.reset();
    m_copy.reset();
    m_descriptor.reset();
    m_target = nullptr;
    m_rhi = nullptr;
    m_size = {};
}
bool Oran7HdrCompositor::prepare(QRhi* rhi, QRhiRenderTarget* target, float whiteNits)
{
    if (!rhi || rhi->backend() != QRhi::D3D11 || !target || target->pixelSize().isEmpty())
        return false;
    m_whiteScale = qBound(1.0f, whiteNits / 80.0f, 12.5f);
    if (m_pipeline && m_rhi == rhi && m_target == target && m_size == target->pixelSize() &&
        m_pipeline->sampleCount() == target->sampleCount() &&
        m_descriptor->isCompatible(target->renderPassDescriptor()))
        return true;
    release();
    m_rhi = rhi;
    m_target = target;
    m_size = target->pixelSize();
    m_copy.reset(rhi->newTexture(QRhiTexture::RGBA16F, m_size, 1));
    m_sampler.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                    QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    if (!m_copy->create() || !m_sampler->create())
        return false;
    m_uniform.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));
    if (!m_uniform->create())
        return false;
    m_bindings.reset(rhi->newShaderResourceBindings());
    m_bindings->setBindings(
        {QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_copy.get(),
                                                   m_sampler.get()),
         QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_uniform.get())});
    if (!m_bindings->create())
        return false;
    const auto vertex = shader(":/shaders/hdrComposite.vert.qsb");
    const auto fragment = shader(":/shaders/hdrComposite.frag.qsb");
    if (!vertex.isValid() || !fragment.isValid())
        return false;
    m_descriptor.reset(target->renderPassDescriptor()->newCompatibleRenderPassDescriptor());
    m_pipeline.reset(rhi->newGraphicsPipeline());
    m_pipeline->setShaderStages({{QRhiShaderStage::Vertex, vertex}, {QRhiShaderStage::Fragment, fragment}});
    m_pipeline->setShaderResourceBindings(m_bindings.get());
    m_pipeline->setRenderPassDescriptor(m_descriptor.get());
    m_pipeline->setSampleCount(target->sampleCount());
    return m_pipeline->create();
}
bool Oran7HdrCompositor::render(QRhiCommandBuffer* cb, QRhiTexture* source)
{
    if (!cb || !m_pipeline)
        return false;
    auto* uniforms = m_rhi->nextResourceUpdateBatch();
    const float values[4] = {m_whiteScale, 0, 0, 0};
    uniforms->updateDynamicBuffer(m_uniform.get(), 0, sizeof(values), values);
    cb->resourceUpdate(uniforms);
    if (source)
    {
        auto* updates = m_rhi->nextResourceUpdateBatch();
        updates->copyTexture(m_copy.get(), source);
        cb->resourceUpdate(updates);
    }
    else
    {
        // Qt 6.7.3 的后端内部入口仅用于取得当前 RTV；不持有 swapchain/Qt 的资源。
        // beginExternal 会先执行已排队的 Qt 命令，避免在 UI 绘制完成前复制空白缓冲。
        ID3D11RenderTargetView* view = nullptr;
        if (m_target->resourceType() == QRhiResource::SwapChainRenderTarget)
            view = static_cast<QD3D11SwapChainRenderTarget*>(m_target)->d.rtv[0];
        else if (m_target->resourceType() == QRhiResource::TextureRenderTarget)
            view = static_cast<QD3D11TextureRenderTarget*>(m_target)->d.rtv[0];
        if (!view)
            return false;
        ComPtr<ID3D11Resource> backBuffer;
        view->GetResource(&backBuffer);
        auto* destination = reinterpret_cast<ID3D11Texture2D*>(m_copy->nativeTexture().object);
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        destination->GetDevice(&device);
        device->GetImmediateContext(&context);
        cb->beginExternal();
        ID3D11ShaderResourceView* empty[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
        context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, empty);
        if (m_target->sampleCount() > 1)
            context->ResolveSubresource(destination, 0, backBuffer.Get(), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
        else
            context->CopyResource(destination, backBuffer.Get());
        cb->endExternal();
    }
    cb->beginPass(m_target, Qt::transparent, {1.0f, 0});
    cb->setGraphicsPipeline(m_pipeline.get());
    cb->setViewport(QRhiViewport(0, 0, float(m_size.width()), float(m_size.height())));
    cb->setShaderResources(m_bindings.get());
    cb->draw(3);
    cb->endPass();
    return true;
}
