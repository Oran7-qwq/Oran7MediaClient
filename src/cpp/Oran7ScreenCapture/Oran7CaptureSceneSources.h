#pragma once

#include "Oran7CaptureScene.h"
#include "Oran7WindowCaptureWorker.h"
#include <QObject>
#include <QString>
#include <QTimer>
#include <optional>

class QQuickItem;
class QThread;
class D3D11VideoItem;
struct Oran7CaptureControl;

// GUI 线程管理显示器、窗口会话及预览宿主，采集线程只持帧回调、取消标记和邮箱。
class Oran7CaptureSceneSources : public QObject
{
  public:
    using Composition = std::pair<int, Oran7CaptureComposition>;
    explicit Oran7CaptureSceneSources(std::function<void(int, const QString&)> reportError);
    ~Oran7CaptureSceneSources() override;
    const std::shared_ptr<Oran7CaptureScene>& scene() const { return m_scene; }
    const std::shared_ptr<Oran7CapturePreviewFanout>& primaryPreview() const { return m_primaryPreview; }
    bool hasLayers() const { return !m_layers.empty(); }
    void setCompositions(std::vector<Composition> layers);
    int addLayer(const QString& device, int outputIndex);
    int addWindowLayer(Oran7CaptureWindowTarget target);
    int addSavedWindowLayer(const QVariantMap& identity, Oran7CaptureWindowTarget target);
    int addImageLayer(std::shared_ptr<AVFrame> frame);
    void removeLayer(int id);
    QObject* attachLayer(int id, QQuickItem* host);
    bool preparePrimary(const QString& device);
    void start(int fps, bool drawMouse, const std::map<QString, int>& outputs);
    void stop();
    void shutdown();

  private:
    struct Layer
    {
        QString device;
        std::unique_ptr<D3D11VideoItem> item;
        QMetaObject::Connection widthChanged, heightChanged;
        std::shared_ptr<Oran7SceneFrameMailbox> image;
    };
    struct Source
    {
        int outputIndex = -1;
        std::optional<Oran7CaptureWindowTarget> window;
        QVariantMap windowIdentity;
        bool failed = false;
        std::shared_ptr<Oran7CapturePreviewFanout> preview = std::make_shared<Oran7CapturePreviewFanout>();
        std::shared_ptr<Oran7SceneFrameMailbox> frames;
        std::shared_ptr<Oran7CaptureControl> control;
        std::unique_ptr<QThread> thread;
    };
    void rebuildScene();
    int addSourceLayer(const QString& device, int outputIndex, std::optional<Oran7CaptureWindowTarget> window,
                       const QVariantMap& identity = {});
    void reconcileSources();
    void startSource(const QString& name, Source& source);
    void stopSource(Source& source);
    void reapSources();
    void retryWindows();
    void reportFailure(const QString& device, const QString& error);
    std::map<int, Layer> m_layers;
    std::map<QString, Source> m_sources;
    std::vector<Composition> m_compositions;
    QString m_primary;
    int m_nextId = 0;
    int m_fps = 60;
    bool m_drawMouse = true;
    bool m_running = false, m_closed = false;
    std::function<void(int, const QString&)> m_reportError;
    QTimer m_reapTimer;
    QTimer m_windowRetry;
    std::shared_ptr<Oran7CapturePreviewFanout> m_primaryPreview = std::make_shared<Oran7CapturePreviewFanout>();
    std::shared_ptr<Oran7CaptureScene> m_scene = std::make_shared<Oran7CaptureScene>();
};
