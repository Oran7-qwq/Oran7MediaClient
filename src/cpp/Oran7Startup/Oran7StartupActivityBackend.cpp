#define NOMINMAX
#include "Oran7StartupActivityBackend.h"
#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <QDebug>

using Microsoft::WRL::ComPtr;
struct Oran7StartupActivityBackend::Oran7Native
{
    HWND child = nullptr;
    HWND owner = nullptr;
    QSize size;
    bool running = false;
    ComPtr<ID3D11Device> graphics;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDCompositionDevice> composition;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual> root, moving;
    ComPtr<IDCompositionAnimation> animation;
    ComPtr<IDCompositionEffectGroup> effect;
    ~Oran7Native()
    {
        if (target)
            target->SetRoot(nullptr);
        target.Reset();
        if (child && IsWindow(child))
            DestroyWindow(child);
    }
    HRESULT solid(const QColor& color, float width, float height, float x, IDCompositionVisual** visual)
    {
        ComPtr<IDCompositionSurface> surface;
        HRESULT hr =
            composition->CreateSurface(1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, &surface);
        if (FAILED(hr))
            return hr;
        ComPtr<IDXGISurface> update;
        POINT offset{};
        hr = surface->BeginDraw(nullptr, __uuidof(IDXGISurface), reinterpret_cast<void**>(update.GetAddressOf()),
                                &offset);
        if (FAILED(hr))
            return hr;
        ComPtr<ID3D11Texture2D> texture;
        hr = update.As(&texture);
        if (SUCCEEDED(hr))
        {
            const QRgb pixel = qPremultiply(color.rgba());
            const D3D11_BOX box{UINT(offset.x), UINT(offset.y), 0, UINT(offset.x + 1), UINT(offset.y + 1), 1};
            // 更新必须尊重 BeginDraw 给定的 atlas 偏移，不清空共享纹理的其他区域。
            context->UpdateSubresource(texture.Get(), 0, &box, &pixel, sizeof(pixel), sizeof(pixel));
        }
        const auto end = surface->EndDraw();
        if (FAILED(hr))
            return hr;
        if (FAILED(end))
            return end;
        ComPtr<IDCompositionScaleTransform> scale;
        if (FAILED(hr = composition->CreateScaleTransform(&scale)))
            return hr;
        if (FAILED(hr = scale->SetScaleX(width)))
            return hr;
        if (FAILED(hr = scale->SetScaleY(height)))
            return hr;
        if (FAILED(hr = composition->CreateVisual(visual)))
            return hr;
        if (FAILED(hr = (*visual)->SetContent(surface.Get())))
            return hr;
        if (FAILED(hr = (*visual)->SetTransform(scale.Get())))
            return hr;
        return (*visual)->SetOffsetX(x);
    }
};
Oran7StartupActivityBackend::Oran7StartupActivityBackend() = default;
Oran7StartupActivityBackend::~Oran7StartupActivityBackend() = default;
bool Oran7StartupActivityBackend::initialize(void* parentWindow, const QSize& size, const QColor& accent,
                                             const QColor& track, bool softwareOnly)
{
    m_native.reset();
    if (!parentWindow || !IsWindow(static_cast<HWND>(parentWindow)) || size.isEmpty())
        return false;
    auto state = std::make_unique<Oran7Native>();
    state->size = size;
    state->owner = static_cast<HWND>(parentWindow);
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Oran7StartupActivitySurface";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;
    // Qt 的透明 D3D 窗口可能把自己的合成树放在子 HWND 上方。
    // 使用无激活、无任务栏入口的 owned popup，独立于宿主的子窗口合成层级。
    state->child = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
                                   wc.lpszClassName, L"", WS_POPUP, 0, 0, size.width(), size.height(), state->owner,
                                   nullptr, wc.hInstance, nullptr);
    if (!state->child)
        return false;
    auto hr = D3D11CreateDevice(nullptr, softwareOnly ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &state->graphics,
                                nullptr, &state->context);
    if (FAILED(hr) && !softwareOnly)
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                               D3D11_SDK_VERSION, &state->graphics, nullptr, &state->context);
    if (FAILED(hr))
        return false;
    ComPtr<IDXGIDevice> dxgi;
    if (FAILED(state->graphics.As(&dxgi)))
        return false;
    if (FAILED(DCompositionCreateDevice(dxgi.Get(), __uuidof(IDCompositionDevice),
                                        reinterpret_cast<void**>(state->composition.GetAddressOf()))))
        return false;
    if (FAILED(state->composition->CreateTargetForHwnd(state->child, TRUE, &state->target)))
        return false;
    if (FAILED(state->composition->CreateVisual(&state->root)))
        return false;
    if (FAILED(state->composition->CreateEffectGroup(&state->effect)))
        return false;
    if (FAILED(state->root->SetEffect(state->effect.Get())))
        return false;
    const D2D_RECT_F clip{0, 0, float(size.width()), float(size.height())};
    if (FAILED(state->root->SetClip(clip)))
        return false;
    ComPtr<IDCompositionVisual> background, pulse, wrap;
    if (FAILED(state->solid(track, size.width(), size.height(), 0, &background)))
        return false;
    if (FAILED(state->solid(accent, size.width() * 0.3f, size.height(), 0, &pulse)))
        return false;
    if (FAILED(state->solid(accent, size.width() * 0.3f, size.height(), -float(size.width()), &wrap)))
        return false;
    if (FAILED(state->composition->CreateVisual(&state->moving)))
        return false;
    if (FAILED(state->moving->AddVisual(pulse.Get(), FALSE, nullptr)))
        return false;
    if (FAILED(state->moving->AddVisual(wrap.Get(), TRUE, pulse.Get())))
        return false;
    if (FAILED(state->root->AddVisual(background.Get(), FALSE, nullptr)))
        return false;
    if (FAILED(state->root->AddVisual(state->moving.Get(), TRUE, background.Get())))
        return false;
    if (FAILED(state->target->SetRoot(state->root.Get())))
        return false;
    if (FAILED(state->composition->Commit()))
        return false;
    m_native = std::move(state);
    return true;
}
bool Oran7StartupActivityBackend::setRunning(bool running)
{
    if (!m_native)
        return false;
    auto& s = *m_native;
    if (s.running == running)
        return true;
    HRESULT hr = S_OK;
    if (running)
    {
        ComPtr<IDCompositionAnimation> animation;
        if (FAILED(hr = s.composition->CreateAnimation(&animation)))
            return false;
        // 1600ms 向右一周，合成器自主插值/重复，无 GUI 定时器或逐帧 Commit。
        if (FAILED(hr = animation->AddCubic(0, 0, s.size.width() / 1.6f, 0, 0)))
            return false;
        if (FAILED(hr = animation->AddRepeat(1.6, 1.6)))
            return false;
        if (FAILED(hr = s.moving->SetOffsetX(animation.Get())))
            return false;
        s.animation = animation;
    }
    else
    {
        if (FAILED(hr = s.moving->SetOffsetX(0.0f)))
            return false;
        s.animation.Reset();
    }
    if (FAILED(s.composition->Commit()))
        return false;
    s.running = running;
    return true;
}
bool Oran7StartupActivityBackend::present(const QRect& bounds, bool visible, qreal opacity)
{
    if (!m_native || !IsWindow(m_native->child) || !IsWindow(m_native->owner))
        return false;
    BOOL valid = FALSE;
    if (FAILED(m_native->composition->CheckDeviceState(&valid)) || !valid)
        return false;
    POINT position{bounds.x(), bounds.y()};
    if (!ClientToScreen(m_native->owner, &position))
        return false;
    if (!SetWindowPos(m_native->child, HWND_TOP, position.x, position.y, bounds.width(), bounds.height(),
                      SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)))
        return false;
    if (FAILED(m_native->effect->SetOpacity(float(qBound(0.0, opacity, 1.0)))))
        return false;
    return SUCCEEDED(m_native->composition->Commit());
}
