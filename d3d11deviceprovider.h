#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>

#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

class D3D11DeviceProvider : public QObject
{
    Q_OBJECT
public:
    static D3D11DeviceProvider* instance(); // 单例

    // 绑定某个 Window
    Q_INVOKABLE void attachWindow(QQuickWindow* w);

    // 当前是否可用
    Q_INVOKABLE bool isReady() const;

    // 安全 getter-->返回带引用计数的指针，调用者负责 Release
    ID3D11Device* acquireDevice() const;

#ifdef _WIN32
    ID3D11Device* device() const { return m_dev.Get(); }
    ID3D11DeviceContext* context() const { return m_ctx.Get(); }
    ID3D11VideoDevice* videoDevice() const { return m_videoDev.Get(); }
    ID3D11VideoContext* videoContext() const { return m_videoCtx.Get(); }
#endif

signals:
#ifdef _WIN32
    void deviceReady();
#endif
    void deviceLost();

private:
    explicit D3D11DeviceProvider(QObject* parent = nullptr);
    void hookWindow(QQuickWindow* w);
    void tryInitFromWindow(QQuickWindow* w);
    void cleanup();

private:
    QPointer<QQuickWindow> m_window;
    bool m_connected = false;

#ifdef _WIN32
    ComPtr<ID3D11Device> m_dev;
    ComPtr<ID3D11DeviceContext> m_ctx;
    ComPtr<ID3D11VideoDevice> m_videoDev;
    ComPtr<ID3D11VideoContext> m_videoCtx;
#endif
};
