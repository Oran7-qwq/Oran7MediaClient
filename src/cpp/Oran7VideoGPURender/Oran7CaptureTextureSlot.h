#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <memory>
extern "C"
{
#include <libavutil/frame.h>
}

// 槽位由池与 AVFrame 租约共同持有；仍有帧引用时生产者不得重写。
struct Oran7CaptureTextureSlot
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex;
};

inline AVFrame* oran7MakeCaptureFrame(const std::shared_ptr<Oran7CaptureTextureSlot>& slot, int width, int height)
{
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return nullptr;
    auto lease = std::make_unique<std::shared_ptr<Oran7CaptureTextureSlot>>(slot);
    frame->buf[0] = av_buffer_create(
        nullptr, 0, [](void* opaque, uint8_t*)
        { delete static_cast<std::shared_ptr<Oran7CaptureTextureSlot>*>(opaque); }, lease.get(), 0);
    if (!frame->buf[0])
    {
        av_frame_free(&frame);
        return nullptr;
    }
    lease.release();
    frame->format = AV_PIX_FMT_D3D11;
    frame->width = width;
    frame->height = height;
    frame->data[0] = reinterpret_cast<uint8_t*>(slot->texture.Get());
    return frame;
}
