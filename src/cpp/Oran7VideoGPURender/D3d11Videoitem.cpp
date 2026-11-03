#include "D3d11Videoitem.h"
#include "D3d11VideoRenderNode.h"
#include <QThread>

D3D11VideoItem::D3D11VideoItem(QQuickItem* parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    m_mailbox->attach(this, [this] { drainMailbox(); });
}
D3D11VideoItem::~D3D11VideoItem()
{
    m_mailbox->close();
}
void D3D11VideoItem::submitFrame(AVFrame* frame)
{
    m_mailbox->submit(frame);
}
std::function<void(AVFrame*)> D3D11VideoItem::frameSubmitter() const
{
    const auto mailbox = m_mailbox;
    return [mailbox](AVFrame* frame) { mailbox->submit(frame); };
}
void D3D11VideoItem::renderBlackFrame()
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_mailbox->clear();
    update();
}
void D3D11VideoItem::setContentRect(const QRectF& rect)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_contentRect == rect)
        return;
    m_contentRect = rect;
    emit videoContentRectChanged();
    update();
}
void D3D11VideoItem::drainMailbox()
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto delivery = m_mailbox->drain();
    if (delivery.sizeChanged)
        emit sourceSizeChanged(delivery.sourceSize.width(), delivery.sourceSize.height());
    if (delivery.infoChanged)
    {
        if (m_debugLog.load(std::memory_order_relaxed))
            INFO_LOG << "QRhi video input fps=" << delivery.info.fps << "format=" << delivery.info.dxgiFormatName;
        emit sendVideoFrameInfo(delivery.info);
    }
    if (delivery.update)
        update();
}
QSGNode* D3D11VideoItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<D3D11VideoRenderNode*>(oldNode);
    if (!node)
        node = new D3D11VideoRenderNode(window(), m_mailbox);
    node->setRect(boundingRect());
    node->setContentRect(m_contentRect);
    node->markDirty(QSGNode::DirtyMaterial);
    return node;
}
void D3D11VideoItem::releaseResources()
{
    // 节点及 QRhi 资源由 Qt 在渲染线程回收；此处不得释放 GPU 对象。
    m_mailbox->clear();
}
