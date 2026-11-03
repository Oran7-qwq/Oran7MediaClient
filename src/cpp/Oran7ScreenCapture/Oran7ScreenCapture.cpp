#include "Oran7ScreenCapture.h"
#include "Oran7DesktopCaptureWorker.h"
#include "D3d11DeviceProvider.h"
#include "Oran7AppConfigManager.h"
#include "Oran7RecordingBackend.h"
#include <QMetaEnum>
#include <QHash>

#include <dxgi.h>
#include <libavutil/log.h>

namespace
{
QString ffErrStr(int err)
{
    char buf[256];
    av_strerror(err, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}
} // namespace

// ================= Controller (lives in main/GUI thread) =================
Oran7ScreenCaptureController::Oran7ScreenCaptureController(QObject* parent)
    : QObject(parent), m_recording(std::make_unique<Oran7RecordingSession>())
{
    av_log_set_level(AV_LOG_INFO);
    m_recoveryTimer.setSingleShot(true);
    m_recoveryTimer.setInterval(300);
    m_recoveryTimer.setParent(this);
    m_recoveryTimer.setObjectName(QStringLiteral("captureRecoveryDelay"));
    connect(&m_recoveryTimer, &QTimer::timeout, this,
            [this]
            {
                m_retryDelay = false;
                schedulePreview();
            });
    m_reapTimer.setSingleShot(true);
    m_reapTimer.setInterval(10);
    connect(&m_reapTimer, &QTimer::timeout, this, &Oran7ScreenCaptureController::reapWorker);
    m_startTimer.setParent(this);
    m_startTimer.setObjectName(QStringLiteral("captureFirstFrameTimeout"));
    m_startTimer.setSingleShot(true);
    m_startTimer.setInterval(5000);
    connect(&m_startTimer, &QTimer::timeout, this,
            [this]
            {
                if (!m_previewRequested || m_previewState != Oran7PreviewState::Starting)
                    return;
                const auto reason = QStringLiteral("抓屏预览等待首帧超时");
                if (m_pendingCapture)
                    rollbackCaptureSettings(reason);
                else
                    recoverCapture(reason);
            });
    connect(m_recording.get(), &Oran7RecordingSession::changed, this,
            [this]
            {
                emit recordingStateChanged();
                if (!recordingBusy())
                    schedulePreview();
            });
    connect(m_recording.get(), &Oran7RecordingSession::started, this,
            [this](const QVariantMap& details)
            {
                m_recordingOutputPath = details.value("outputPath").toString();
                const auto options = Oran7AppConfigManager::ins().snapshot().recording;
                if (options.mode == 1 && !options.signalingUrl.isEmpty())
                {
                    m_signaling = std::make_unique<Oran7SignalingClient>();
                    m_signaling->start(options.signalingUrl.toStdString(), options.signalingUser.toStdString(),
                                       options.roomId.toStdString(), options.signalingPassword.toStdString());
                    m_signaling->notifyStreamPublished(details.value("streamId").toString().toStdString());
                }
                emit recordingStateChanged();
                emit recordingStarted();
            });
    connect(m_recording.get(), &Oran7RecordingSession::finished, this,
            [this](bool ok, const QString& error)
            {
                if (m_signaling)
                    m_signaling->stop();
                if (!ok)
                    emit errorOccurred(error);
                emit recordingStopped();
            });

#ifdef _WIN32
    auto* p = D3D11DeviceProvider::instance();

    connect(p, &D3D11DeviceProvider::deviceReady, this, &Oran7ScreenCaptureController::onProviderReady,
            Qt::UniqueConnection);

    connect(p, &D3D11DeviceProvider::deviceLost, this, &Oran7ScreenCaptureController::onProviderLost,
            Qt::UniqueConnection);
#endif
}

Oran7ScreenCaptureController::~Oran7ScreenCaptureController()
{
    shutdown();
}

void Oran7ScreenCaptureController::setVideoItem(QObject* item)
{
    auto* video = qobject_cast<D3D11VideoItem*>(item);
    if (!video || m_shuttingDown)
        return;
    if (m_item == video)
        return;
    stop();
    QObject::disconnect(m_itemDestroyed);
    m_item = video;
    m_itemDestroyed = connect(video, &QObject::destroyed, this,
                              [this]
                              {
                                  if (!m_shuttingDown && (m_previewRequested || m_thread))
                                      failCapture(QStringLiteral("预览渲染项已释放"));
                              });
    video->setDebugLog(m_debugLog);
}
QString Oran7ScreenCaptureController::previewStateName() const
{
    return QString::fromLatin1(QMetaEnum::fromType<Oran7PreviewState>().valueToKey(int(m_previewState)));
}
bool Oran7ScreenCaptureController::previewBusy() const
{
    return m_previewState == Oran7PreviewState::Starting || m_previewState == Oran7PreviewState::Stopping ||
           m_previewState == Oran7PreviewState::Recovering;
}
void Oran7ScreenCaptureController::setPreviewState(Oran7PreviewState state)
{
    if (m_previewState == state)
        return;
    m_previewState = state;
    emit previewStateChanged();
}
bool Oran7ScreenCaptureController::start()
{
    if (m_shuttingDown || !m_item)
        return false;
    if (m_previewRequested)
        return true;
    m_previewRequested = true;
    m_previewError.clear();
    m_recoveryAttempts = 0;
    setPreviewState(m_thread ? Oran7PreviewState::Stopping : Oran7PreviewState::Starting);
    schedulePreview();
    return true;
}
bool Oran7ScreenCaptureController::restartPreview()
{
    if (m_shuttingDown || !m_item || recordingBusy())
        return false;
    m_previewRequested = true;
    m_previewError.clear();
    m_recoveryAttempts = 0;
    m_recoveryTimer.stop();
    m_retryDelay = false;
    teardownWorker();
    setPreviewState(m_thread ? Oran7PreviewState::Stopping : Oran7PreviewState::Starting);
    schedulePreview();
    return true;
}
void Oran7ScreenCaptureController::stop()
{
    if (m_shuttingDown)
        return;
    m_previewRequested = false;
    m_previewError.clear();
    m_restoreTarget.reset();
    m_recoveryTimer.stop();
    m_retryDelay = false;
    m_recoveryAttempts = 0;
    stopRecording();
    teardownWorker();
    cancelPending(QStringLiteral("预览已停止，参数应用已取消"));
    schedulePreview();
}
void Oran7ScreenCaptureController::shutdown()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    m_reapTimer.stop();
    m_previewRequested = false;
    m_recoveryTimer.stop();
    m_startTimer.stop();
    teardownWorker();
    cancelPending(QStringLiteral("录屏服务已关闭"));
    m_recording->shutdown();
    // 唯一允许等待的入口：退出时必须保证设备/渲染依赖覆盖所有 worker。
    if (m_thread)
    {
        m_thread->wait();
        m_thread.reset();
    }
    m_control.reset();
    m_feed.reset();
    if (m_item)
        m_item->renderBlackFrame();
    setPreviewState(Oran7PreviewState::Idle);
}

/**
 * @brief 枚举抓屏源（显示器）
 *
 * @note 必须从「抓屏 D3D11 设备所属 adapter」枚举：ddagrab 的 output_idx 就是它内部
 *       那次 EnumOutputs(i) 的 i。用 QScreen / EnumAdapters 自己另起一套坐标系，
 *       在多显卡机器上会和 ddagrab 实际看到的输出对不上。
 * @note 这里创建的 hw device 与 Oran7DesktopCaptureWorker::initGraph() 同参（adapter 传 nullptr），
 *       保证枚举的就是抓屏将要用的那块设备。
 */
// 以 GDI 设备名关联 Windows 显示路径与 DXGI 输出，不能按两套枚举的顺序对齐。
static QHash<QString, QString> captureMonitorNames()
{
    QHash<QString, QString> names;
#ifdef _WIN32
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        UINT32 pathCount = 0, modeCount = 0;
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
            break;
        QVector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        QVector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        const LONG result =
            QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
        if (result == ERROR_INSUFFICIENT_BUFFER)
            continue; // 查询期间插拔显示器，重新获取容量。
        if (result != ERROR_SUCCESS)
            break;
        for (UINT32 i = 0; i < pathCount; ++i)
        {
            const auto& path = paths[i];
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size = sizeof(source);
            source.header.adapterId = path.sourceInfo.adapterId;
            source.header.id = path.sourceInfo.id;
            DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
            target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            target.header.size = sizeof(target);
            target.header.adapterId = path.targetInfo.adapterId;
            target.header.id = path.targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
                DisplayConfigGetDeviceInfo(&target.header) != ERROR_SUCCESS)
                continue;
            const QString name = QString::fromWCharArray(target.monitorFriendlyDeviceName).trimmed();
            if (name.isEmpty())
                continue;
            const QString key = QString::fromWCharArray(source.viewGdiDeviceName).toUpper();
            // 镜像模式下一个采集源可能连接多个物理显示器，保留各个名称。
            auto list = names.value(key).split(QStringLiteral(" / "), Qt::SkipEmptyParts);
            if (!list.contains(name))
                list.push_back(name);
            names.insert(key, list.join(QStringLiteral(" / ")));
        }
        break;
    }
#endif
    return names;
}

static QVector<CaptureOutputInfo> enumerateCaptureOutputs(QString* err = nullptr)
{
    QVector<CaptureOutputInfo> list;
#ifdef _WIN32
    AVBufferRef* hwdev = nullptr;
    const int ret = av_hwdevice_ctx_create(&hwdev, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
    if (ret < 0)
    {
        if (err)
            *err = "av_hwdevice_ctx_create failed: " + ffErrStr(ret);
        return list;
    }

    auto* hw = reinterpret_cast<AVHWDeviceContext*>(hwdev->data);
    auto* d3d11 = reinterpret_cast<AVD3D11VADeviceContext*>(hw->hwctx);

    ComPtr<IDXGIDevice> dxgiDev;
    ComPtr<IDXGIAdapter> adapter;
    if (!d3d11->device || FAILED(d3d11->device->QueryInterface(IID_PPV_ARGS(&dxgiDev))) ||
        FAILED(dxgiDev->GetAdapter(&adapter)))
    {
        if (err)
            *err = "query IDXGIAdapter failed";
        av_buffer_unref(&hwdev);
        return list;
    }

    DXGI_ADAPTER_DESC adesc{};
    const QString adapterName =
        SUCCEEDED(adapter->GetDesc(&adesc)) ? QString::fromWCharArray(adesc.Description) : QString();

    const auto monitorNames = captureMonitorNames();
    for (UINT i = 0; i < 16; ++i)
    {
        ComPtr<IDXGIOutput> out;
        if (FAILED(adapter->EnumOutputs(i, &out)))
            break; // 序号不连续即到末尾

        DXGI_OUTPUT_DESC odesc{};
        if (FAILED(out->GetDesc(&odesc)))
            continue;

        CaptureOutputInfo info;
        info.outputIndex = static_cast<int>(i); // 原始序号原样透传，不做任何压缩
        info.attached = odesc.AttachedToDesktop;
        info.deviceName = QString::fromWCharArray(odesc.DeviceName);
        info.monitorName = monitorNames.value(info.deviceName.toUpper());
        info.adapterName = adapterName;
        const RECT& r = odesc.DesktopCoordinates;
        info.width = r.right - r.left;
        info.height = r.bottom - r.top;
        info.primary = (r.left == 0 && r.top == 0);
        list.push_back(info);
    }

    av_buffer_unref(&hwdev);
#else
    if (err)
        *err = "screen capture is Windows only";
#endif
    return list;
}

QVariantList Oran7ScreenCaptureController::monitors() const
{
    QVariantList out;
    QString err;
    const auto list = enumerateCaptureOutputs(&err);
    if (!err.isEmpty())
        WARNING_LOG << "[CaptureDiag] enumerateCaptureOutputs: " << err;

    for (const auto& m : list)
    {
        QVariantMap item;
        item["outputIndex"] = m.outputIndex;
        item["attached"] = m.attached;
        item["primary"] = m.primary;
        item["width"] = m.width;
        item["height"] = m.height;
        item["deviceName"] = m.deviceName;
        item["monitorName"] = m.monitorName;
        item["adapterName"] = m.adapterName;
        out.push_back(item);
    }
    return out;
}

QVariantMap Oran7ScreenCaptureController::captureSettings() const
{
    return {{"outputIndex", m_capSettings.outputIndex},
            {"fps", m_capSettings.fps},
            {"drawMouse", m_capSettings.drawMouse},
            {"debugLog", m_debugLog}};
}
bool Oran7ScreenCaptureController::recordingBusy() const
{
    return m_recording->busy();
}
void Oran7ScreenCaptureController::setDebugLog(bool on)
{
    m_debugLog = on;
    if (m_control)
        m_control->debugLog.store(on);
    if (m_item)
        m_item->setDebugLog(on);
}
Oran7ScreenCaptureController::Oran7CaptureTarget Oran7ScreenCaptureController::committedTarget() const
{
    return {m_capSettings, m_captureDeviceName, m_debugLog};
}
void Oran7ScreenCaptureController::loadCaptureSettings()
{
    if (m_shuttingDown || m_thread || m_pendingCapture)
        return;
    const auto saved = Oran7AppConfigManager::ins().snapshot().capture;
    m_capSettings = {saved.outputIndex, saved.fps, saved.drawMouse};
    m_captureDeviceName.clear();
    m_lastUsableTarget.reset();
    setDebugLog(saved.debugLog);
    emit captureSettingsChanged();
}
bool Oran7ScreenCaptureController::refreshCaptureSource(Oran7CaptureTarget& target)
{
    QVariantMap selected, fallback;
    for (const auto& value : monitors())
    {
        const auto output = value.toMap();
        if (!output.value("attached").toBool())
            continue;
        if (fallback.isEmpty())
            fallback = output;
        const bool matches = target.deviceName.isEmpty()
                                 ? output.value("outputIndex").toInt() == target.settings.outputIndex
                                 : output.value("deviceName").toString() == target.deviceName;
        if (matches)
            selected = output;
    }
    if (selected.isEmpty())
        selected = fallback;
    if (selected.isEmpty())
        return false;
    target.settings.outputIndex = selected.value("outputIndex").toInt();
    target.deviceName = selected.value("deviceName").toString();
    return true;
}
bool Oran7ScreenCaptureController::saveCaptureSettings(const Oran7CaptureTarget& target, QString* error)
{
    auto& config = Oran7AppConfigManager::ins();
    auto current = config.snapshot();
    const auto previous = current.capture;
    const auto& s = target.settings;
    const bool changed = previous.outputIndex != s.outputIndex || previous.fps != s.fps ||
                         previous.drawMouse != s.drawMouse || previous.debugLog != target.debugLog;
    if (changed)
    {
        if (!config.isWritable())
        {
            *error = config.lastError();
            return false;
        }
        current.capture = {s.outputIndex, s.fps, s.drawMouse, target.debugLog};
        config.updateConfig(current);
        if (!config.saveConfig())
        {
            *error = config.lastError();
            // 仅恢复自己的分组；不写回异步操作开始前的整份快照。
            auto latest = config.snapshot();
            latest.capture = previous;
            config.updateConfig(latest);
            return false;
        }
    }
    m_capSettings = s;
    m_captureDeviceName = target.deviceName;
    setDebugLog(target.debugLog);
    return true;
}
QVariantMap Oran7ScreenCaptureController::applyCaptureSettings(int outputIndex, int fps, bool drawMouse, bool debugLog)
{
    const auto rejected = [](const QString& message) {
        return QVariantMap{{"ok", false}, {"pending", false}, {"msg", message}};
    };
    if (m_shuttingDown)
        return rejected(QStringLiteral("录屏服务已关闭"));
    if (recordingBusy())
        return rejected(QStringLiteral("录制中不可修改抓屏参数，请先停止录制"));
    auto& config = Oran7AppConfigManager::ins();
    if (!config.isWritable())
        return rejected(config.lastError());
    fps = qBound(kCaptureFpsMin, fps, kCaptureFpsMax);
    if (fps < config.snapshot().recording.fps)
        return rejected(QStringLiteral("采集帧率不能低于已保存的录制帧率，请先降低并保存录制帧率"));
    Oran7CaptureTarget target{{outputIndex, fps, drawMouse}, {}, debugLog};
    for (const auto& value : monitors())
    {
        const auto output = value.toMap();
        if (output.value("outputIndex").toInt() == outputIndex && output.value("attached").toBool())
        {
            target.deviceName = output.value("deviceName").toString();
            break;
        }
    }
    if (target.deviceName.isEmpty())
        return rejected(QStringLiteral("抓屏源已断开或不可用，请刷新显示器列表后重新选择"));
    const quint64 id = ++m_requestCounter;
    const bool unchanged = m_capSettings.outputIndex == outputIndex && m_capSettings.fps == fps &&
                           m_capSettings.drawMouse == drawMouse && m_captureDeviceName == target.deviceName;
    if (!m_item || (unchanged && m_previewState == Oran7PreviewState::Running && !m_pendingCapture))
    {
        QString error;
        if (!saveCaptureSettings(target, &error))
            return rejected(error);
        if (m_previewState == Oran7PreviewState::Running)
            m_lastUsableTarget = target; // 热更新日志参数也属于最新已提交配置。
        cancelPending(QStringLiteral("参数已被较新的设置替代"), true);
        emit captureSettingsChanged();
        return {{"ok", true}, {"pending", false}, {"requestId", id}, {"msg", QStringLiteral("抓屏参数已保存")}};
    }
    const auto previous = m_pendingCapture;
    m_pendingCapture = Oran7CaptureRequest{id, target};
    m_restoreTarget.reset();
    m_previewRequested = true;
    m_previewError.clear();
    m_recoveryAttempts = 0;
    m_recoveryTimer.stop();
    m_retryDelay = false;
    teardownWorker();
    setPreviewState(m_thread ? Oran7PreviewState::Stopping : Oran7PreviewState::Starting);
    if (previous)
        finishRequest(previous->id, false, QStringLiteral("参数已被较新的设置替代"), true);
    schedulePreview();
    return {{"ok", true}, {"pending", true}, {"requestId", id}, {"msg", QStringLiteral("正在应用…")}};
}
void Oran7ScreenCaptureController::finishRequest(quint64 id, bool ok, const QString& message, bool superseded)
{
    emit captureRequestFinished({{"requestId", id}, {"ok", ok}, {"msg", message}, {"superseded", superseded}});
    // 兼容旧的单请求消费者；被替代请求只走带编号的终态，不结束新请求的等待。
    if (!superseded && id == m_requestCounter)
        emit captureApplyFinished(ok, message);
}
void Oran7ScreenCaptureController::cancelPending(const QString& message, bool superseded)
{
    const auto request = m_pendingCapture;
    m_pendingCapture.reset();
    if (request)
        finishRequest(request->id, false, message, superseded);
}
void Oran7ScreenCaptureController::rollbackCaptureSettings(const QString& reason)
{
    const auto request = m_pendingCapture;
    m_pendingCapture.reset();
    m_previewError = reason;
    m_restoreTarget = m_lastUsableTarget.value_or(committedTarget());
    if (m_restoreTarget->settings.fps < Oran7AppConfigManager::ins().snapshot().recording.fps)
        m_restoreTarget = committedTarget(); // 不能恢复到低于当前录制上限的历史采集值。
    m_recoveryAttempts = 0;
    m_retryDelay = false;
    m_recoveryTimer.stop();
    teardownWorker();
    schedulePreview();
    if (request)
        finishRequest(request->id, false, QStringLiteral("参数应用失败，正在恢复上次设置"));
    emit errorOccurred(reason);
}
void Oran7ScreenCaptureController::failCapture(const QString& reason)
{
    m_previewRequested = false;
    m_restoreTarget.reset();
    m_previewError = reason;
    m_retryDelay = false;
    m_recoveryTimer.stop();
    teardownWorker();
    cancelPending(reason);
    emit previewStateChanged();
    emit errorOccurred(reason);
    schedulePreview();
}
void Oran7ScreenCaptureController::recoverCapture(const QString& reason)
{
    if (!m_previewRequested || m_shuttingDown)
        return;
    m_previewError = reason;
    emit previewStateChanged();
    if (recordingBusy())
    {
        stopRecording();
        emit errorOccurred(QStringLiteral("显示模式变化，已停止录制并保存已有内容，正在恢复预览"));
    }
    teardownWorker();
    if (++m_recoveryAttempts > 8)
    {
        if (m_pendingCapture)
            rollbackCaptureSettings(reason);
        else
            failCapture(QStringLiteral("抓屏预览恢复失败，请重新开启预览：") + reason);
        return;
    }
    m_retryDelay = true;
    schedulePreview();
}
void Oran7ScreenCaptureController::onProviderReady()
{
    ComPtr<ID3D11Device> device;
    device.Attach(D3D11DeviceProvider::instance()->acquireDevice());
    if (!device)
        return;
    const bool changed = m_d3dqtDev && m_d3dqtDev.Get() != device.Get();
    m_d3dqtDev = std::move(device);
    if (changed && m_previewRequested)
        recoverCapture(QStringLiteral("预览宿主设备已变化"));
}
void Oran7ScreenCaptureController::onProviderLost()
{
    m_d3dqtDev.Reset();
    if (m_previewRequested)
        recoverCapture(QStringLiteral("预览宿主设备已丢失"));
}
void Oran7ScreenCaptureController::setD3D11Device(ID3D11Device* device)
{
    m_d3dqtDev = device;
}

bool Oran7ScreenCaptureController::startRecording()
{
    if (m_shuttingDown || recordingBusy() || m_pendingCapture || previewBusy())
        return false;
    const auto options = Oran7AppConfigManager::ins().snapshot().recording;
    if (options.fps > m_capSettings.fps)
    {
        emit errorOccurred(QStringLiteral("录制帧率不能超过采集帧率，请先应用更高的采集帧率或降低录制帧率"));
        return false;
    }
    if (m_previewState != Oran7PreviewState::Running || !m_feed)
        return false;
    const auto feed = m_feed;
    m_recordingOutputPath.clear();
    return m_recording->start(
        [options, feed](const std::atomic_bool& cancelled, const Oran7RecordingCallbacks& callbacks)
        { return Oran7RecordingBackend::run(options, feed, cancelled, callbacks); });
}
void Oran7ScreenCaptureController::stopRecording()
{
    m_recording->stop();
}
bool Oran7ScreenCaptureController::isRecording() const
{
    return m_recording->state() == Oran7RecordingSession::Oran7RecordingState::Recording;
}
QString Oran7ScreenCaptureController::recordingStateName() const
{
    return QString::fromLatin1(
        QMetaEnum::fromType<Oran7RecordingSession::Oran7RecordingState>().valueToKey(int(m_recording->state())));
}
QVariantMap Oran7ScreenCaptureController::recordingSettings() const
{
    return Oran7RecordingBackend::settings(Oran7AppConfigManager::ins().snapshot().recording);
}
QVariantMap Oran7ScreenCaptureController::applyRecordingSettings(const QVariantMap& values)
{
    if (m_shuttingDown || recordingBusy() || previewBusy())
        return {{"ok", false}, {"msg", QStringLiteral("预览切换或录制忙碌期间不可修改参数")}};
    auto& config = Oran7AppConfigManager::ins();
    if (!config.isWritable())
        return {{"ok", false}, {"msg", config.lastError()}};
    const auto previous = config.snapshot();
    auto saved = previous;
    const auto error = Oran7RecordingBackend::apply(values, &saved.recording);
    if (!error.isEmpty())
        return {{"ok", false}, {"msg", error}};
    if (saved.recording.fps > m_capSettings.fps)
        return {{"ok", false}, {"msg", QStringLiteral("录制帧率不能超过已应用的采集帧率，请先提高并应用采集帧率")}};
    config.updateConfig(saved);
    if (!config.saveConfig())
    {
        const auto failure = config.lastError();
        config.updateConfig(previous);
        return {{"ok", false}, {"msg", failure}};
    }
    emit recordingSettingsChanged();
    return {{"ok", true}, {"msg", QStringLiteral("录制参数已保存")}};
}
void Oran7ScreenCaptureController::setRecordMode(RecordMode mode)
{
    applyRecordingSettings({{"mode", int(mode)}});
}
Oran7ScreenCaptureController::RecordMode Oran7ScreenCaptureController::recordMode() const
{
    return RecordMode(Oran7AppConfigManager::ins().snapshot().recording.mode);
}

void Oran7ScreenCaptureController::schedulePreview()
{
    if (m_shuttingDown || m_driveScheduled)
        return;
    m_driveScheduled = true;
    QTimer::singleShot(0, this,
                       [this]
                       {
                           m_driveScheduled = false;
                           drivePreview();
                       });
}
void Oran7ScreenCaptureController::drivePreview()
{
    if (m_shuttingDown || m_thread)
        return;
    if (!m_previewRequested)
    {
        if (m_item)
            m_item->renderBlackFrame();
        const bool wasActive = m_previewState != Oran7PreviewState::Idle && m_previewState != Oran7PreviewState::Error;
        setPreviewState(m_previewError.isEmpty() ? Oran7PreviewState::Idle : Oran7PreviewState::Error);
        if (wasActive)
            emit stopped();
        return;
    }
    if (recordingBusy())
    {
        setPreviewState(Oran7PreviewState::Stopping);
        return;
    }
    if (m_retryDelay)
    {
        setPreviewState(Oran7PreviewState::Recovering);
        if (!m_recoveryTimer.isActive())
            m_recoveryTimer.start();
        return;
    }
    if (!m_item)
    {
        failCapture(QStringLiteral("预览渲染项已释放"));
        return;
    }
    auto target = m_pendingCapture ? m_pendingCapture->target : m_restoreTarget.value_or(committedTarget());
    if (!refreshCaptureSource(target))
    {
        recoverCapture(QStringLiteral("当前没有可用的显示输出，请连接显示器后重连预览"));
        return;
    }
    startWorker(std::move(target));
}
void Oran7ScreenCaptureController::startWorker(Oran7CaptureTarget target)
{
    m_activeTarget = std::move(target);
    m_activeRequest = m_pendingCapture ? m_pendingCapture->id : 0;
    m_thread = std::make_unique<QThread>();
    auto* worker = new Oran7DesktopCaptureWorker(m_item->frameSubmitter(), m_activeTarget.settings.outputIndex,
                                                 m_activeTarget.settings.drawMouse, m_activeTarget.settings.fps);
    worker->setDebugLog(m_activeTarget.debugLog);
    m_control = worker->control();
    m_feed = worker->recordingFeed();
    worker->moveToThread(m_thread.get());
    const quint64 gen = ++m_workerGen;
    auto* thread = m_thread.get();
    connect(thread, &QThread::started, worker, &Oran7DesktopCaptureWorker::start);
    // start 是阻塞抓取循环；不能把取消请求排到该循环后面的事件队列。
    // worker 退出/错误均让线程结束，GUI 只在 finished 后释放其句柄并驱动下一轮。
    connect(worker, &Oran7DesktopCaptureWorker::stopped, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &Oran7DesktopCaptureWorker::errorOccurred, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &Oran7DesktopCaptureWorker::firstFrameReady, this, [this, gen] { confirmPreview(gen); });
    connect(worker, &Oran7DesktopCaptureWorker::captureInvalidated, this,
            [this, gen](const QString& reason)
            {
                if (gen == m_workerGen)
                    recoverCapture(reason);
            });
    connect(worker, &Oran7DesktopCaptureWorker::errorOccurred, this,
            [this, gen](const QString& reason)
            {
                if (gen != m_workerGen)
                    return;
                if (m_recoveryAttempts > 0)
                    recoverCapture(reason);
                else if (m_pendingCapture)
                    rollbackCaptureSettings(reason);
                else
                    failCapture(reason);
            });
    connect(worker, &Oran7DesktopCaptureWorker::stopped, this,
            [this, gen]
            {
                if (gen == m_workerGen && m_previewRequested)
                    recoverCapture(QStringLiteral("抓屏预览意外结束"));
            });
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(
        thread, &QThread::finished, this,
        [this, thread]
        {
            if (m_thread.get() != thread)
                return;
            m_startTimer.stop();
            reapWorker();
        },
        Qt::QueuedConnection);
    m_startTimer.start();
    thread->start();
    setPreviewState(Oran7PreviewState::Starting);
}
void Oran7ScreenCaptureController::reapWorker()
{
    if (m_shuttingDown || !m_thread)
        return;
    // finished 发出后，延迟删除/TLS 清理仍可能正在执行。
    // wait(0) 是零超时探测，绝不等待；尚未结束则下个事件循环再检查。
    if (!m_thread->wait(0))
    {
        m_reapTimer.start();
        return;
    }
    m_thread.reset();
    m_control.reset();
    m_feed.reset();
    schedulePreview();
}
void Oran7ScreenCaptureController::teardownWorker()
{
    ++m_workerGen; // 停止后旧 started/error/首帧均失效；finished 另按线程身份回收。
    m_startTimer.stop();
    if (!m_thread)
        return;
    if (m_feed)
        m_feed->close();
    if (m_control)
        m_control->stopRequested.store(true);
    m_thread->quit();
    setPreviewState(Oran7PreviewState::Stopping);
}
void Oran7ScreenCaptureController::confirmPreview(quint64 generation)
{
    if (generation != m_workerGen || !m_previewRequested || m_previewState != Oran7PreviewState::Starting)
        return;
    m_startTimer.stop();
    QString error;
    const bool saved = saveCaptureSettings(m_activeTarget, &error);
    if (generation != m_workerGen)
        return;
    if (!saved)
    {
        if (m_pendingCapture)
            rollbackCaptureSettings(error);
        else
            failCapture(error);
        return;
    }
    const auto request = m_pendingCapture;
    if (request && request->id == m_activeRequest)
        m_pendingCapture.reset();
    m_lastUsableTarget = m_activeTarget;
    m_restoreTarget.reset();
    m_recoveryAttempts = 0; // 图创建/started 不代表恢复；只在实际产帧后清零。
    m_previewError.clear();
    setPreviewState(Oran7PreviewState::Running);
    emit captureSettingsChanged();
    if (request && request->id == m_activeRequest)
        finishRequest(request->id, true, QStringLiteral("抓屏参数已应用"));
    if (generation == m_workerGen)
        emit started();
}
