#include "Oran7RecordingBackend.h"
#include "Oran7WasapiAudioWorker.h"
#include "Oran7Mp4Muxer.h"
#include "Oran7WhipPublisher.h"
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDir>
#include <QRegularExpression>
#include <QUrl>
#include <cmath>
#include <limits>

namespace {
struct Oran7RecordingError {
    std::mutex mutex;
    QString message;
    void set(const QString& value) { std::lock_guard<std::mutex> lock(mutex); if (message.isEmpty()) message = value; }
    QString get() { std::lock_guard<std::mutex> lock(mutex); return message; }
};
// 任何准备失败、取消或异常都按相同顺序关入口、停生产者、flush 编码器并结束容器。
struct Oran7RecordingResources {
    std::shared_ptr<Oran7RecordingFeed> feed;
    std::shared_ptr<Oran7VideoEncoder> video = std::make_shared<Oran7VideoEncoder>();
    std::shared_ptr<Oran7AudioEncoder> audio;
    std::unique_ptr<Oran7WasapiAudioWorker> capture;
    std::unique_ptr<Oran7Mp4Muxer> muxer;
    std::unique_ptr<Oran7WhipPublisher> publisher;
    std::thread bridge;
    std::atomic_bool bridgeRunning{false};
    std::function<void()> onClosing;
    bool closed = false;
    ~Oran7RecordingResources() { close(); }
    void close() {
        if (closed) return;
        closed = true;
        if (onClosing) onClosing();
        feed->detach();
        bridgeRunning.store(false);
        if (bridge.joinable()) bridge.join();
        if (capture) capture->stop();
        if (audio) audio->stop();
        video->stop();
        if (muxer) muxer->close();
        if (publisher) publisher->close();
    }
};
bool waitOpened(const std::atomic_bool& cancel, const std::function<bool()>& opened,
                const std::function<bool()>& running) {
    QElapsedTimer timer; timer.start();
    while (!cancel.load() && timer.elapsed() < 3000) {
        if (opened()) return true;
        if (!running()) return false;
        QThread::msleep(10);
    }
    return false;
}
}

QString Oran7RecordingBackend::validate(const Oran7RecordingConfig& c, bool starting) {
    if (c.mode < 0 || c.mode > 1) return QStringLiteral("请选择有效的录制模式");
    if (c.format != "mp4" && c.format != "flv") return QStringLiteral("录制格式须为 MP4 或 FLV");
    if (c.fps < 1 || c.fps > 240 || c.bitrateKbps < 100 || c.bitrateKbps > 200000)
        return QStringLiteral("帧率须为 1～240，码率须为 100～200000 kbps");
    if ((c.width == 0) != (c.height == 0) || c.width < 0 || c.height < 0 || c.width > 16384 || c.height > 16384
        || c.width % 2 || c.height % 2 || (c.width > 0 && (c.width < 16 || c.height < 16)))
        return QStringLiteral("分辨率须为至少 16 的偶数；宽高同时为 0 时使用原始尺寸");
    if (c.filePrefix.trimmed().isEmpty() || c.filePrefix.contains(QRegularExpression("[<>:\"/\\\\|?*\\x00-\\x1f]")))
        return QStringLiteral("文件名前缀不能为空或包含路径及特殊字符");
    if (c.bearerToken.contains('\r') || c.bearerToken.contains('\n')) return QStringLiteral("推流凭据不能包含换行");
    if (!c.outputDirectory.isEmpty() && !QDir::isAbsolutePath(c.outputDirectory)) return QStringLiteral("输出目录须为绝对路径");
    const auto validUrl = [](const QString& text, bool websocket) {
        const QUrl url(text);
        return url.isValid() && !url.host().isEmpty() && url.userInfo().isEmpty()
            && (websocket ? (url.scheme() == "ws" || url.scheme() == "wss") : (url.scheme() == "http" || url.scheme() == "https"));
    };
    if ((!c.whipUrl.isEmpty() || (starting && c.mode == 1)) && !validUrl(c.whipUrl, false))
        return QStringLiteral("WHIP 地址须为完整的 HTTP/HTTPS 接口地址");
    if (!c.signalingUrl.isEmpty() && !validUrl(c.signalingUrl, true)) return QStringLiteral("信令地址须为 WS/WSS 地址");
    return {};
}

QString Oran7RecordingBackend::run(const Oran7RecordingConfig& c, std::shared_ptr<Oran7RecordingFeed> feed,
                                  const std::atomic_bool& cancelled, const Oran7RecordingCallbacks& cb) {
    if (const auto error = validate(c, true); !error.isEmpty()) return error;
    if (!feed) return QStringLiteral("采集源不可用");
    QElapsedTimer timer; timer.start();
    auto source = feed->snapshot();
    while (source.alive && (!source.width || !source.height || !source.device) && !cancelled.load() && timer.elapsed() < 5000) {
        QThread::msleep(10); source = feed->snapshot();
    }
    if (cancelled.load()) return {};
    if (!source.alive || !source.width || !source.height || !source.device) return QStringLiteral("采集首帧尚未就绪或预览已经停止");
    const int width = c.width ? c.width : source.width - source.width % 2;
    const int height = c.height ? c.height : source.height - source.height % 2;
    if (width < 16 || height < 16) return QStringLiteral("采集画面尺寸无效");
    const auto errors = std::make_shared<Oran7RecordingError>();
    const ErrorCallback onError = [errors](const std::string&) { errors->set(QStringLiteral("编码或输出失败，请检查设备与录制参数")); };
    QTemporaryFile output;
    Oran7RecordingResources resources; resources.feed = feed; resources.onClosing = cb.stopping;
    auto& r = resources;
    Oran7VideoEncoder::EncoderConfig video;
    video.width = width; video.height = height; video.fps = c.fps; video.gopSize = c.fps;
    video.bitrateKbps = c.bitrateKbps; video.codecName = "h264_mf"; video.hwDevice = source.device.get();
    video.liveStreaming = c.mode == 1;
    if (!r.video->configure(video, onError) || !r.video->start()
        || !waitOpened(cancelled, [&] { return r.video->isOpened(); }, [&] { return r.video->isRuning(); }))
        return cancelled.load() ? QString() : QStringLiteral("视频编码器准备失败或超时");
    if (cancelled.load()) return {};
    if (c.audioEnabled) {
        Oran7AudioEncoder::EncoderConfig audio;
        audio.codecName = c.mode == 1 ? "opus" : (avcodec_find_encoder_by_name("libfdk_aac") ? "libfdk_aac" : "aac");
        audio.sampleRate = 48000; audio.channels = 2; audio.bitrateBps = 128000;
        r.audio = std::make_shared<Oran7AudioEncoder>();
        if (!r.audio->configure(audio, onError) || !r.audio->start()
            || !waitOpened(cancelled, [&] { return r.audio->isOpened(); }, [&] { return r.audio->isRunning(); }))
            return cancelled.load() ? QString() : QStringLiteral("音频编码器准备失败或超时");
    }
    if (cancelled.load()) return {};
    QString outputPath, streamId;
    if (c.mode == 1) {
        Oran7WhipPublisher::Config config;
        config.serverUrl = c.whipUrl.toStdString(); config.bearerToken = c.bearerToken.toStdString();
        config.roomId = c.roomId.toStdString(); config.publisherId = c.publisherId.toStdString();
        config.stunUrl = c.stunUrl.toStdString(); config.turnUrl = c.turnUrl.toStdString();
        config.turnUser = c.turnUser.toStdString(); config.turnPass = c.turnPassword.toStdString();
        config.videoBitrateKbps = c.bitrateKbps;
        r.publisher = std::make_unique<Oran7WhipPublisher>(); r.publisher->setErrorCallback(onError);
        if (!r.publisher->open(config, r.video, r.audio)) return cancelled.load() ? QString() : QStringLiteral("WHIP 会话建立失败");
        timer.restart();
        while (!cancelled.load() && r.publisher->connectionState() != "connected" && timer.elapsed() < 10000) {
            const auto state = r.publisher->connectionState();
            if (state == "failed" || state == "closed") return QStringLiteral("WHIP 连接准备失败");
            QThread::msleep(20);
        }
        if (cancelled.load()) return {};
        if (r.publisher->connectionState() != "connected") return QStringLiteral("WHIP 连接准备超时");
        streamId = QString::fromStdString(r.publisher->streamId());
    } else {
        const auto directory = c.outputDirectory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) : c.outputDirectory;
        if (directory.isEmpty() || !QDir().mkpath(directory)) return QStringLiteral("无法创建录制输出目录");
        output.setFileTemplate(QDir(directory).filePath(c.filePrefix + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + "-XXXXXX." + c.format));
        if (!output.open()) return QStringLiteral("无法创建录制文件");
        outputPath = output.fileName(); output.close();
        r.muxer = std::make_unique<Oran7Mp4Muxer>(); r.muxer->setErrorCallback(onError);
        if (!r.muxer->open(outputPath.toUtf8().constData(), width, height, AVRational{1,c.fps}, r.video, r.audio, c.format.toUtf8().constData()))
            return QStringLiteral("无法打开录制输出文件");
    }
    if (cancelled.load()) return {};
    if (c.audioEnabled) {
        r.capture = std::make_unique<Oran7WasapiAudioWorker>();
        if (!r.capture->start({48000, 2, 10})) return QStringLiteral("系统音频采集启动失败");
    }
    if (!feed->attach(r.video, width, height, c.fps)) return QStringLiteral("采集源已经停止");
    if (r.capture) {
        r.bridgeRunning.store(true);
        r.bridge = std::thread([&r] {
            while (r.bridgeRunning.load()) {
                AVFrame* frame = nullptr;
                if (r.capture->getPcmFrame(&frame, 50)) r.audio->pushFrame(frame);
            }
        });
    }
    output.setAutoRemove(false);
    cb.started({{"outputPath",outputPath},{"streamId",streamId}});
    qint64 lastBytes = 0; timer.restart();
    while (!cancelled.load()) {
        if (!feed->snapshot().alive) return QStringLiteral("采集源已停止");
        if (!errors->get().isEmpty()) return errors->get();
        if (!r.video->isRuning() || (r.audio && (!r.audio->isRunning() || !r.capture->isRunning())))
            return QStringLiteral("编码或系统音频采集已意外停止");
        if (timer.elapsed() >= 500) {
            QVariantMap stats{{"outputPath",outputPath},{"width",width},{"height",height}};
            if (r.publisher) {
                const auto state = QString::fromStdString(r.publisher->connectionState());
                const auto ice = QString::fromStdString(r.publisher->iceState());
                if (state == "failed" || state == "closed" || ice == "failed") return QStringLiteral("WHIP 连接已失败或关闭");
                const qint64 bytes = r.publisher->videoBytesSent() + r.publisher->audioBytesSent();
                stats.insert("state",state); stats.insert("iceState",ice);
                stats.insert("bitrateKbps", (bytes - lastBytes) * 8 / qMax(qint64(1), timer.elapsed()));
                stats.insert("framesSent", r.publisher->videoFramesSent()); lastBytes = bytes;
            }
            cb.statistics(stats); timer.restart();
        }
        QThread::msleep(20);
    }
    resources.close();
    return errors->get();
}

QVariantMap Oran7RecordingBackend::settings(const Oran7RecordingConfig& c) {
    return {{"format",c.format},{"mode",c.mode},{"width",c.width},{"height",c.height},{"fps",c.fps},{"bitrateKbps",c.bitrateKbps},{"audioEnabled",c.audioEnabled},{"outputDirectory",c.outputDirectory},{"filePrefix",c.filePrefix},{"whipUrl",c.whipUrl},{"bearerToken",c.bearerToken},{"roomId",c.roomId},{"publisherId",c.publisherId},{"stunUrl",c.stunUrl},{"turnUrl",c.turnUrl},{"turnUser",c.turnUser},{"turnPassword",c.turnPassword},{"signalingUrl",c.signalingUrl},{"signalingUser",c.signalingUser},{"signalingPassword",c.signalingPassword}};
}

QString Oran7RecordingBackend::apply(const QVariantMap& values, Oran7RecordingConfig* config) {
    if (values.contains("format") && values.value("format").metaType().id() != QMetaType::QString)
        return QStringLiteral("录制格式须为 MP4 或 FLV");
    auto next = *config;
    if (values.contains("format")) next.format = values.value("format").toString();
    if (values.contains("mode")) { const auto v = values.value("mode"); if ((v.metaType().id() != QMetaType::Int && v.metaType().id() != QMetaType::LongLong && v.metaType().id() != QMetaType::Double) || !std::isfinite(v.toDouble()) || std::floor(v.toDouble()) != v.toDouble() || v.toDouble() < std::numeric_limits<int>::min() || v.toDouble() > std::numeric_limits<int>::max()) return QStringLiteral("录制参数类型错误：mode"); next.mode = v.toInt(); }
    if (values.contains("width")) { const auto v = values.value("width"); if ((v.metaType().id() != QMetaType::Int && v.metaType().id() != QMetaType::LongLong && v.metaType().id() != QMetaType::Double) || !std::isfinite(v.toDouble()) || std::floor(v.toDouble()) != v.toDouble() || v.toDouble() < std::numeric_limits<int>::min() || v.toDouble() > std::numeric_limits<int>::max()) return QStringLiteral("录制参数类型错误：width"); next.width = v.toInt(); }
    if (values.contains("height")) { const auto v = values.value("height"); if ((v.metaType().id() != QMetaType::Int && v.metaType().id() != QMetaType::LongLong && v.metaType().id() != QMetaType::Double) || !std::isfinite(v.toDouble()) || std::floor(v.toDouble()) != v.toDouble() || v.toDouble() < std::numeric_limits<int>::min() || v.toDouble() > std::numeric_limits<int>::max()) return QStringLiteral("录制参数类型错误：height"); next.height = v.toInt(); }
    if (values.contains("fps")) { const auto v = values.value("fps"); if ((v.metaType().id() != QMetaType::Int && v.metaType().id() != QMetaType::LongLong && v.metaType().id() != QMetaType::Double) || !std::isfinite(v.toDouble()) || std::floor(v.toDouble()) != v.toDouble() || v.toDouble() < std::numeric_limits<int>::min() || v.toDouble() > std::numeric_limits<int>::max()) return QStringLiteral("录制参数类型错误：fps"); next.fps = v.toInt(); }
    if (values.contains("bitrateKbps")) { const auto v = values.value("bitrateKbps"); if ((v.metaType().id() != QMetaType::Int && v.metaType().id() != QMetaType::LongLong && v.metaType().id() != QMetaType::Double) || !std::isfinite(v.toDouble()) || std::floor(v.toDouble()) != v.toDouble() || v.toDouble() < std::numeric_limits<int>::min() || v.toDouble() > std::numeric_limits<int>::max()) return QStringLiteral("录制参数类型错误：bitrateKbps"); next.bitrateKbps = v.toInt(); }
    if (values.contains("audioEnabled")) { const auto v = values.value("audioEnabled"); if (v.metaType().id() != QMetaType::Bool) return QStringLiteral("录制参数类型错误：audioEnabled"); next.audioEnabled = v.toBool(); }
    if (values.contains("outputDirectory")) { const auto v = values.value("outputDirectory"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：outputDirectory"); next.outputDirectory = v.toString(); }
    if (values.contains("filePrefix")) { const auto v = values.value("filePrefix"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：filePrefix"); next.filePrefix = v.toString(); }
    if (values.contains("whipUrl")) { const auto v = values.value("whipUrl"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：whipUrl"); next.whipUrl = v.toString(); }
    if (values.contains("bearerToken")) { const auto v = values.value("bearerToken"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：bearerToken"); next.bearerToken = v.toString(); }
    if (values.contains("roomId")) { const auto v = values.value("roomId"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：roomId"); next.roomId = v.toString(); }
    if (values.contains("publisherId")) { const auto v = values.value("publisherId"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：publisherId"); next.publisherId = v.toString(); }
    if (values.contains("stunUrl")) { const auto v = values.value("stunUrl"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：stunUrl"); next.stunUrl = v.toString(); }
    if (values.contains("turnUrl")) { const auto v = values.value("turnUrl"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：turnUrl"); next.turnUrl = v.toString(); }
    if (values.contains("turnUser")) { const auto v = values.value("turnUser"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：turnUser"); next.turnUser = v.toString(); }
    if (values.contains("turnPassword")) { const auto v = values.value("turnPassword"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：turnPassword"); next.turnPassword = v.toString(); }
    if (values.contains("signalingUrl")) { const auto v = values.value("signalingUrl"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：signalingUrl"); next.signalingUrl = v.toString(); }
    if (values.contains("signalingUser")) { const auto v = values.value("signalingUser"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：signalingUser"); next.signalingUser = v.toString(); }
    if (values.contains("signalingPassword")) { const auto v = values.value("signalingPassword"); if (v.metaType().id() != QMetaType::QString) return QStringLiteral("录制参数类型错误：signalingPassword"); next.signalingPassword = v.toString(); }
    const auto error = validate(next, false);
    if (error.isEmpty()) *config = next;
    return error;
}
