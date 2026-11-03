#pragma once
#include <QString>

// 持久化键兼容旧版本；业务使用 Oran7AppConfig 分组字段。
namespace Oran7ConfigKeys {
inline const QString SchemaVersion = QStringLiteral("schemaVersion");
inline const QString WindowWidth = QStringLiteral("window.width");
inline const QString WindowHeight = QStringLiteral("window.height");
inline const QString WindowX = QStringLiteral("window.position.x");
inline const QString WindowY = QStringLiteral("window.position.y");
inline const QString WindowMaximized = QStringLiteral("window.maximized");
inline const QString Language = QStringLiteral("appearance.language");
inline const QString UiFontFamily = QStringLiteral("appearance.fontFamily");
inline const QString UiFontPixelSize = QStringLiteral("appearance.fontPixelSize");
inline const QString ThemeProfile = QStringLiteral("appearance.themeProfile");
inline const QString Volume = QStringLiteral("playerVolume");
inline const QString RoamFilterLiked = QStringLiteral("player.roam_filter_liked");
inline const QString CaptureOutput = QStringLiteral("screen_capture.output_index");
inline const QString CaptureFps = QStringLiteral("screen_capture.fps");
inline const QString CaptureMouse = QStringLiteral("screen_capture.draw_mouse");
inline const QString CaptureDebug = QStringLiteral("screen_capture.debug_log");
inline const QString NeteaseMusicU = QStringLiteral("netease_auth.MUSIC_U");
inline const QString NeteaseCsrf = QStringLiteral("netease_auth.__csrf");
inline const QString NeteaseNickname = QStringLiteral("netease_auth.nickname");
inline const QString BilibiliSessdata = QStringLiteral("bilibili_auth.sessdata");
inline const QString BilibiliUserId = QStringLiteral("bilibili_auth.dede_user_id");
inline const QString BilibiliJct = QStringLiteral("bilibili_auth.bili_jct");
inline const QString BilibiliUserName = QStringLiteral("bilibili_auth.user_name");
inline const QString PlayOrder = QStringLiteral("local_music_sort.play_order");
inline const QString SortType = QStringLiteral("local_music_sort.sort_type");
inline const QString LastModified = QStringLiteral("local_music_sort.last_modified");
inline const QString Integrated = QStringLiteral("integratedMusic.list");
inline const QString LastPath = QStringLiteral("last_used_file");
inline const QString LastSourceVip = QStringLiteral("last_used_file_isvip");
inline const QString LastTitle = QStringLiteral("last_media_name");
inline const QString LastArtist = QStringLiteral("last_media_artist");
inline const QString LastAlbum = QStringLiteral("last_media_album");
inline const QString LastCover = QStringLiteral("last_media_cover");
inline const QString LastTimesize = QStringLiteral("last_media_timesize");
inline const QString LastSongId = QStringLiteral("last_media_songid");
inline const QString LastVip = QStringLiteral("last_media_isvip");
inline const QString RecentFiles = QStringLiteral("recent_files");
inline const QString RecentMax = QStringLiteral("recent_files_max_count");
inline const QString RecordingMode = QStringLiteral("recording.mode");
inline const QString RecordingWidth = QStringLiteral("recording.width");
inline const QString RecordingHeight = QStringLiteral("recording.height");
inline const QString RecordingFps = QStringLiteral("recording.fps");
inline const QString RecordingBitrate = QStringLiteral("recording.bitrate_kbps");
inline const QString RecordingAudio = QStringLiteral("recording.audio_enabled");
inline const QString RecordingDirectory = QStringLiteral("recording.output_directory");
inline const QString RecordingPrefix = QStringLiteral("recording.file_prefix");
inline const QString RecordingWhipUrl = QStringLiteral("recording.whip_url");
inline const QString RecordingToken = QStringLiteral("recording.bearer_token");
inline const QString RecordingRoom = QStringLiteral("recording.room_id");
inline const QString RecordingPublisher = QStringLiteral("recording.publisher_id");
inline const QString RecordingStun = QStringLiteral("recording.stun_url");
inline const QString RecordingTurn = QStringLiteral("recording.turn_url");
inline const QString RecordingTurnUser = QStringLiteral("recording.turn_user");
inline const QString RecordingTurnPassword = QStringLiteral("recording.turn_password");
inline const QString RecordingSignal = QStringLiteral("recording.signaling_url");
inline const QString RecordingSignalUser = QStringLiteral("recording.signaling_user");
inline const QString RecordingSignalPassword = QStringLiteral("recording.signaling_password");
}
