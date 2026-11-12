#ifndef ORAN7MEDIACLIENT_H
#define ORAN7MEDIACLIENT_H

#include "Oran7PlaybackState.h"
#include "Oran7TrackLibrary.h"
#include "Oran7CloudMusicService.h"
#include "Oran7PlaybackViewModel.h"
#include "Oran7VideoHistory.h"
#include <QObject>
#include <QQuickItem>
#include <QList>
#include <QVariantMap>
#include <QVariantList>
#include <memory>

class QQmlApplicationEngine;
class QQuickItem;
class QTimer;
class Oran7RenderBridge;
class Oran7LyricsService;
class Oran7LocalLibraryService;
class Oran7NetworkSongResolver;
class Oran7PlaybackController;
class Oran7ScreenCaptureBridge;
class Oran7ConfigBinding;
class Oran7TrackMetadataService;
class Oran7LegacyServerClient;
class Oran7AudioEffects;
struct ID3D11Device;

// QML 命令与通知门面；业务状态和生命周期由各服务持有。
class Oran7MediaClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Oran7PlaybackState* playback READ playback CONSTANT)
    Q_PROPERTY(Oran7TrackLibrary* tracks READ tracks CONSTANT)
    Q_PROPERTY(Oran7CloudMusicService* cloudMusic READ cloudMusic CONSTANT)
    Q_PROPERTY(Oran7PlaybackViewModel* musicQueue READ musicQueue CONSTANT)
    Q_PROPERTY(Oran7AudioEffects* audioEffects READ audioEffects CONSTANT)
    Q_PROPERTY(Oran7VideoHistory* videoHistory READ videoHistory CONSTANT)
public:
    explicit Oran7MediaClient(const QString& host, quint16 port, QObject* parent = nullptr);
    ~Oran7MediaClient() override;
    Oran7PlaybackState* playback() const { return m_playback.get(); }

    Oran7TrackLibrary* tracks() const { return m_tracks.get(); }
    Oran7CloudMusicService* cloudMusic() const { return m_cloudMusic.get(); }
    Oran7PlaybackViewModel* musicQueue() const { return m_musicQueue.get(); }
    Oran7AudioEffects* audioEffects() const { return m_audioEffects.get(); }
    Oran7VideoHistory* videoHistory() const { return m_videoHistory.get(); }

    // 应用生命周期：配置管理器须先加载，调用方负责最终落盘。
    bool restoreConfiguration(QQmlApplicationEngine& engine);
    void saveConfiguration(QQmlApplicationEngine& engine);
    void shutdown();

    // 曲目选择使用具名字段对象：path/title/artist/album/cover/timesize/songId/isVip。
    // timesize 继续沿用配置中的秒数字符串；songId 可用十进制字符串提交。
    Q_INVOKABLE void selectMusicTrack(const QVariantMap& track);
    Q_INVOKABLE void persistCurrentMusic();
    Q_INVOKABLE void playMusic(QString source);
    Q_INVOKABLE void playNetworkSong(qint64 songId, const QString& fallbackUrl, bool isVip);
    Q_INVOKABLE void playVideo(QString source);
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE int seekTo(int seconds);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void setPlayMode(int mode);
    Q_INVOKABLE bool saveAudioEffects();
    Q_INVOKABLE void playNext() { emit playNextTriggered(); }
    Q_INVOKABLE void playPrevious() { emit playPreviousTriggered(); }
    // 实际播放器目标与所选曲目路径可能不同（临时 CDN 地址、视频）。
    Q_INVOKABLE QString mediaSource() const;
    Q_INVOKABLE qint64 currentSongId() const { return m_playback->currentTrack().songId; }
    Q_INVOKABLE bool currentIsVip() const { return m_playback->currentTrack().isVip; }

    // 曲库命令；信号仅用于向外通知，不作为写入入口。
    Q_INVOKABLE void scanLocalMusic(const QString& directory);
    Q_INVOKABLE void importLocalMusic(const QVariantList& files);
    Q_INVOKABLE void removeLocalMusic(const QVariantList& files);
    Q_INVOKABLE void focusLocalMusic(QString path);

    enum class ScaleMode { Fit, Fill };
    Q_ENUM(ScaleMode)
    enum class RenderObject { VideoPlayerRender, ScreenCaptureRender };
    Q_ENUM(RenderObject)
    Q_INVOKABLE bool attachVideoItem(RenderObject key, QQuickItem* host);
    Q_INVOKABLE void setScaleMode(RenderObject key, ScaleMode mode);
    Q_INVOKABLE bool renderBlackFrame(RenderObject key);
    Q_INVOKABLE QQuickItem* videoItem(RenderObject key) const;

    // 原采集控制器句柄，保持 C++ 所有权；设置应用结果异步通知。
    Q_INVOKABLE QObject* screenCapture() const;
    Q_INVOKABLE QVariantList screenCaptureMonitors() const;
    Q_INVOKABLE QVariantMap screenCaptureSettings() const;
    Q_INVOKABLE QVariantMap applyScreenCaptureSettings(int outputIndex, int fps,
                                                       bool drawMouse, bool debugLog);
    Q_INVOKABLE bool screenCaptureRecording() const;

    // 陈旧自定义服务端兼容入口，默认未启用。
    Q_INVOKABLE void validateLogin(QString username, QString password);

signals:
    void focusedMusicRestored(const Oran7Track& track);
    void networkPlayResolveFailed(qint64 songId);
    void playerVolumeConfigLoaded(int volume);
    // 内部停止旧播放器的通知；用户停止须调用 stopPlayback() 以取消待起播请求。
    void playbackStopRequested();
    void stopIconUpdated();
    void playProgressUpdated(int position, double seconds);
    void totalDurationUpdated(int seconds);
    void playNextTriggered();
    void playPreviousTriggered();
    void lyricsAvailable(const QString& lrcFilePath);
    void lyricsUnavailable();
    void onlineLyricsSearching();
    void onlineLyricsNotFound();
    void videoRenderInfoUpdated(const QVariantMap& info);
    void screenCaptureApplyFinished(bool ok, const QString& message);
    void loginSuccess(QString username);
    void loginInputFormatValid(int errorFormatCode);

private:
    void initializeConnections();
    QString createAppDirectories();
    void preparePlayingMedia(QString source, bool musicPlayback = true);
    void publishFocusedMusic(const Oran7Track& track);
    void refetchLastMediaDetail();
    void loadScreenCaptureSettings();
    void restoreFocusedMusic();
    void restoreLocalMusicOrder();
    void restoreWindowSize(QQmlApplicationEngine& engine);
    void restoreWindowPosition(QQmlApplicationEngine& engine);
    void restoreVolume();
    void saveFocusedMusic();
    void saveLocalMusicOrder();
    void saveWindowSize(QQmlApplicationEngine& engine);
    void saveWindowPosition(QQmlApplicationEngine& engine);
    void saveVolume();
    void setD3D11Device(ID3D11Device* device);

    // 析构时统一断开服务连接。
    QList<QMetaObject::Connection> m_connections;

    // Client 独占所有服务，不叠加 QObject parent。被依赖对象先声明，
    // 保证逆序析构时 Controller/采集桥先释放，构造异常时同样成立。
    std::unique_ptr<Oran7PlaybackState> m_playback;
    std::unique_ptr<Oran7RenderBridge> m_renderBridge;
    std::unique_ptr<Oran7PlaybackController> m_playbackController;
    std::unique_ptr<Oran7ScreenCaptureBridge> m_captureBridge;

    // 独占歌词服务，析构在 .cpp 中执行；不叠加 QObject 父子所有权。
    std::unique_ptr<Oran7LyricsService> m_lyricsService;
    static bool isNetworkUrl(const QString &path);
    // 独占网络歌曲解析服务；不叠加 QObject parent。
    std::unique_ptr<Oran7NetworkSongResolver> m_networkResolver;
    void resetNetworkPlayRequest();
    void startMusicPlayback(QString filePath);
    void onPlaybackOpenFailed();

    std::unique_ptr<Oran7TrackLibrary> m_tracks;
    std::unique_ptr<Oran7VideoHistory> m_videoHistory;
    std::unique_ptr<Oran7CloudMusicService> m_cloudMusic;
    std::unique_ptr<Oran7PlaybackViewModel> m_musicQueue;
    std::unique_ptr<Oran7AudioEffects> m_audioEffects;
    std::unique_ptr<QTimer> m_audioSaveTimer;
    bool m_audioSaveReady = false;
    std::unique_ptr<Oran7LocalLibraryService> m_libraryService;
    std::unique_ptr<Oran7ConfigBinding> m_configBinding;
    std::unique_ptr<Oran7TrackMetadataService> m_metadataService;
    std::unique_ptr<Oran7LegacyServerClient> m_legacyClient;

};


#endif // ORAN7MEDIACLIENT_H
