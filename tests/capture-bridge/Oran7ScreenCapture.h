#pragma once
#include <QObject>
#include <QPointer>
#include <QVariantMap>
#include <QVariantList>
#include <memory>
#include <vector>

// 仅替换真实采集 I/O，保留 QObject/QML 调用方式、信号和所有权。
struct Oran7CaptureStats {
    int starts = 0, stops = 0, binds = 0, loads = 0, applies = 0;
    bool running = false, recording = false, destroyed = false, renderAliveAtDestruction = false;
    QPointer<QObject> item;
    QVariantMap settings{{"outputIndex", 3}, {"fps", 60}, {"drawMouse", true}, {"debugLog", false}};
};
inline std::vector<std::shared_ptr<Oran7CaptureStats>> captures;
class Oran7ScreenCaptureController : public QObject {
    Q_OBJECT
public:
    std::shared_ptr<Oran7CaptureStats> stats = std::make_shared<Oran7CaptureStats>();
    explicit Oran7ScreenCaptureController(QObject* parent = nullptr) : QObject(parent) { captures.push_back(stats); }
    ~Oran7ScreenCaptureController() override { stop(); stats->renderAliveAtDestruction = !stats->item.isNull(); stats->destroyed = true; }
    void shutdown() { stop(); }
    Q_INVOKABLE bool start() { ++stats->starts; if (!stats->item) { emit errorOccurred("未绑定渲染项"); return false; } stats->running = true; emit started(); return true; }
    Q_INVOKABLE void stop() { ++stats->stops; if (!stats->running) return; stats->running = false; stats->recording = false; emit stopped(); }
    Q_INVOKABLE bool startRecording() { stats->recording = true; emit recordingStarted(); return true; }
    Q_INVOKABLE void stopRecording() { stats->recording = false; emit recordingStopped(); }
    Q_INVOKABLE bool isRecording() const { return stats->recording; }
    Q_INVOKABLE bool recordingBusy() const { return stats->recording; }
    Q_INVOKABLE QVariantList monitors() const { return {QVariantMap{{"outputIndex", 3}, {"attached", true}}}; }
    Q_INVOKABLE QVariantMap captureSettings() const { return stats->settings; }
    Q_INVOKABLE QVariantMap applyCaptureSettings(int output, int fps, bool mouse, bool debug) {
        ++stats->applies;
        if (stats->recording) return {{"ok", false}, {"msg", "正在录制"}};
        stats->settings = {{"outputIndex", output}, {"fps", fps}, {"drawMouse", mouse}, {"debugLog", debug}};
        emit captureSettingsChanged();
        emit captureApplyFinished(true, "参数已应用");
        return {{"ok", true}, {"msg", "参数已应用"}};
    }
    void setVideoItem(QObject* item) { if (stats->item == item) return; stop(); stats->item = item; ++stats->binds; }
    void loadCaptureSettings() { ++stats->loads; emit captureSettingsChanged(); }
signals:
    void started(); void stopped(); void recordingStarted(); void recordingStopped();
    void errorOccurred(const QString& error);
    void captureApplyFinished(bool ok, const QString& message);
    void captureSettingsChanged();
};
