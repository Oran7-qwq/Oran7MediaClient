#include "Oran7AppConfig.h"
#include "Oran7ConfigKeys.h"
#include <QJsonArray>
#include <cmath>

namespace {
QJsonValue value(const QJsonObject& object, const QString& path) {
    const auto parts = path.split('.'); QJsonValue result(object);
    for (const auto& part : parts) result = result.toObject().value(part);
    return result;
}
void put(QJsonObject& object, const QStringList& parts, const QJsonValue& v) {
    if (parts.size() == 1) { object[parts.first()] = v; return; }
    auto child = object.value(parts.first()).toObject(); put(child, parts.mid(1), v); object[parts.first()] = child;
}
void put(QJsonObject& object, const QString& path, const QJsonValue& v) { put(object, path.split('.'), v); }
QJsonObject merge(const QJsonObject& input, const QJsonObject& defaults) {
    auto result = input;
    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
        const auto current = input.value(it.key());
        if (it->isObject()) result[it.key()] = merge(current.toObject(), it->toObject());
        else if (current.type() != it->type() || current.isNull()
                 || (current.isDouble() && (!std::isfinite(current.toDouble()) || std::floor(current.toDouble()) != current.toDouble())))
            result[it.key()] = it.value();
    }
    return result;
}
}
using namespace Oran7ConfigKeys;

bool Oran7AppConfig::normalize(const QJsonObject& input, const QJsonObject& defaults, QJsonObject* output, QString* error) {
    if (error) error->clear();
    const auto version = input.value(SchemaVersion);
    if (!version.isUndefined() && (!version.isDouble() || version.toDouble() < 0 || std::floor(version.toDouble()) != version.toDouble())) {
        if (error) *error = QStringLiteral("配置版本格式无效"); return false;
    }
    if (version.toDouble() > kVersion) { if (error) *error = QStringLiteral("配置来自较新版本，当前程序禁止覆盖"); return false; }
    auto migrated = input;
    // v0/v1 均沿用现有键布局；补齐旧版仅记录在播放源上的 VIP 标记。
    if (version.toInt() < 2 && !migrated.contains(LastVip) && migrated.value(LastSourceVip).isBool())
        migrated[LastVip] = migrated.value(LastSourceVip);
    // v2：统一默认值深合并和分组类型校验，不重命名用户键或歌曲行字段。
    auto result = merge(migrated, defaults);
    const auto range = [&](const QString& key, double min, double max) {
        const auto v = value(result, key);
        if (!v.isDouble() || v.toDouble() < min || v.toDouble() > max) put(result, key, value(defaults, key));
    };
    range(WindowWidth, 1, 32768); range(WindowHeight, 1, 32768);
    range(WindowX, -1000000, 1000000); range(WindowY, -1000000, 1000000);
    range(Volume, 0, 100); range(CaptureOutput, 0, 1000000); range(CaptureFps, 1, 240);
    range(RecentMax, 1, 50);
    range(UiFontPixelSize, 10, 24);
    if (value(result, Language).toString() != "zh_CN" && value(result, Language).toString() != "en_US")
        put(result, Language, value(defaults, Language));
    if (value(result, UiFontFamily).toString().trimmed().isEmpty())
        put(result, UiFontFamily, value(defaults, UiFontFamily));
    if (value(result, LastSongId).toInteger(-2) < -1) put(result, LastSongId, value(defaults, LastSongId));
    if (value(result, ThemeProfile).toString().trimmed().isEmpty()) put(result, ThemeProfile, value(defaults, ThemeProfile));
    range(RecordingMode, 0, 1); range(RecordingWidth, 0, 16384); range(RecordingHeight, 0, 16384);
    if (value(result, RecordingFormat).toString() != "mp4" && value(result, RecordingFormat).toString() != "flv")
        put(result, RecordingFormat, value(defaults, RecordingFormat));
    range(RecordingFps, 1, 240); range(RecordingBitrate, 100, 200000);
    const int rw = value(result, RecordingWidth).toInt(), rh = value(result, RecordingHeight).toInt();
    if ((rw == 0) != (rh == 0) || rw % 2 || rh % 2) {
        put(result, RecordingWidth, value(defaults, RecordingWidth));
        put(result, RecordingHeight, value(defaults, RecordingHeight));
    }
    range(AudioVersion, 1, 2);
    range(AudioPreamp, -24.0, 24.0);
    range(AudioOutput, -24.0, 24.0);
    range(AudioCeiling, -24.0, 0.0);
    range(AudioHighPass, 0.0, 1000.0);
    range(AudioLowPass, 1000.0, 24000.0);
    range(AudioBassBoost, 0.0, 12.0);
    range(AudioBassFreq, 40.0, 300.0);
    range(AudioStereoWidth, 0.0, 2.0);
    range(AudioCrossfeed, 0.0, 3.0);
    range(AudioCompThreshold, -60.0, 0.0);
    range(AudioCompRatio, 1.0, 20.0);
    range(AudioCompAttack, 0.1, 100.0);
    range(AudioCompRelease, 10.0, 1000.0);
    range(AudioCompKnee, 0.0, 12.0);
    range(AudioCompMakeup, -12.0, 12.0);
    if (value(result, AudioHighPass).toDouble() >= value(result, AudioLowPass).toDouble()) {
        put(result, AudioHighPass, value(defaults, AudioHighPass));
        put(result, AudioLowPass, value(defaults, AudioLowPass));
    }
    put(result, AudioVersion, 2);
    const auto audioBands = value(result, AudioBands).toArray();
    bool audioBandsValid = audioBands.size() == 10;
    for (const auto& band : audioBands) {
        if (!band.isDouble() || !std::isfinite(band.toDouble())
            || band.toDouble() < -12.0 || band.toDouble() > 12.0) {
            audioBandsValid = false;
            break;
        }
    }
    if (!audioBandsValid) put(result, AudioBands, value(defaults, AudioBands));
    if (value(result, AudioPreset).toString().trimmed().isEmpty())
        put(result, AudioPreset, value(defaults, AudioPreset));
    QJsonArray recent;
    for (const auto& v : result.value(RecentFiles).toArray()) if (v.isString()) recent.append(v);
    result[RecentFiles] = recent;
    result[SchemaVersion] = kVersion;
    if (output) *output = result;
    return true;
}

Oran7AppConfig Oran7AppConfig::fromJson(const QJsonObject& json) {
    Oran7AppConfig result; result.m_extensions = json;
    result.window.width = value(json, WindowWidth).toVariant().toInt();
    result.window.height = value(json, WindowHeight).toVariant().toInt();
    result.window.x = value(json, WindowX).toVariant().toInt();
    result.window.y = value(json, WindowY).toVariant().toInt();
    result.window.maximized = value(json, WindowMaximized).toVariant().toBool();
    result.appearance.themeProfile = value(json, ThemeProfile).toVariant().toString();
    result.appearance.language = value(json, Language).toString();
    result.general.developerMode = value(json, DeveloperMode).toBool();
    result.appearance.fontFamily = value(json, UiFontFamily).toString();
    result.appearance.fontPixelSize = value(json, UiFontPixelSize).toInt();
    result.player.volume = value(json, Volume).toVariant().toInt();
    result.player.roamFilterLiked = value(json, RoamFilterLiked).toBool();
    result.capture.outputIndex = value(json, CaptureOutput).toVariant().toInt();
    result.capture.fps = value(json, CaptureFps).toVariant().toInt();
    result.capture.drawMouse = value(json, CaptureMouse).toVariant().toBool();
    result.capture.debugLog = value(json, CaptureDebug).toVariant().toBool();
    result.auth.netease.musicU = value(json, NeteaseMusicU).toVariant().toString();
    result.auth.netease.csrf = value(json, NeteaseCsrf).toVariant().toString();
    result.auth.netease.nickname = value(json, NeteaseNickname).toVariant().toString();
    result.auth.bilibili.sessdata = value(json, BilibiliSessdata).toVariant().toString();
    result.auth.bilibili.dedeUserId = value(json, BilibiliUserId).toVariant().toString();
    result.auth.bilibili.biliJct = value(json, BilibiliJct).toVariant().toString();
    result.auth.bilibili.userName = value(json, BilibiliUserName).toVariant().toString();
    result.library.playOrder = value(json, PlayOrder).toVariant().toList();
    result.library.integrated = value(json, Integrated).toVariant().toList();
    result.library.sortType = value(json, SortType).toVariant().toString();
    result.library.lastModified = value(json, LastModified).toVariant().toString();
    result.lastMedia.path = value(json, LastPath).toVariant().toString();
    result.lastMedia.title = value(json, LastTitle).toVariant().toString();
    result.lastMedia.artist = value(json, LastArtist).toVariant().toString();
    result.lastMedia.album = value(json, LastAlbum).toVariant().toString();
    result.lastMedia.cover = value(json, LastCover).toVariant().toString();
    result.lastMedia.timesize = value(json, LastTimesize).toVariant().toString();
    result.lastMedia.songId = value(json, LastSongId).toVariant().toLongLong();
    result.lastMedia.isVip = value(json, LastVip).toVariant().toBool();
    result.lastMedia.sourceIsVip = value(json, LastSourceVip).toVariant().toBool();
    result.recentFiles = value(json, RecentFiles).toVariant().toStringList();
    result.videoHistory = value(json, QStringLiteral("video.history")).toArray().toVariantList();
    result.recentFilesMaxCount = value(json, RecentMax).toVariant().toInt();
    result.recording.mode = value(json, RecordingMode).toVariant().toInt();
    result.recording.format = value(json, RecordingFormat).toString();
    result.recording.width = value(json, RecordingWidth).toVariant().toInt();
    result.recording.height = value(json, RecordingHeight).toVariant().toInt();
    result.recording.fps = value(json, RecordingFps).toVariant().toInt();
    result.recording.bitrateKbps = value(json, RecordingBitrate).toVariant().toInt();
    result.recording.audioEnabled = value(json, RecordingAudio).toVariant().toBool();
    result.recording.outputDirectory = value(json, RecordingDirectory).toVariant().toString();
    result.recording.filePrefix = value(json, RecordingPrefix).toVariant().toString();
    result.recording.whipUrl = value(json, RecordingWhipUrl).toVariant().toString();
    result.recording.bearerToken = value(json, RecordingToken).toVariant().toString();
    result.recording.roomId = value(json, RecordingRoom).toVariant().toString();
    result.recording.publisherId = value(json, RecordingPublisher).toVariant().toString();
    result.recording.stunUrl = value(json, RecordingStun).toVariant().toString();
    result.recording.turnUrl = value(json, RecordingTurn).toVariant().toString();
    result.recording.turnUser = value(json, RecordingTurnUser).toVariant().toString();
    result.recording.turnPassword = value(json, RecordingTurnPassword).toVariant().toString();
    result.recording.signalingUrl = value(json, RecordingSignal).toVariant().toString();
    result.recording.signalingUser = value(json, RecordingSignalUser).toVariant().toString();
    result.recording.signalingPassword = value(json, RecordingSignalPassword).toVariant().toString();
    result.audio.version = value(json, AudioVersion).toInt(1);
    result.audio.enabled = value(json, AudioEnabled).toBool();
    result.audio.presetId = value(json, AudioPreset).toString();
    result.audio.preampDb = value(json, AudioPreamp).toDouble();
    result.audio.outputGainDb = value(json, AudioOutput).toDouble();
    result.audio.limiterCeilingDb = value(json, AudioCeiling).toDouble(-1.0);
    const auto bands = value(json, AudioBands).toArray();
    for (int i = 0; i < 10; ++i)
        result.audio.bandGainsDb.append(bands.at(i).toDouble(0.0));
    for (const auto& item : value(json, AudioCustom).toArray())
        result.audio.customPresets.append(item.toVariant());
    result.audio.highPassHz = value(json, AudioHighPass).toDouble(20.0);
    result.audio.lowPassHz = value(json, AudioLowPass).toDouble(20000.0);
    result.audio.bassBoostDb = value(json, AudioBassBoost).toDouble();
    result.audio.bassFreqHz = value(json, AudioBassFreq).toDouble(120.0);
    result.audio.stereoWidth = value(json, AudioStereoWidth).toDouble(1.0);
    result.audio.crossfeed = value(json, AudioCrossfeed).toInt();
    result.audio.compressorEnabled = value(json, AudioCompressor).toBool();
    result.audio.compressorThresholdDb = value(json, AudioCompThreshold).toDouble(-18.0);
    result.audio.compressorRatio = value(json, AudioCompRatio).toDouble(3.0);
    result.audio.compressorAttackMs = value(json, AudioCompAttack).toDouble(10.0);
    result.audio.compressorReleaseMs = value(json, AudioCompRelease).toDouble(150.0);
    result.audio.compressorKneeDb = value(json, AudioCompKnee).toDouble(3.0);
    result.audio.compressorMakeupDb = value(json, AudioCompMakeup).toDouble();
    return result;
}

QJsonObject Oran7AppConfig::toJson() const {
    auto result = m_extensions;
    put(result, WindowWidth, QJsonValue::fromVariant(QVariant::fromValue(window.width)));
    put(result, WindowHeight, QJsonValue::fromVariant(QVariant::fromValue(window.height)));
    put(result, WindowX, QJsonValue::fromVariant(QVariant::fromValue(window.x)));
    put(result, WindowY, QJsonValue::fromVariant(QVariant::fromValue(window.y)));
    put(result, WindowMaximized, QJsonValue::fromVariant(QVariant::fromValue(window.maximized)));
    put(result, Language, appearance.language);
    put(result, DeveloperMode, general.developerMode);
    put(result, UiFontFamily, appearance.fontFamily);
    put(result, UiFontPixelSize, appearance.fontPixelSize);
    put(result, ThemeProfile, QJsonValue::fromVariant(QVariant::fromValue(appearance.themeProfile)));
    put(result, Volume, QJsonValue::fromVariant(QVariant::fromValue(player.volume)));
    put(result, RoamFilterLiked, player.roamFilterLiked);
    put(result, CaptureOutput, QJsonValue::fromVariant(QVariant::fromValue(capture.outputIndex)));
    put(result, CaptureFps, QJsonValue::fromVariant(QVariant::fromValue(capture.fps)));
    put(result, CaptureMouse, QJsonValue::fromVariant(QVariant::fromValue(capture.drawMouse)));
    put(result, CaptureDebug, QJsonValue::fromVariant(QVariant::fromValue(capture.debugLog)));
    put(result, NeteaseMusicU, QJsonValue::fromVariant(QVariant::fromValue(auth.netease.musicU)));
    put(result, NeteaseCsrf, QJsonValue::fromVariant(QVariant::fromValue(auth.netease.csrf)));
    put(result, NeteaseNickname, QJsonValue::fromVariant(QVariant::fromValue(auth.netease.nickname)));
    put(result, BilibiliSessdata, QJsonValue::fromVariant(QVariant::fromValue(auth.bilibili.sessdata)));
    put(result, BilibiliUserId, QJsonValue::fromVariant(QVariant::fromValue(auth.bilibili.dedeUserId)));
    put(result, BilibiliJct, QJsonValue::fromVariant(QVariant::fromValue(auth.bilibili.biliJct)));
    put(result, BilibiliUserName, QJsonValue::fromVariant(QVariant::fromValue(auth.bilibili.userName)));
    put(result, PlayOrder, QJsonValue::fromVariant(QVariant::fromValue(library.playOrder)));
    put(result, Integrated, QJsonValue::fromVariant(QVariant::fromValue(library.integrated)));
    put(result, SortType, QJsonValue::fromVariant(QVariant::fromValue(library.sortType)));
    put(result, LastModified, QJsonValue::fromVariant(QVariant::fromValue(library.lastModified)));
    put(result, LastPath, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.path)));
    put(result, LastTitle, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.title)));
    put(result, LastArtist, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.artist)));
    put(result, LastAlbum, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.album)));
    put(result, LastCover, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.cover)));
    put(result, LastTimesize, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.timesize)));
    put(result, LastSongId, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.songId)));
    put(result, LastVip, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.isVip)));
    put(result, LastSourceVip, QJsonValue::fromVariant(QVariant::fromValue(lastMedia.sourceIsVip)));
    put(result, RecentFiles, QJsonValue::fromVariant(QVariant::fromValue(recentFiles)));
    put(result, QStringLiteral("video.history"), QJsonArray::fromVariantList(videoHistory));
    put(result, RecentMax, QJsonValue::fromVariant(QVariant::fromValue(recentFilesMaxCount)));
    put(result, RecordingMode, QJsonValue::fromVariant(QVariant::fromValue(recording.mode)));
    put(result, RecordingFormat, recording.format);
    put(result, RecordingWidth, QJsonValue::fromVariant(QVariant::fromValue(recording.width)));
    put(result, RecordingHeight, QJsonValue::fromVariant(QVariant::fromValue(recording.height)));
    put(result, RecordingFps, QJsonValue::fromVariant(QVariant::fromValue(recording.fps)));
    put(result, RecordingBitrate, QJsonValue::fromVariant(QVariant::fromValue(recording.bitrateKbps)));
    put(result, RecordingAudio, QJsonValue::fromVariant(QVariant::fromValue(recording.audioEnabled)));
    put(result, RecordingDirectory, QJsonValue::fromVariant(QVariant::fromValue(recording.outputDirectory)));
    put(result, RecordingPrefix, QJsonValue::fromVariant(QVariant::fromValue(recording.filePrefix)));
    put(result, RecordingWhipUrl, QJsonValue::fromVariant(QVariant::fromValue(recording.whipUrl)));
    put(result, RecordingToken, QJsonValue::fromVariant(QVariant::fromValue(recording.bearerToken)));
    put(result, RecordingRoom, QJsonValue::fromVariant(QVariant::fromValue(recording.roomId)));
    put(result, RecordingPublisher, QJsonValue::fromVariant(QVariant::fromValue(recording.publisherId)));
    put(result, RecordingStun, QJsonValue::fromVariant(QVariant::fromValue(recording.stunUrl)));
    put(result, RecordingTurn, QJsonValue::fromVariant(QVariant::fromValue(recording.turnUrl)));
    put(result, RecordingTurnUser, QJsonValue::fromVariant(QVariant::fromValue(recording.turnUser)));
    put(result, RecordingTurnPassword, QJsonValue::fromVariant(QVariant::fromValue(recording.turnPassword)));
    put(result, RecordingSignal, QJsonValue::fromVariant(QVariant::fromValue(recording.signalingUrl)));
    put(result, RecordingSignalUser, QJsonValue::fromVariant(QVariant::fromValue(recording.signalingUser)));
    put(result, RecordingSignalPassword, QJsonValue::fromVariant(QVariant::fromValue(recording.signalingPassword)));
    put(result, AudioVersion, QJsonValue::fromVariant(QVariant::fromValue(audio.version)));
    put(result, AudioEnabled, QJsonValue::fromVariant(QVariant::fromValue(audio.enabled)));
    put(result, AudioPreset, QJsonValue::fromVariant(QVariant::fromValue(audio.presetId)));
    put(result, AudioPreamp, QJsonValue::fromVariant(QVariant::fromValue(audio.preampDb)));
    put(result, AudioOutput, QJsonValue::fromVariant(QVariant::fromValue(audio.outputGainDb)));
    put(result, AudioCeiling, QJsonValue::fromVariant(QVariant::fromValue(audio.limiterCeilingDb)));
    QJsonArray audioBands;
    for (double gain : audio.bandGainsDb) audioBands.append(gain);
    put(result, AudioBands, audioBands);
    QJsonArray customPresets;
    for (const auto& item : audio.customPresets)
        customPresets.append(QJsonValue::fromVariant(item));
    put(result, AudioCustom, customPresets);
    put(result, AudioHighPass, QJsonValue::fromVariant(QVariant::fromValue(audio.highPassHz)));
    put(result, AudioLowPass, QJsonValue::fromVariant(QVariant::fromValue(audio.lowPassHz)));
    put(result, AudioBassBoost, QJsonValue::fromVariant(QVariant::fromValue(audio.bassBoostDb)));
    put(result, AudioBassFreq, QJsonValue::fromVariant(QVariant::fromValue(audio.bassFreqHz)));
    put(result, AudioStereoWidth, QJsonValue::fromVariant(QVariant::fromValue(audio.stereoWidth)));
    put(result, AudioCrossfeed, QJsonValue::fromVariant(QVariant::fromValue(audio.crossfeed)));
    put(result, AudioCompressor, QJsonValue::fromVariant(QVariant::fromValue(audio.compressorEnabled)));
    put(result, AudioCompThreshold, QJsonValue::fromVariant(QVariant::fromValue(audio.compressorThresholdDb)));
    put(result, AudioCompRatio, QJsonValue::fromVariant(QVariant::fromValue(audio.compressorRatio)));
    put(result, AudioCompAttack, QJsonValue::fromVariant(QVariant::fromValue(audio.compressorAttackMs)));
    put(result, AudioCompRelease, QJsonValue::fromVariant(QVariant::fromValue(audio.compressorReleaseMs)));
    put(result, AudioCompKnee, QJsonValue::fromVariant(QVariant::fromValue(audio.compressorKneeDb)));
    put(result, AudioCompMakeup, QJsonValue::fromVariant(QVariant::fromValue(audio.compressorMakeupDb)));
    result[SchemaVersion] = kVersion;
    return result;
}
