#include "d3d11deviceprovider.h"
#include "globalhelper.h"

#include <QSGRendererInterface>
#include <QDebug>

D3D11DeviceProvider* D3D11DeviceProvider::instance()
{
    static D3D11DeviceProvider inst;
    return &inst;
}

D3D11DeviceProvider::D3D11DeviceProvider(QObject* parent)
    : QObject(parent)
{
}

void D3D11DeviceProvider::attachWindow(QQuickWindow* w)
{
    if (!w) return;
    if (m_window == w)
    {
        // 晚订阅-->如果已经 ready
#ifdef _WIN32
        // if (isReady())
        //     emit deviceReady(/*m_dev.Get()*/); //Discard
        //晚订阅--->调用者使用 isReady() && acquireDevice()主动拉取
#endif
        return;
    }

    // 切换 window：先清理旧状态
    cleanup();
    m_window = w;
    hookWindow(w);

    // 关键：如果 scene graph 已初始化，信号不会重放，所以这里主动尝试初始化
    if (w->isSceneGraphInitialized()) {
        tryInitFromWindow(w);
    }
}

bool D3D11DeviceProvider::isReady() const
{
#ifdef _WIN32
    return m_dev != nullptr && m_ctx != nullptr;
#else
    return false;
#endif
}

/**
 * @brief D3D11DeviceProvider::acquireDevice
 * @note !!! must be by Attach() Get reference for ComPtr
 * @return
 */
ID3D11Device *D3D11DeviceProvider::acquireDevice()
{
    ID3D11Device* dev = m_dev.Get();
    if (dev) dev->AddRef();
    return dev;
}

void D3D11DeviceProvider::hookWindow(QQuickWindow* w)
{
    if (!w || m_connected) return;
    m_connected = true;

    // sceneGraphInitialized 只在“创建 SG 时”触发一次；晚连上就收不到，所以 attachWindow 里会补 tryInitFromWindow
    connect(w, &QQuickWindow::sceneGraphInitialized, this, [this, w]() {
        tryInitFromWindow(w);
    }, Qt::DirectConnection);

    connect(w, &QQuickWindow::sceneGraphInvalidated, this, [this]() {
        INFO_LOG<<"QQuickWindow::sceneGraphInvalidated.";
        cleanup();
        emit deviceLost();
    }, Qt::DirectConnection);

    // 如果 window 被销毁，也清掉
    connect(w, &QObject::destroyed, this, [this]() {
        cleanup();
        emit deviceLost();
    }, Qt::DirectConnection);
}

void D3D11DeviceProvider::tryInitFromWindow(QQuickWindow* w)
{
#ifndef _WIN32
    Q_UNUSED(w);
    return;
#else
    if (!w) return;

    auto* ri = w->rendererInterface();
    if (!ri) return;

    auto* dev = static_cast<ID3D11Device*>(
        ri->getResource(w, QSGRendererInterface::DeviceResource)
        );

    if (!dev) {
        WARNING_LOG << "D3D11DeviceProvider : getResource(DeviceResource) returned null";
        return;
    }

    // 避免重复初始化
    if (m_dev.Get() == dev && isReady())
    {
        INFO_LOG<<"D3D11DeviceProvider : d3dDev Allready.";
        emit deviceReady(/*m_dev.Get()*/); // 允许“后来 attach/后来监听”的模块也能被唤醒
        return;
    }

    // 更新 device/context
    m_dev = dev;
    m_ctx.Reset();
    dev->GetImmediateContext(&m_ctx);
    ComPtr<ID3D10Multithread> mt;
    if (SUCCEEDED(m_ctx.As(&mt)) && mt) {
        mt->SetMultithreadProtected(TRUE);
    }

    m_videoDev.Reset();
    m_videoCtx.Reset();
    dev->QueryInterface(IID_ID3D11VideoDevice, (void**)&m_videoDev);
    m_ctx->QueryInterface(IID_ID3D11VideoContext, (void**)&m_videoCtx);

    INFO_LOG<<"D3D11DeviceProvider : d3dDev is Ready.";
    emit deviceReady(/*m_dev.Get()*/);
#endif
}

void D3D11DeviceProvider::cleanup()
{
#ifdef _WIN32
    m_videoCtx.Reset();
    m_videoDev.Reset();
    m_ctx.Reset();
    m_dev.Reset();
#endif
    m_connected = false;
    m_window = nullptr;
}
