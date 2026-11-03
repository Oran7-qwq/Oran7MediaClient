#pragma once
#include <QQuickItem>
#include <functional>
#include "Oran7GlobalHelper.h"
extern "C"
{
#include <libavutil/frame.h>
}
class D3D11VideoItem : public QQuickItem
{
    Q_OBJECT
  public:
    using QQuickItem::QQuickItem;
    void submitFrame(AVFrame* frame) { av_frame_free(&frame); }
    std::function<void(AVFrame*)> frameSubmitter() const
    {
        return [](AVFrame* frame) { av_frame_free(&frame); };
    }
    Q_INVOKABLE void renderBlackFrame() {}
    void setDebugLog(bool) {}
};
