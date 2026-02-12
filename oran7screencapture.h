#ifndef ORAN7SCREENCAPTURE_H
#define ORAN7SCREENCAPTURE_H
#pragma once

#include <QObject>
#include <QScreen>
#include <QVideoSink>
#include <QMediaCaptureSession>
#include <QScreenCapture>
#include <QElapsedTimer>

#include "d3d11videoitem.h"

#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

extern "C"{
#include <libavutil/frame.h>
#include <libavutil/buffer.h>
}

class Oran7ScreenCaptureController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool previewing READ previewing NOTIFY previewingChanged FINAL)
public:
    explicit Oran7ScreenCaptureController(QObject *parent = nullptr);

    bool previewing()const {return m_previewing;}

    // 由外部设置渲染目标 by D3D11VideoItem
    void setVideoItem(D3D11VideoItem* item) { m_item = item; }

    // 由外部设置 Qt 的 D3D11 device（从 D3D11VideoItem::d3d11DeviceReady 拿）
    void setD3D11Device(ID3D11Device* dev);

    Q_INVOKABLE void startPreview(int screenIndex = 0);
    Q_INVOKABLE void stopPreview();
signals:
    void previewingChanged();
private:
    void onFrame(const QVideoFrame& f);
    // CPU BGRA -> CPU RGBA
    void bgraToRgba(uint8_t* dst, const uint8_t* src, int w, int h, int srcStride);
    // D3D11 texture cache
    bool ensureUploadTex(int w, int h);
    // AVFrame 包装-托管纹理引用
    AVFrame* wrapTexToAvFrame(ID3D11Texture2D* tex, int w, int h);
private:
    bool m_previewing;

    QMediaCaptureSession m_session;
    QScreenCapture m_screenCap;
    QVideoSink m_sink;

    QPointer<D3D11VideoItem> m_item;

    // D3D11
    ComPtr<ID3D11Device> m_dev;
    ComPtr<ID3D11DeviceContext> m_ctx;
    ComPtr<ID3D11Texture2D> m_uploadTex; // RGBA texture
    QSize m_uploadSize;

    QByteArray m_tmpRgba; // 临时 RGBA buffer
};

#endif // ORAN7SCREENCAPTURE_H
