#include "Oran7CaptureSceneSources.h"
#include "D3d11Videoitem.h"
#include "Oran7DesktopCaptureWorker.h"
#include <QThread>
#include <QQmlEngine>
#include <QPointer>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <algorithm>
#include <type_traits>

Oran7CaptureSceneSources::Oran7CaptureSceneSources(std::function<void(int, const QString&)> reportError)
    : m_reportError(std::move(reportError))
{
    m_reapTimer.setSingleShot(true);
    m_reapTimer.setInterval(10);
    connect(&m_reapTimer, &QTimer::timeout, this, &Oran7CaptureSceneSources::reapSources);
    m_windowRetry.setInterval(1500);
    connect(&m_windowRetry, &QTimer::timeout, this, &Oran7CaptureSceneSources::retryWindows);
}

Oran7CaptureSceneSources::~Oran7CaptureSceneSources() { shutdown(); }

void Oran7CaptureSceneSources::setCompositions(std::vector<Composition> layers)
{
    m_compositions = std::move(layers);
    rebuildScene();
}

int Oran7CaptureSceneSources::addLayer(const QString& device, int outputIndex)
{
    return addSourceLayer(device, outputIndex, std::nullopt);
}

int Oran7CaptureSceneSources::addImageLayer(std::shared_ptr<AVFrame> frame)
{
    if (m_closed)
        return 0;
    const int id = ++m_nextId;
    auto& layer = m_layers[id];
    layer.image = std::make_shared<Oran7SceneFrameMailbox>();
    layer.image->submit(std::move(frame));
    return id;
}

int Oran7CaptureSceneSources::addWindowLayer(Oran7CaptureWindowTarget target)
{
    return target.matches() ? addSourceLayer(QStringLiteral("window:") + target.token(), -1, target) : 0;
}

int Oran7CaptureSceneSources::addSavedWindowLayer(const QVariantMap& source, Oran7CaptureWindowTarget target)
{
    const QVariantMap identity{{"name", source.value("name")}, {"windowClass", source.value("windowClass")},
                               {"executable", source.value("executable").toString().toCaseFolded()}};
    const auto stable = QJsonDocument::fromVariant(identity).toJson(QJsonDocument::Compact);
    auto key = QStringLiteral("saved-window:") + QString::fromLatin1(QCryptographicHash::hash(stable, QCryptographicHash::Sha256).toHex());
    const auto existing = m_sources.find(key);
    const bool connected = target.matches();
    if (connected && existing != m_sources.end() && existing->second.window &&
        existing->second.window->token() != target.token())
        key += QStringLiteral(":") + target.token();
    else if (!connected && existing != m_sources.end() && existing->second.window)
        key += QStringLiteral(":waiting");
    return addSourceLayer(key, -1, connected ? std::optional<Oran7CaptureWindowTarget>(target) : std::nullopt, identity);
}

int Oran7CaptureSceneSources::addSourceLayer(const QString& device, int outputIndex,
                                            std::optional<Oran7CaptureWindowTarget> window, const QVariantMap& identity)
{
    if (m_closed || device.isEmpty())
        return 0;
    const int id = ++m_nextId;
    auto& layer = m_layers[id];
    layer.device = device;
    layer.item = std::make_unique<D3D11VideoItem>();
    QQmlEngine::setObjectOwnership(layer.item.get(), QQmlEngine::CppOwnership);
    auto& source = m_sources[device];
    source.outputIndex = outputIndex;
    if (!source.thread && (window || !source.window))
        source.window = window;
    source.windowIdentity = identity;
    if (source.failed && !source.thread && (window || identity.isEmpty()))
        source.failed = false;
    auto fanout = device == m_primary ? m_primaryPreview : source.preview;
    fanout->setConsumer(id, layer.item->frameSubmitter());
    reconcileSources();
    return id;
}

void Oran7CaptureSceneSources::removeLayer(int id)
{
    const auto layer = m_layers.find(id);
    if (layer == m_layers.end())
        return;
    m_primaryPreview->setConsumer(id, {});
    if (const auto source = m_sources.find(layer->second.device); source != m_sources.end())
        source->second.preview->setConsumer(id, {});
    m_layers.erase(layer);
    reconcileSources();
}

QObject* Oran7CaptureSceneSources::attachLayer(int id, QQuickItem* host)
{
    const auto found = m_layers.find(id);
    if (m_closed || found == m_layers.end() || !found->second.item || !host)
        return nullptr;
    auto& layer = found->second;
    auto* item = layer.item.get();
    if (item->parentItem() == host)
        return item;
    disconnect(layer.widthChanged);
    disconnect(layer.heightChanged);
    item->setParentItem(host);
    const auto fit = [item, host] {
        const QSizeF size(host->width(), host->height());
        item->setSize(size);
        item->setContentRect(QRectF(QPointF(), size));
    };
    layer.widthChanged = connect(host, &QQuickItem::widthChanged, item, fit);
    layer.heightChanged = connect(host, &QQuickItem::heightChanged, item, fit);
    fit();
    return item;
}

bool Oran7CaptureSceneSources::preparePrimary(const QString& device)
{
    if (device != m_primary)
    {
        m_primary = device;
        for (const auto& entry : m_layers)
        {
            const auto& layer = entry.second;
            if (layer.image)
                continue;
            auto& source = m_sources.at(layer.device);
            const bool primary = layer.device == m_primary;
            m_primaryPreview->setConsumer(entry.first, primary ? layer.item->frameSubmitter() : nullptr);
            source.preview->setConsumer(entry.first, primary ? nullptr : layer.item->frameSubmitter());
            layer.item->renderBlackFrame();
        }
        reconcileSources();
    }
    const auto candidate = m_sources.find(device);
    return candidate == m_sources.end() || !candidate->second.thread;
}

void Oran7CaptureSceneSources::start(int fps, bool drawMouse, const std::map<QString, int>& outputs)
{
    m_running = true;
    m_fps = fps;
    m_drawMouse = drawMouse;
    for (auto& entry : m_sources)
    {
        auto& source = entry.second;
        source.failed = false;
        if (source.window || !source.windowIdentity.isEmpty())
            continue;
        const auto output = outputs.find(entry.first);
        source.outputIndex = output != outputs.end() ? output->second : -1;
        if (source.outputIndex < 0 && entry.first != m_primary)
            reportFailure(entry.first, QStringLiteral("显示器已断开"));
    }
    reconcileSources();
}

void Oran7CaptureSceneSources::stop()
{
    m_running = false;
    m_windowRetry.stop();
    for (auto& entry : m_sources)
        stopSource(entry.second);
    for (const auto& entry : m_layers)
        if (entry.second.item)
            entry.second.item->renderBlackFrame();
    rebuildScene();
}

void Oran7CaptureSceneSources::shutdown()
{
    if (m_closed)
        return;
    m_closed = true;
    m_reapTimer.stop();
    stop();
    for (auto& entry : m_sources)
        if (entry.second.thread)
            entry.second.thread->wait();
    for (const auto& entry : m_layers)
        m_primaryPreview->setConsumer(entry.first, {});
    m_sources.clear();
    m_layers.clear();
}

void Oran7CaptureSceneSources::rebuildScene()
{
    std::vector<Oran7CaptureScene::Layer> scene;
    for (const auto& entry : m_compositions)
    {
        if (entry.first == 0)
        {
            scene.push_back({entry.second, {}, true});
            continue;
        }
        const auto layer = m_layers.find(entry.first);
        if (layer == m_layers.end())
            continue;
        if (layer->second.image)
        {
            scene.push_back({entry.second, layer->second.image, false, true});
            continue;
        }
        const auto& device = layer->second.device;
        auto& source = m_sources.at(device);
        scene.push_back({entry.second, source.frames, device == m_primary});
    }
    m_scene->setLayers(std::move(scene));
}

void Oran7CaptureSceneSources::reconcileSources()
{
    bool waitingForWindow = false;
    for (auto it = m_sources.begin(); it != m_sources.end();)
    {
        auto& [name, source] = *it;
        const bool used = std::any_of(m_layers.begin(), m_layers.end(), [&](const auto& layer) {
            return layer.second.device == name;
        });
        waitingForWindow |= m_running && used && !source.window && !source.windowIdentity.isEmpty();
        if (m_running && used && name != m_primary && (source.window || source.outputIndex >= 0) && !source.failed)
        {
            if (!source.thread)
                startSource(name, source);
        }
        else
            stopSource(source);
        if (!used && !source.thread)
            it = m_sources.erase(it);
        else
            ++it;
    }
    if (waitingForWindow && !m_windowRetry.isActive())
        m_windowRetry.start();
    else if (!waitingForWindow)
        m_windowRetry.stop();
    rebuildScene();
}

void Oran7CaptureSceneSources::retryWindows()
{
    const auto windows = oran7CaptureWindows();
    for (auto& entry : m_sources)
    {
        auto& source = entry.second;
        if (source.thread || source.window || source.windowIdentity.isEmpty())
            continue;
        const auto match = oran7FindCaptureWindow(oran7MatchCaptureWindow(source.windowIdentity, windows));
        if (match.window)
        {
            source.window = match;
            source.failed = false;
        }
    }
    reconcileSources();
}

void Oran7CaptureSceneSources::startSource(const QString& name, Source& source)
{
    source.frames = std::make_shared<Oran7SceneFrameMailbox>();
    source.thread = std::make_unique<QThread>();
    auto* thread = source.thread.get();
    const QPointer<QThread> generation(thread);
    const auto preview = source.preview;
    const auto current = [this, name, generation]() -> Source* {
        const auto it = m_sources.find(name);
        if (!generation || it == m_sources.end() || it->second.thread.get() != generation.data() ||
            it->second.control->stopRequested.load())
            return nullptr;
        return &it->second;
    };
    const auto failed = [this, name, current](const QString& error) {
        if (auto* active = current())
        {
            active->failed = true;
            if (!active->windowIdentity.isEmpty() && active->window &&
                (!active->window->matches() || error == QStringLiteral("采集窗口已关闭")))
            {
                active->window.reset();
                reportFailure(name, QStringLiteral("等待窗口重新打开"));
            }
            else
                reportFailure(name, error);
        }
    };
    const auto connectWorker = [&](auto* worker) {
        using Worker = std::remove_pointer_t<decltype(worker)>;
        source.control = worker->control();
        worker->moveToThread(thread);
        connect(thread, &QThread::started, worker, &Worker::start);
        connect(worker, &Worker::stopped, thread, &QThread::quit, Qt::DirectConnection);
        connect(worker, &Worker::errorOccurred, thread, &QThread::quit, Qt::DirectConnection);
        connect(worker, &Worker::errorOccurred, this, failed, Qt::QueuedConnection);
        connect(worker, &Worker::firstFrameReady, this, [this, name, current] {
            if (!current())
                return;
            for (const auto& entry : m_layers)
                if (entry.second.device == name)
                    m_reportError(entry.first, {});
        });
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    };
    if (source.window)
    {
        auto* worker = new Oran7WindowCaptureWorker(*source.window, m_drawMouse, m_fps, preview, m_scene, source.frames);
        connectWorker(worker);
        connect(worker, &Oran7WindowCaptureWorker::temporarilyUnavailable, this, [this, name, current](const QString& message) {
            if (current())
                reportFailure(name, message);
        });
    }
    else
    {
        auto* worker = new Oran7DesktopCaptureWorker([preview](AVFrame* frame) { preview->submit(frame); },
                                                    source.outputIndex, m_drawMouse, m_fps);
        worker->setAuxiliaryOutput(m_scene, source.frames);
        connectWorker(worker);
        connect(worker, &Oran7DesktopCaptureWorker::captureInvalidated, this, failed, Qt::QueuedConnection);
    }
    connect(thread, &QThread::finished, this, &Oran7CaptureSceneSources::reapSources, Qt::QueuedConnection);
    thread->start();
}

void Oran7CaptureSceneSources::stopSource(Source& source)
{
    if (source.frames)
        source.frames->close();
    if (source.control)
        source.control->stopRequested.store(true);
    if (source.thread)
        source.thread->quit();
}

void Oran7CaptureSceneSources::reapSources()
{
    if (m_closed)
        return;
    for (auto& entry : m_sources)
    {
        auto& source = entry.second;
        if (!source.thread || !source.thread->isFinished())
            continue;
        if (!source.thread->wait(0))
        {
            m_reapTimer.start();
            continue;
        }
        source.thread.reset();
        source.frames.reset();
        source.control.reset();
    }
    reconcileSources();
}

void Oran7CaptureSceneSources::reportFailure(const QString& device, const QString& error)
{
    for (const auto& entry : m_layers)
        if (entry.second.device == device)
        {
            entry.second.item->renderBlackFrame();
            m_reportError(entry.first, error);
        }
}
