#pragma once
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QMutex>
#include <QDebug>
#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/buffer.h>
}
#define INFO_LOG qDebug()
#define WARNING_LOG qWarning()
struct Oran7VideoInfo { QString fillModeName, sourceTag; };
inline QVariantMap Oran7VideoInfo_To_QVariantMap(const Oran7VideoInfo& i) { return {{"sourceTag",i.sourceTag}}; }
class D3D11DeviceProvider : public QObject {
 Q_OBJECT
public:
 static D3D11DeviceProvider* instance() { static D3D11DeviceProvider p; return &p; }
 void attachWindow(QQuickWindow*) {}
 ID3D11Device* acquireDevice() { return nullptr; }
signals:
 void deviceReady();
};
class D3D11VideoItem : public QQuickItem {
 Q_OBJECT
public:
 using QQuickItem::QQuickItem;
 int received=0, lastWidth=0;
 QRectF rect;
 void submitFrame(AVFrame* frame) { ++received; lastWidth=frame->width; av_frame_free(&frame); }
 void renderBlackFrame() {}
 void setContentRect(const QRectF& r) { rect=r; }
signals:
 void sourceSizeChanged(int,int);
 void sendVideoFrameInfo(Oran7VideoInfo);
};
