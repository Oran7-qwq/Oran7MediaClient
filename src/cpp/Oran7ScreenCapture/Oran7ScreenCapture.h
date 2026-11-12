#ifndef ORAN7SCREENCAPTURE_H
#define ORAN7SCREENCAPTURE_H
#pragma once

#include <QObject>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <memory>
#include <optional>
#include <d3d11.h>
#include <wrl/client.h>
#include "D3d11Videoitem.h"
#include "Oran7SignalingClient.h"
#include "Oran7RecordingSession.h"
using Microsoft::WRL::ComPtr;
struct Oran7CaptureControl;
class Oran7RecordingFeed;

//  =====================  CaptureOutputInfo  ==================

/**
 * @brief 抓屏源（显示器）描述
 * @note outputIndex 就是 ddagrab 的 output_idx：抓屏 D3D11 设备所属 DXGI adapter 上
 *       IDXGIAdapter::EnumOutputs 的「原始序号」。必须原样透传 —— 过滤掉
 *       !AttachedToDesktop 的输出会让后续所有序号整体前移、抓错屏。
 */
struct CaptureOutputInfo
{
    int outputIndex = 0;   // ddagrab 的 output_idx
    bool attached = false; // DXGI_OUTPUT_DESC.AttachedToDesktop
    bool primary = false;  // 桌面坐标原点落在该输出内
    int width = 0;         // DesktopCoordinates 宽（仅用于界面显示）
    int height = 0;
    QString deviceName;  // "\\.\DISPLAY1"
    QString monitorName; // Windows 友好名称，例如 X7IQ；不可用时为空。
    QString adapterName; // 所属 GPU 描述
};

/**
 * @brief 抓屏参数：改动这三项必须重启抓屏 worker 才能生效
 * @note debugLog 不在其中 —— 它可以热更新，见 setDebugLog()
 */
struct CaptureSettings
{
    int outputIndex = 0; // ddagrab output_idx
    int fps = 60;        // ddagrab 源头帧率（必须 >= 1）
    bool drawMouse = true;
};

/*! @note ddagrab 的 framerate 是 AV_OPT_TYPE_VIDEO_RATE，0 直接解析失败 */
static constexpr int kCaptureFpsMin = 1;
/*! @note ddagrab 自身不设上限，这里只是避免高刷屏上白烧 GPU（超出部分全是补帧） */
static constexpr int kCaptureFpsMax = 240;

//  =====================  Oran7ScreenCaptureController  ==================

class Oran7ScreenCaptureController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString previewState READ previewStateName NOTIFY previewStateChanged)
    Q_PROPERTY(bool previewBusy READ previewBusy NOTIFY previewStateChanged)
    Q_PROPERTY(QString previewError READ previewError NOTIFY previewStateChanged)
    Q_PROPERTY(QString recordingState READ recordingStateName NOTIFY recordingStateChanged)
    Q_PROPERTY(bool recordingBusy READ recordingBusy NOTIFY recordingStateChanged)
    Q_PROPERTY(QString recordingError READ recordingError NOTIFY recordingStateChanged)
    Q_PROPERTY(QVariantMap recordingStatistics READ recordingStatistics NOTIFY recordingStateChanged)
    Q_PROPERTY(QString recordingOutputPath READ recordingOutputPath NOTIFY recordingStateChanged)
    Q_PROPERTY(QVariantMap recordingOptions READ recordingSettings NOTIFY recordingSettingsChanged)
  public:
    explicit Oran7ScreenCaptureController(QObject* parent = nullptr);
    ~Oran7ScreenCaptureController() override;

    enum class Oran7PreviewState
    {
        Idle,
        Starting,
        Running,
        Stopping,
        Recovering,
        Error
    };
    Q_ENUM(Oran7PreviewState)
    QString previewStateName() const;
    bool previewBusy() const;
    QString previewError() const { return m_previewError; }
    void shutdown(); // 仅退出/析构允许同步 join，普通 stop 只提交请求。

    /*! @note 启停抓屏（返回仅代表请求接受，完成见状态/stopped） */
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    /*! @note 预览失败后可主动重连；录制期间禁止重建采集链路。 */
    Q_INVOKABLE bool restartPreview();

    /*! @note 开关抓屏调试日志（submitFrame fps / DUP 检测等），可热更新、无需重启 */
    Q_INVOKABLE void setDebugLog(bool on);
    Q_INVOKABLE bool debugLog() const { return m_debugLog; }

    /*! @note 枚举抓屏源（显示器），返回 QVariantMap 列表，key 见 CaptureOutputInfo */
    Q_INVOKABLE QVariantList monitors() const;

    /*! @note 读取当前生效的抓屏参数 */
    Q_INVOKABLE QVariantMap captureSettings() const;

    /**
     * @brief 应用抓屏参数（QML 端唯一的写入入口）
     * @note 校验后排队应用；预览首帧成功后保存，失败保留已提交配置并恢复。
     *       重启是异步的，最终结果通过 captureApplyFinished 给出。
     * @return {ok, pending, msg, requestId}；连续提交只保留最新请求。
     */
    Q_INVOKABLE QVariantMap applyCaptureSettings(int outputIndex, int fps, bool drawMouse, bool debugLog);

    /*! @note 录制（含 WHIP 推流）进行中？此时禁止改抓屏参数 */
    Q_INVOKABLE bool recordingBusy() const;

    /*! @note 从 config.json 载入抓屏参数（由 Oran7MediaClient::restoreConfiguration 调用） */
    void loadCaptureSettings();

    // ========================= Render ==========================//
    ///*! @note 设置qml端QtQuickItem 渲染对象*/
    void setVideoItem(QObject* item);
    ///*! @note 绑定qml端QtQuickItem 渲染对象*/
    const QObject* bindVideoItem() const { return m_item; }

    void setD3D11Device(ID3D11Device* dev); //<-- Discard 现已改为在initGraph自动创建，非QtSceneGraphic的设置

    //========================= DataStreamCatch ===================//

    /** 录制输出模式 */
    enum class RecordMode
    {
        MuxerToMp4 = 0, // 本地 MP4 / FLV 文件；保留旧枚举名与数值兼容。
        WhipPush = 1,   // WHIP/WebRTC 推流到媒体服务器
    };
    Q_ENUM(RecordMode)

    Q_INVOKABLE bool startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE bool isRecording() const;

    /** 设置录制输出模式（必须在 startRecording 之前调用） */
    Q_INVOKABLE void setRecordMode(RecordMode mode);
    Q_INVOKABLE RecordMode recordMode() const;
    Q_INVOKABLE QVariantMap recordingSettings() const;
    Q_INVOKABLE QVariantMap applyRecordingSettings(const QVariantMap& values);
    QString recordingStateName() const;
    QString recordingError() const { return m_recording->error(); }
    QVariantMap recordingStatistics() const { return m_recording->statistics(); }
    QString recordingOutputPath() const { return m_recordingOutputPath; }

  signals:
    void previewStateChanged();
    void captureRequestFinished(const QVariantMap& result);
    void recordingSettingsChanged();
    void started();
    void stopped();
    void errorOccurred(const QString& err);
    void recordingStateChanged();
    void recordingStarted();
    void recordingStopped();

    /*! @note 抓屏参数已变更（含 apply 成功后的确认） */
    void captureSettingsChanged();
    /*! @note applyCaptureSettings 的最终结果：重启是异步的，成功/回滚都走这里 */
    void captureApplyFinished(bool ok, const QString& msg);

  private slots:
    void onProviderReady(); //<-- Discard
    void onProviderLost();  //<-- Discard

  private:
    struct Oran7CaptureTarget
    {
        CaptureSettings settings;
        QString deviceName;
        bool debugLog = false;
    };
    struct Oran7CaptureRequest
    {
        quint64 id;
        Oran7CaptureTarget target;
    };
    Oran7CaptureTarget committedTarget() const;
    bool refreshCaptureSource(Oran7CaptureTarget& target);
    bool saveCaptureSettings(const Oran7CaptureTarget& target, QString* error);
    void schedulePreview();
    void drivePreview();
    void startWorker(Oran7CaptureTarget target);
    void reapWorker();
    void teardownWorker(); // 非阻塞：仅取消/quit，finished 后才能重建。
    void confirmPreview(quint64 generation);
    void recoverCapture(const QString& reason);
    void failCapture(const QString& reason);
    void rollbackCaptureSettings(const QString& reason);
    void finishRequest(quint64 id, bool ok, const QString& message, bool superseded = false);
    void cancelPending(const QString& message, bool superseded = false);
    void setPreviewState(Oran7PreviewState state);

    CaptureSettings m_capSettings;
    QString m_captureDeviceName;
    bool m_debugLog = true;
    std::optional<Oran7CaptureRequest> m_pendingCapture;
    std::optional<Oran7CaptureTarget> m_restoreTarget, m_lastUsableTarget;
    Oran7CaptureTarget m_activeTarget;
    quint64 m_requestCounter = 0, m_activeRequest = 0, m_workerGen = 0;
    Oran7PreviewState m_previewState = Oran7PreviewState::Idle;
    QString m_previewError;
    bool m_previewRequested = false, m_shuttingDown = false, m_driveScheduled = false;
    bool m_retryDelay = false;
    QTimer m_recoveryTimer, m_startTimer, m_reapTimer;
    int m_recoveryAttempts = 0;
    QPointer<D3D11VideoItem> m_item;
    QMetaObject::Connection m_itemDestroyed;
    ComPtr<ID3D11Device> m_d3dqtDev; // Qt 宿主设备观察；采集设备由 worker 独占。
    std::unique_ptr<QThread> m_thread;
    std::shared_ptr<Oran7CaptureControl> m_control;
    std::shared_ptr<Oran7RecordingFeed> m_feed;
    //=================== CaptureRecording ===================//
    std::unique_ptr<Oran7RecordingSession> m_recording;
    std::unique_ptr<Oran7SignalingClient> m_signaling;
    QString m_recordingOutputPath;
};

#endif // ORAN7SCREENCAPTURE_H
