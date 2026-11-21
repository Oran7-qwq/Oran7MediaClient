#include "Oran7WindowCaptureWorker.h"
#include "Oran7CaptureFrameConverter.h"
#include "Oran7WindowCaptureAbi.h"
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QThread>
#include <d3d11_4.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <iterator>

using Microsoft::WRL::ComPtr;
namespace Wgc = Oran7WindowCaptureAbi;

namespace
{
struct Oran7ListedWindow
{
    Oran7CaptureWindowTarget target;
    QString title;
    QSize size;
};

Oran7ListedWindow describeWindow(HWND window)
{
    if (!window || window == GetShellWindow() || !IsWindowVisible(window) || GetAncestor(window, GA_ROOT) != window)
        return {};
    const auto style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((style & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) ||
        (GetWindow(window, GW_OWNER) && !(style & WS_EX_APPWINDOW)))
        return {};
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
        return {};
    wchar_t caption[512]{};
    const int length = GetWindowTextW(window, caption, int(std::size(caption)));
    const QString title = QString::fromWCharArray(caption, length).trimmed();
    if (title.isEmpty())
        return {};
    Oran7CaptureWindowTarget target{window};
    target.thread = GetWindowThreadProcessId(window, &target.process);
    if (!target.thread)
        return {};
    RECT bounds{};
    if (FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds))))
        GetWindowRect(window, &bounds);
    const QSize size(bounds.right - bounds.left, bounds.bottom - bounds.top);
    return {target, title, size.isEmpty() ? QSize(640, 480) : size};
}

QString executablePath(DWORD processId)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process)
        return {};
    wchar_t path[2048]{};
    DWORD length = std::size(path);
    const bool found = QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    if (!found)
        return {};
    return QString::fromWCharArray(path, int(length));
}

template <typename T, size_t N> HRESULT factory(const wchar_t (&name)[N], REFIID id, ComPtr<T>& result)
{
    HSTRING_HEADER header{};
    HSTRING className = nullptr;
    const HRESULT hr = WindowsCreateStringReference(name, UINT32(N - 1), &header, &className);
    return FAILED(hr) ? hr : RoGetActivationFactory(className, id, reinterpret_cast<void**>(result.GetAddressOf()));
}

template <typename T> void closeObject(ComPtr<T>& object)
{
    if (!object)
        return;
    ComPtr<Wgc::Closable> closable;
    if (SUCCEEDED(object->QueryInterface(Wgc::closableId, reinterpret_cast<void**>(closable.GetAddressOf()))))
        closable->Close();
    object.Reset();
}

class Oran7WindowClosedHandler final : public Wgc::ClosedHandler
{
  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (id != IID_IUnknown && id != IID_IAgileObject && id != Wgc::closedHandlerId)
            return E_NOINTERFACE;
        *result = static_cast<Wgc::ClosedHandler*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_references; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const auto remaining = --m_references;
        if (!remaining)
            delete this;
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE Invoke(Wgc::Item*, IInspectable*) override
    {
        closed.store(true);
        return S_OK;
    }
    std::atomic_bool closed{false};

  private:
    std::atomic_ulong m_references{1};
};

}

bool Oran7CaptureWindowTarget::matches() const
{
    DWORD actualProcess = 0;
    return window && thread && GetWindowThreadProcessId(window, &actualProcess) == thread && actualProcess == process;
}

QString Oran7CaptureWindowTarget::token() const
{
    return QStringLiteral("%1:%2:%3").arg(quintptr(window), 0, 16).arg(process).arg(thread);
}

QVariantList oran7CaptureWindows()
{
    QVariantList result;
    EnumWindows([](HWND handle, LPARAM context) -> BOOL {
        const auto entry = describeWindow(handle);
        if (entry.target.window)
        {
            auto& windows = *reinterpret_cast<QVariantList*>(context);
            const auto executable = executablePath(entry.target.process);
            wchar_t className[256]{};
            const int classLength = GetClassNameW(handle, className, int(std::size(className)));
            windows.append(QVariantMap{{"token", entry.target.token()}, {"name", entry.title},
                {"application", executable.mid(executable.lastIndexOf(QLatin1Char('\\')) + 1)},
                {"executable", executable}, {"windowClass", QString::fromWCharArray(className, classLength)},
                {"width", entry.size.width()},
                {"height", entry.size.height()}, {"minimized", !!IsIconic(handle)}});
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}

QString oran7MatchCaptureWindow(const QVariantMap& identity, const QVariantList& candidates)
{
    const auto executable = identity.value("executable").toString();
    const auto name = identity.value("name").toString();
    const auto className = identity.value("windowClass").toString();
    if (name.isEmpty() || className.isEmpty())
        return {};
    QString match;
    for (const auto& candidate : candidates)
    {
        const auto window = candidate.toMap();
        if (window.value("executable").toString().compare(executable, Qt::CaseInsensitive) ||
            window.value("windowClass").toString() != className || window.value("name").toString() != name)
            continue;
        if (!match.isEmpty())
            return {}; // 同一应用有多个同名窗口时保留待连接，避免自动选错窗口。
        match = window.value("token").toString();
    }
    return match;
}

Oran7CaptureWindowTarget oran7FindCaptureWindow(const QString& token)
{
    // 以包含进程和线程身份的完整令牌为准。窗口关闭后不跟随新的句柄所有者。
    bool ok = false;
    const auto numericHandle = token.section(QLatin1Char(':'), 0, 0).toULongLong(&ok, 16);
    if (!ok || quint64(quintptr(numericHandle)) != numericHandle)
        return {};
    const auto entry = describeWindow(reinterpret_cast<HWND>(quintptr(numericHandle)));
    return entry.target.window && entry.target.token() == token ? entry.target : Oran7CaptureWindowTarget{};
}

Oran7WindowCaptureWorker::Oran7WindowCaptureWorker(Oran7CaptureWindowTarget target, bool drawMouse, int fps,
    std::shared_ptr<Oran7CapturePreviewFanout> preview, std::shared_ptr<Oran7CaptureScene> scene,
    std::shared_ptr<Oran7SceneFrameMailbox> frames)
    : m_target(target), m_drawMouse(drawMouse), m_fps(fps), m_preview(std::move(preview)),
      m_scene(std::move(scene)), m_frames(std::move(frames))
{
}

void Oran7WindowCaptureWorker::start()
{
    Q_ASSERT(thread() == QThread::currentThread());
    const auto error = m_control->stopRequested.load() ? QString() : capture();
    m_frames->close();
    if (!error.isEmpty() && !m_control->stopRequested.load())
        emit errorOccurred(error);
    emit stopped();
}

QString Oran7WindowCaptureWorker::capture()
{
    if (FAILED(RoInitialize(RO_INIT_MULTITHREADED)))
        return QStringLiteral("无法初始化 Windows 窗口采集");
    const auto apartment = qScopeGuard([] { RoUninitialize(); });
    ComPtr<Wgc::SessionStatics> sessionFactory;
    boolean supported = false;
    if (FAILED(factory(L"Windows.Graphics.Capture.GraphicsCaptureSession", Wgc::sessionStaticsId, sessionFactory)) ||
        FAILED(sessionFactory->IsSupported(&supported)) || !supported)
        return QStringLiteral("当前系统不支持窗口采集");

    ComPtr<Wgc::ItemInterop> interop;
    ComPtr<Wgc::Item> target;
    if (FAILED(factory(L"Windows.Graphics.Capture.GraphicsCaptureItem", Wgc::itemInteropId, interop)))
        return QStringLiteral("当前系统不支持窗口采集");
    if (!m_target.matches() || FAILED(interop->CreateForWindow(m_target.window, Wgc::itemId,
            reinterpret_cast<void**>(target.GetAddressOf()))) || !m_target.matches())
        return QStringLiteral("无法采集此窗口，窗口可能已关闭");

    ComPtr<Oran7WindowClosedHandler> closed;
    closed.Attach(new Oran7WindowClosedHandler);
    Wgc::Token closedToken{};
    const bool subscribed = SUCCEEDED(target->add_Closed(closed.Get(), &closedToken));
    const auto event = qScopeGuard([&] { if (subscribed) target->remove_Closed(closedToken); });

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf())))
        return QStringLiteral("无法创建窗口采集设备");
    ComPtr<ID3D11Multithread> multithread;
    if (SUCCEEDED(context.As(&multithread)))
        multithread->SetMultithreadProtected(TRUE);
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IInspectable> wrappedDevice;
    ComPtr<IInspectable> graphicsDevice;
    if (FAILED(device.As(&dxgi)) || FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(), wrappedDevice.GetAddressOf())) ||
        FAILED(wrappedDevice->QueryInterface(Wgc::graphicsDeviceId, reinterpret_cast<void**>(graphicsDevice.GetAddressOf()))))
        return QStringLiteral("无法创建窗口采集设备");

    Wgc::Size poolSize{};
    ComPtr<Wgc::FramePoolStatics> poolFactory;
    ComPtr<Wgc::FramePool> pool;
    ComPtr<Wgc::Session> session;
    const auto sessionLifetime = qScopeGuard([&] { closeObject(session); closeObject(pool); });
    constexpr int buffers = 2;
    constexpr auto format = DXGI_FORMAT_B8G8R8A8_UNORM;
    if (FAILED(target->get_Size(&poolSize)) || poolSize.width <= 0 || poolSize.height <= 0 ||
        FAILED(factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool", Wgc::framePoolStaticsId, poolFactory)) ||
        FAILED(poolFactory->CreateFreeThreaded(graphicsDevice.Get(), format, buffers, poolSize, pool.GetAddressOf())) ||
        FAILED(pool->CreateCaptureSession(target.Get(), session.GetAddressOf())))
        return QStringLiteral("无法创建窗口采集会话");
    ComPtr<Wgc::SessionCursor> cursor;
    if (SUCCEEDED(session->QueryInterface(Wgc::cursorId, reinterpret_cast<void**>(cursor.GetAddressOf()))))
        cursor->put_IsCursorCaptureEnabled(m_drawMouse);
    if (FAILED(session->StartCapture()))
        return QStringLiteral("无法启动窗口采集");

    Oran7CaptureFrameConverter converter(device.Get(), m_fps);
    QElapsedTimer clock;
    clock.start();
    const qint64 interval = 1000000000 / std::max(1, m_fps);
    qint64 nextFrame = 0;
    qint64 awaitingFrameSince = 0;
    bool hasPicture = false;
    bool hasOutput = false;
    Oran7CaptureFrameConverter::Frame lastFrame;
    QString reportedStatus;
    const auto publishOutput = [&](const AVFrame* source) {
        if (auto output = converter.recording(source, source->width & ~1, source->height & ~1))
        {
            m_frames->submit(std::move(output));
            hasOutput = true;
        }
    };
    const auto unavailable = [&](const QString& reason) {
        if (reportedStatus == reason)
            return;
        hasPicture = hasOutput = false;
        lastFrame.reset();
        m_frames->submit({});
        reportedStatus = reason;
        emit temporarilyUnavailable(reason);
    };

    while (!m_control->stopRequested.load())
    {
        if (closed->closed.load() || !m_target.matches())
            return QStringLiteral("采集窗口已关闭");
        const qint64 now = clock.nsecsElapsed();
        if (now < nextFrame)
        {
            QThread::msleep(unsigned(std::clamp<qint64>((nextFrame - now) / 1000000, 1, 16)));
            continue;
        }
        nextFrame = now + interval;
        const bool outputActive = m_scene->outputActive.load();
        if (!outputActive && hasOutput)
        {
            m_frames->submit({});
            hasOutput = false;
        }
        const bool hidden = IsIconic(m_target.window) || !IsWindowVisible(m_target.window);
        ComPtr<Wgc::Frame> frame;
        const auto frameLifetime = qScopeGuard([&] { closeObject(frame); });
        for (int i = 0; i < buffers; ++i)
        {
            ComPtr<Wgc::Frame> newest;
            if (FAILED(pool->TryGetNextFrame(newest.GetAddressOf())))
            {
                if (hidden)
                    break;
                return QStringLiteral("窗口采集画面已中断");
            }
            if (!newest)
                break;
            closeObject(frame);
            frame = std::move(newest);
        }
        if (hidden)
        {
            awaitingFrameSince = now;
            unavailable(QStringLiteral("窗口已最小化或隐藏"));
            continue;
        }
        if (!frame)
        {
            if (!hasPicture && now - awaitingFrameSince >= 3000000000LL)
                unavailable(QStringLiteral("等待窗口画面"));
            // 静止窗口不保证继续产帧，开始编码时沿用上一张有效预览纹理。
            if (outputActive && !hasOutput && lastFrame)
            {
                auto* texture = reinterpret_cast<ID3D11Texture2D*>(lastFrame->data[0]);
                ComPtr<IDXGIKeyedMutex> mutex;
                if (SUCCEEDED(texture->QueryInterface(IID_PPV_ARGS(&mutex))) && mutex->AcquireSync(0, 0) == S_OK)
                {
                    publishOutput(lastFrame.get());
                    context->Flush();
                    if (FAILED(mutex->ReleaseSync(0)))
                        return QStringLiteral("窗口采集画面已中断");
                }
            }
            continue;
        }

        Wgc::Size size{};
        if (FAILED(frame->get_ContentSize(&size)) || size.width < 2 || size.height < 2)
            continue;
        if (size.width != poolSize.width || size.height != poolSize.height)
        {
            closeObject(frame);
            if (FAILED(pool->Recreate(graphicsDevice.Get(), format, buffers, size)))
                return QStringLiteral("无法更新窗口采集尺寸");
            poolSize = size;
            continue;
        }

        ComPtr<IInspectable> surface;
        ComPtr<Wgc::DxgiInterfaceAccess> access;
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(frame->get_Surface(surface.GetAddressOf())) || !surface ||
            FAILED(surface->QueryInterface(Wgc::accessId, reinterpret_cast<void**>(access.GetAddressOf()))) ||
            FAILED(access->GetInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(texture.GetAddressOf()))))
            return QStringLiteral("无法读取窗口采集画面");
        AVFrame raw{};
        raw.format = AV_PIX_FMT_D3D11;
        raw.width = size.width;
        raw.height = size.height;
        raw.data[0] = reinterpret_cast<uint8_t*>(texture.Get());
        auto preview = converter.preview(&raw);
        if (!preview.error.isEmpty())
            return QStringLiteral("无法读取窗口采集画面");
        if (preview.frame)
        {
            if (auto* copy = av_frame_clone(preview.frame.get()))
                lastFrame.reset(copy);
            m_preview->submit(preview.frame.release());
            if (!hasPicture)
            {
                hasPicture = true;
                reportedStatus.clear();
                emit firstFrameReady();
            }
        }
        if (outputActive)
            publishOutput(&raw);
    }
    return {};
}
