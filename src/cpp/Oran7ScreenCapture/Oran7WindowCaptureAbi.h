#pragma once

#include <inspectable.h>
#include <dxgi.h>
#include <roapi.h>
#include <winstring.h>

// MinGW 11 的 SDK 未提供这些 WinRT 接口。只声明 Windows SDK 中使用的 ABI 槽位，
// 所有接口从系统运行时取得。接口顺序和 GUID 对应 windows.graphics.capture.h 及其 interop 头。
namespace Oran7WindowCaptureAbi
{
struct Size { INT32 width, height; };
struct Token { INT64 value; };
struct Item;
struct Session;
struct Frame;
struct FramePool;

struct ClosedHandler : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Invoke(Item*, IInspectable*) = 0;
};
struct ItemInterop : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE CreateForWindow(HWND, REFIID, void**) = 0;
};
struct Item : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_DisplayName(HSTRING*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Size(Size*) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_Closed(ClosedHandler*, Token*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_Closed(Token) = 0;
};
struct Frame : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_Surface(IInspectable**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_SystemRelativeTime(INT64*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ContentSize(Size*) = 0;
};
struct FramePool : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE Recreate(IInspectable*, INT32, INT32, Size) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryGetNextFrame(Frame**) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameArrived(IUnknown*, Token*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameArrived(Token) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateCaptureSession(Item*, Session**) = 0;
};
struct FramePoolStatics : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE CreateFreeThreaded(IInspectable*, INT32, INT32, Size, FramePool**) = 0;
};
struct Session : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE StartCapture() = 0;
};
struct SessionCursor : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_IsCursorCaptureEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsCursorCaptureEnabled(boolean) = 0;
};
struct SessionStatics : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE IsSupported(boolean*) = 0;
};
struct DxgiInterfaceAccess : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID, void**) = 0;
};
struct Closable : IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};

inline constexpr GUID itemInteropId{0x3628e81b, 0x3cac, 0x4c60, {0xb7,0xf4,0x23,0xce,0x0e,0x0c,0x33,0x56}};
inline constexpr GUID itemId{0x79c3f95b, 0x31f7, 0x4ec2, {0xa4,0x64,0x63,0x2e,0xf5,0xd3,0x07,0x60}};
inline constexpr GUID closedHandlerId{0xe9c610c0, 0xa68c, 0x5bd9, {0x80,0x21,0x85,0x89,0x34,0x6e,0xee,0xe2}};
inline constexpr GUID framePoolStaticsId{0x589b103f, 0x6bbc, 0x5df5, {0xa9,0x91,0x02,0xe2,0x8b,0x3b,0x66,0xd5}};
inline constexpr GUID cursorId{0x2c39ae40, 0x7d2e, 0x5044, {0x80,0x4e,0x8b,0x67,0x99,0xd4,0xcf,0x9e}};
inline constexpr GUID sessionStaticsId{0x2224a540, 0x5974, 0x49aa, {0xb2,0x32,0x08,0x82,0x53,0x6f,0x4c,0xb5}};
inline constexpr GUID accessId{0xa9b3d012, 0x3df2, 0x4ee3, {0xb8,0xd1,0x86,0x95,0xf4,0x57,0xd3,0xc1}};
inline constexpr GUID graphicsDeviceId{0xa37624ab, 0x8d5f, 0x4650, {0x9d,0x3e,0x9e,0xae,0x3d,0x9b,0xc6,0x70}};
inline constexpr GUID closableId{0x30d5a829, 0x7fa4, 0x4026, {0x83,0xbb,0xd7,0x5b,0xae,0x4e,0xa9,0x9e}};
}

extern "C" HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice*, IInspectable**);
