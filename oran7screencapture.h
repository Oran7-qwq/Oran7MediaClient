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
public:
    explicit Oran7ScreenCaptureController(QObject *parent = nullptr);

signals:

private:

private:

};

#endif // ORAN7SCREENCAPTURE_H
