#pragma once
#include "Oran7VideoFrameMailbox.h"
#include "D3d11DeviceProvider.h"
#include <QQuickItem>
#include <atomic>

// QML 兼容类型保留；GPU 状态只属于场景图节点，Item 只负责几何和 GUI 通知。
class D3D11VideoItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QRectF videoContentRect READ videoContentRect NOTIFY videoContentRectChanged)
  public:
    explicit D3D11VideoItem(QQuickItem* parent = nullptr);
    ~D3D11VideoItem() override;
    void submitFrame(AVFrame* frame);
    std::function<void(AVFrame*)> frameSubmitter() const;
    Q_INVOKABLE void renderBlackFrame();
    void setContentRect(const QRectF& rect);
    QRectF videoContentRect() const { return m_contentRect; }
    Q_INVOKABLE void setDebugLog(bool on) { m_debugLog.store(on); }
    Q_INVOKABLE bool debugLog() const { return m_debugLog.load(); }
  signals:
    void sourceSizeChanged(int w, int h);
    void sendVideoFrameInfo(Oran7VideoInfo info);
    void videoContentRectChanged();

  protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void releaseResources() override;

  private:
    void drainMailbox();
    std::shared_ptr<Oran7VideoFrameMailbox> m_mailbox = std::make_shared<Oran7VideoFrameMailbox>();
    QRectF m_contentRect;
    std::atomic_bool m_debugLog{false};
};
