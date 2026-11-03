#include "Oran7LegacyServerClient.h"
#include "Oran7TrackMetadataService.h"
#include "Oran7ConfigBinding.h"
#include "Oran7MediaClient.h"
#include "Oran7RenderBridge.h"
#include "Oran7LyricsService.h"
#include "Oran7LocalLibraryService.h"
#include "Oran7MediaInfoReader.h"
#include "Oran7NetworkSongResolver.h"
#include "Oran7PlaybackController.h"
#include "Oran7ScreenCaptureBridge.h"

#include "Oran7GlobalHelper.h"
#include "Oran7AppConfigManager.h"
#include "NcmSession.h"
#include "NcmPlaylistApi.h"
#include "NcmAsyncHandlers.h"
#include <QTimer>

#include <functional>
#include <utility>

#include <QList>

#include <QQuickItem>
#include <QQmlEngine>

namespace {
bool sameMusicIdentity(const Oran7Track& first, const Oran7Track& second)
{
    if (first.songId > 0 || second.songId > 0)
        return first.songId > 0 && first.songId == second.songId;
    return first.path == second.path;
}

} // namespace

Oran7MediaClient::Oran7MediaClient(const QString &host,quint16 port,QObject *parent)
    : QObject{parent}
{
    m_configBinding = std::make_unique<Oran7ConfigBinding>(Oran7AppConfigManager::ins());
    m_tracks = std::make_unique<Oran7TrackLibrary>();
    QQmlEngine::setObjectOwnership(m_tracks.get(), QQmlEngine::CppOwnership);
    m_metadataService = std::make_unique<Oran7TrackMetadataService>();
    m_legacyClient = std::make_unique<Oran7LegacyServerClient>(host, port);
    // Client 保留 QML 状态入口，播放生命周期交由 Controller。
    m_playback = std::make_unique<Oran7PlaybackState>();
    m_renderBridge = std::make_unique<Oran7RenderBridge>();
    // QML 仅借用 playback 指针，生命周期始终由 Client 的 unique_ptr 管理。
    QQmlEngine::setObjectOwnership(m_playback.get(), QQmlEngine::CppOwnership);
    m_libraryService = std::make_unique<Oran7LocalLibraryService>();
    m_lyricsService = std::make_unique<Oran7LyricsService>(&Oran7MediaInfoReader::analyzeMediaFileInfo);
    m_networkResolver = std::make_unique<Oran7NetworkSongResolver>(NcmSession::instance().songApi());
    // 渲染钩子只在 GUI 线程调用；播放器线程拿到的是渲染桥的共享帧邮箱。
    auto* renderBridge = m_renderBridge.get();
    m_playbackController = std::make_unique<Oran7PlaybackController>(*m_playback,
        Oran7PlaybackController::Oran7RenderHooks{
            [renderBridge] { return renderBridge->beginPlayback(); },
            [renderBridge] { renderBridge->stopPlayback(); },
            [renderBridge] { return renderBridge->device(); }});
    // 桥先创建并连接控制器信号，之后 Client 只连接桥，不再管理采集对象。
    m_captureBridge = std::make_unique<Oran7ScreenCaptureBridge>(
        Oran7ScreenCaptureBridge::Oran7RenderHooks{
            [renderBridge](QQuickItem* host) {
                return renderBridge->attachVideoItem(Oran7RenderBridge::RenderObject::ScreenCaptureRender, host);
            },
            [renderBridge]() -> QObject* {
                return renderBridge->getVideoItem(Oran7RenderBridge::RenderObject::ScreenCaptureRender);
            }});
    m_cloudMusic = std::make_unique<Oran7CloudMusicService>(*m_tracks,
        Oran7CloudMusicService::Oran7Hooks{
            [] { auto& session = NcmSession::instance();
                return QVariantMap{{"isLoggedIn", session.isLoggedIn()},
                    {"cookie", session.cookieString()}, {"csrf", session.csrfToken()}}; },
            [](const QString& operation, const QVariantMap& args) {
                return operation == "fetchLikedSongs" ? ncm::fetchLikedSongsHandler(args) : ncm::roamSongsHandler(args); },
            [](qint64 id) { NcmSession::instance().playlistApi()->setLikedPlaylistId(id); },
            [](qint64 id, bool liked) { NcmSession::instance().playlistApi()->setLike(id, liked); }});
    m_musicQueue = std::make_unique<Oran7PlaybackViewModel>(*m_tracks, *m_playback, *m_cloudMusic);
    QQmlEngine::setObjectOwnership(m_cloudMusic.get(), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(m_musicQueue.get(), QQmlEngine::CppOwnership);
    connect(m_musicQueue.get(), &Oran7PlaybackViewModel::roamFilterLikedChanged, this, [this] {
        if (!m_configBinding->saveRoamFilterLiked(m_musicQueue->roamFilterLiked()))
            WARNING_LOG << "漫游过滤设置保存失败";
    });
    connect(m_musicQueue.get(), &Oran7PlaybackViewModel::trackRequested, this,
        [this](const QVariantMap& track, bool restart) {
            if (m_playbackController->isShuttingDown()) return;
            const auto command = m_musicQueue->commandRevision();
            if (restart) { resetNetworkPlayRequest(); m_playbackController->stop(); }// 队列推进/重播必须重新打开；直接点击当前曲目仍保持原来的暂停/继续行为。
            if (command != m_musicQueue->commandRevision()) return;
            selectMusicTrack(track);
            if (command != m_musicQueue->commandRevision()) return;
            const auto selected = m_playback->currentTrack();
            playNetworkSong(selected.songId, selected.path, selected.isVip);
        });
    connect(m_musicQueue.get(), &Oran7PlaybackViewModel::stopRequested, this, &Oran7MediaClient::stopPlayback);
    connect(this, &Oran7MediaClient::playNextTriggered, m_musicQueue.get(), [this] { m_musicQueue->advance(1); });
    connect(this, &Oran7MediaClient::playPreviousTriggered, m_musicQueue.get(), [this] { m_musicQueue->advance(-1); });
    connect(this, &Oran7MediaClient::networkPlayResolveFailed, m_musicQueue.get(), &Oran7PlaybackViewModel::resolveFailed);
    connect(&NcmSession::instance(), &NcmSession::loginStatusChanged,
        m_cloudMusic.get(), &Oran7CloudMusicService::syncSession, Qt::QueuedConnection);
    connect(NcmSession::instance().playlistApi(), &NcmPlaylistApi::likeResult, m_cloudMusic.get(),
        [this](quint64, qint64, bool, bool ok, const QString&) {
            if (!ok) { m_cloudMusic->markLikedDirty(); m_cloudMusic->fetchLiked(true); }
        });
    connect(NcmSession::instance().playlistApi(), &NcmPlaylistApi::reorderResult, m_cloudMusic.get(),
        [this](quint64, bool ok, const QString&) {
            if (!ok) { m_cloudMusic->markLikedDirty(); m_cloudMusic->fetchLiked(true); }
        });
    QTimer::singleShot(0, m_cloudMusic.get(), &Oran7CloudMusicService::syncSession);
    // 连接各服务的命令与通知。
    initializeConnections();
}

Oran7MediaClient::~Oran7MediaClient()
{
    shutdown();
    m_musicQueue.reset();
    m_cloudMusic.reset();
    // 先失效消息邮箱并释放播放器，再销毁渲染桥/状态和其他服务。
    m_playbackController.reset();
    m_networkResolver.reset();
    m_lyricsService->stop();
    m_captureBridge.reset();
    m_legacyClient.reset();
    for (auto& connection : m_connections) QObject::disconnect(connection);
}

void Oran7MediaClient::shutdown()
{
    m_musicQueue->shutdown();
    m_cloudMusic->shutdown();
    m_metadataService->cancel();
    m_libraryService->shutdown();
    resetNetworkPlayRequest();
    m_captureBridge->shutdown();
    m_playbackController->shutdown();
}

QString Oran7MediaClient::mediaSource() const
{
    return m_playbackController->source();
}

void Oran7MediaClient::initializeConnections()
{
    m_connections << connect(m_legacyClient.get(), &Oran7LegacyServerClient::loginSuccess, this, &Oran7MediaClient::loginSuccess);
    m_connections << connect(m_legacyClient.get(), &Oran7LegacyServerClient::loginInputFormatValid, this, &Oran7MediaClient::loginInputFormatValid);
    m_connections << connect(m_legacyClient.get(), &Oran7LegacyServerClient::integratedMusicElementReady, this, [this](const QString& cover, const QString& title, const QString& artist,
            const QString& album, const QString& timesize, const QString& musicId) {
            // 陈旧服务端的 ID 原来作为列表播放入口使用，不能解释成网易云 songId。
            m_tracks->addIntegratedTrack({{"url",musicId},{"title",title},{"artist",artist},
                {"album",album},{"cover",cover},{"timesize",timesize}});
        });
    m_connections << connect(m_metadataService.get(), &Oran7TrackMetadataService::detailsReady, this,
        [this](qint64 id, quint64 revision, const QString& album, const QString& timesize) {
            if (m_playbackController->isShuttingDown() || currentSongId() != id) return;
            if (!m_playback->completeTrackDetails(revision, album, timesize)) return;
            if (currentSongId() != id || m_playback->trackRevision() > revision + 1) return;
            const auto track = m_playback->currentTrack();
            m_configBinding->completePersistedDetails(track);
            emit focusedMusicRestored(track);
        });

    // Resolver 只输出值结果，播放状态和播放器命令仍由 Client 负责。
    m_connections << connect(m_networkResolver.get(), &Oran7NetworkSongResolver::identityResolved,
        this, [this](quint64 generation, qint64 songId, bool isVip) {
            if (m_playbackController->isShuttingDown() || !m_networkResolver->isCurrent(generation)) return;
            auto track = m_playback->currentTrack();
            track.songId = songId;
            track.isVip = isVip;
            m_playback->setCurrentTrack(track);
        });
    m_connections << connect(m_networkResolver.get(), &Oran7NetworkSongResolver::urlReady,
        this, [this](quint64 generation, const QString& url) {
            if (!m_playbackController->isShuttingDown() && m_networkResolver->isCurrent(generation)) startMusicPlayback(url);
        });
    m_connections << connect(m_networkResolver.get(), &Oran7NetworkSongResolver::failed,
        this, [this](quint64 generation, qint64 songId, const QString& error) {
            if (m_playbackController->isShuttingDown() || !m_networkResolver->isCurrent(generation)) return;
            if (!error.isEmpty()) m_playback->setError(error);
            // 状态通知可能同步触发下一首；不能再把旧请求的失败转发给新歌。
            if (!m_playbackController->isShuttingDown() && m_networkResolver->isCurrent(generation))
                emit networkPlayResolveFailed(songId);
        });

    m_connections << connect(m_lyricsService.get(), &Oran7LyricsService::lyricsAvailable,
        this, &Oran7MediaClient::lyricsAvailable);
    m_connections << connect(m_lyricsService.get(), &Oran7LyricsService::lyricsUnavailable,
        this, &Oran7MediaClient::lyricsUnavailable);
    m_connections << connect(m_lyricsService.get(), &Oran7LyricsService::onlineLyricsSearching,
        this, &Oran7MediaClient::onlineLyricsSearching);
    m_connections << connect(m_lyricsService.get(), &Oran7LyricsService::onlineLyricsNotFound,
        this, &Oran7MediaClient::onlineLyricsNotFound);

    // Controller 内部请求直接接回控制器；Client 信号仅向外通知。
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::playOrPauseRequested,
        m_playbackController.get(), &Oran7PlaybackController::toggle);
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::stopRequested,
        m_playbackController.get(), &Oran7PlaybackController::stopCurrent);
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::stopRequested,
        this, &Oran7MediaClient::playbackStopRequested);
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::mediaAccepted,
        this, [this](const QString& source, bool musicPlayback) {
            if (musicPlayback) m_lyricsService->load(source, m_configBinding->lyricsDirectory(), m_playback->currentTrack());
            else m_lyricsService->stop();
        });
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::stopped,
        this, [this](bool switching) { if (!switching) m_lyricsService->stop(); });
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::stopIconUpdated,
        this, &Oran7MediaClient::stopIconUpdated);
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::playNextTriggered,
        this, &Oran7MediaClient::playNextTriggered);
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::openFailed,
        this, &Oran7MediaClient::onPlaybackOpenFailed);
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::playProgressUpdated,
        this, &Oran7MediaClient::playProgressUpdated);
    m_connections << connect(m_playbackController.get(), &Oran7PlaybackController::totalDurationUpdated,
        this, &Oran7MediaClient::totalDurationUpdated);

    m_connections << connect(m_libraryService.get(), &Oran7LocalLibraryService::tracksReady,
        m_tracks.get(), &Oran7TrackLibrary::replaceLocalTracks);
    m_connections << connect(m_tracks.get(), &Oran7TrackLibrary::integratedMusicChanged, this,
        [this](const QVariantList& tracks) { m_configBinding->saveIntegratedMusic(tracks); });
    m_connections << connect(m_tracks->local(), &Oran7TrackListModel::contentsChanged, this,
        [this] { m_libraryService->setPlayOrder(m_tracks->local()->paths()); });
    m_connections << connect(m_libraryService.get(), &Oran7LocalLibraryService::operationFailed,
        this, [](const QString& error) { WARNING_LOG << error; });

    m_connections << connect(m_renderBridge.get(), &Oran7RenderBridge::deviceChanged,
        this, [this] { setD3D11Device(m_renderBridge->device()); });
    m_connections << connect(m_renderBridge.get(), &Oran7RenderBridge::videoRenderInfoUpdated,
        this, &Oran7MediaClient::videoRenderInfoUpdated);
    m_connections << connect(m_captureBridge.get(), &Oran7ScreenCaptureBridge::errorOccurred,
        this, [](const QString& error) { ERROR_LOG << error; });
    m_connections << connect(m_captureBridge.get(), &Oran7ScreenCaptureBridge::captureApplyFinished,
        this, &Oran7MediaClient::screenCaptureApplyFinished);
}

//=========================<录屏桥 QML 兼容门面>=========================//
QObject* Oran7MediaClient::screenCapture() const
{
    return m_captureBridge->controller();
}

QVariantList Oran7MediaClient::screenCaptureMonitors() const
{
    return m_captureBridge->monitors();
}

QVariantMap Oran7MediaClient::screenCaptureSettings() const
{
    return m_captureBridge->settings();
}

QVariantMap Oran7MediaClient::applyScreenCaptureSettings(int outputIndex, int fps,
                                                       bool drawMouse, bool debugLog)
{
    return m_captureBridge->applySettings(outputIndex, fps, drawMouse, debugLog);
}

bool Oran7MediaClient::screenCaptureRecording() const
{
    return m_captureBridge->recordingBusy();
}

void Oran7MediaClient::loadScreenCaptureSettings()
{
    m_captureBridge->loadSettings();
}

QString Oran7MediaClient::createAppDirectories()
{
    return m_configBinding->createDirectories();
}

void Oran7MediaClient::saveFocusedMusic()
{
    m_configBinding->saveLastFocused(currentIsVip());
}

void Oran7MediaClient::restoreFocusedMusic()
{
    const auto restored = m_configBinding->restoredTrack();
    if (!restored) return;
    const auto& track = *restored;
    publishFocusedMusic(track);
    if ((track.songId > 0 || isNetworkUrl(track.path)) && (track.album.isEmpty() || track.timesize.isEmpty()))
        refetchLastMediaDetail();
}

// 持久化网络歌元数据缺专辑/时长时,按 songId 重新拉取并补发 focusedMusicRestored
void Oran7MediaClient::refetchLastMediaDetail()
{
    m_metadataService->fetch(currentSongId(), m_playback->trackRevision());
}

void Oran7MediaClient::playMusic(QString source)
{
    if (source.isEmpty()) return;
    resetNetworkPlayRequest();
    startMusicPlayback(source);
}

void Oran7MediaClient::startMusicPlayback(QString source)
{
    if (source.isEmpty()) return;
    m_configBinding->rememberMusicSource(source, currentIsVip());
    preparePlayingMedia(source);
}

void Oran7MediaClient::playVideo(QString source)
{
    if (source.isEmpty()) return;
    m_musicQueue->cancel(true);
    resetNetworkPlayRequest();
    preparePlayingMedia(source, false);
}

void Oran7MediaClient::preparePlayingMedia(QString source, bool musicPlayback)
{
    m_playbackController->prepare(std::move(source), musicPlayback);
}

void Oran7MediaClient::validateLogin(QString username, QString password)
{
    m_legacyClient->validateLogin(std::move(username), std::move(password));
}

void Oran7MediaClient::stopPlayback()
{
    m_musicQueue->cancel();
    resetNetworkPlayRequest();
    m_playbackController->stop();
}

bool Oran7MediaClient::isNetworkUrl(const QString &path)
{
    return Oran7NetworkSongResolver::isNetworkUrl(path);
}

int Oran7MediaClient::seekTo(int seconds)
{
    return m_playbackController->seekTo(seconds);
}

void Oran7MediaClient::setPlayMode(int mode)
{
    m_playback->setMode(mode);
    INFO_LOG << "Oran7MediaClient::setPlayMode:" << mode;
}

void Oran7MediaClient::setVolume(int volume)
{
    m_playbackController->setVolume(volume);
}

bool Oran7MediaClient::attachVideoItem(RenderObject key, QQuickItem* host)
{
    if (key == RenderObject::ScreenCaptureRender) return m_captureBridge->attachVideoItem(host);
    return m_renderBridge->attachVideoItem(static_cast<Oran7RenderBridge::RenderObject>(key), host);
}

void Oran7MediaClient::setScaleMode(RenderObject key, ScaleMode mode)
{
    m_renderBridge->setScaleMode(static_cast<Oran7RenderBridge::RenderObject>(key), static_cast<Oran7RenderBridge::ScaleMode>(mode));
}

bool Oran7MediaClient::renderBlackFrame(RenderObject key)
{
    return m_renderBridge->renderBlackFrame(static_cast<Oran7RenderBridge::RenderObject>(key));
}

QQuickItem* Oran7MediaClient::videoItem(RenderObject key) const
{
    return m_renderBridge->getVideoItem(static_cast<Oran7RenderBridge::RenderObject>(key));
}

void Oran7MediaClient::setD3D11Device(ID3D11Device* device)
{
    m_playbackController->setDevice(device);
}

void Oran7MediaClient::importLocalMusic(const QVariantList& files)
{
    m_libraryService->importFiles(m_configBinding->audioDirectory(), files);
}

void Oran7MediaClient::removeLocalMusic(const QVariantList& files)
{
    if (files.isEmpty()) return;
    const QString playingPath = m_playbackController->playingSource();
    const auto key = [](const QString& path) {
        return Oran7LocalLibraryService::normalizedPath(path).toCaseFolded();
    };
    bool needStop = false;
    for (const auto& file : files) {
        const auto path = key(file.toString());
        if (!path.isEmpty() && (path == key(playingPath) || path == key(m_playback->source()))) {
            needStop = true;
            break;
        }
    }
    if (needStop) {
        stopPlayback();
    }
    // 等所有退役播放器释放文件句柄后再提交删除，避免 Windows 文件仍被占用。
    auto remove = [this, directory = m_configBinding->audioDirectory(), files] {
        m_libraryService->removeFiles(directory, files);
    };
    m_playbackController->whenPlayersReleased(std::move(remove));
}

void Oran7MediaClient::scanLocalMusic(const QString& directory)
{
    m_libraryService->scan(directory);
}

void Oran7MediaClient::focusLocalMusic(QString filepath)
{
    m_playbackController->setFocusedSource(filepath);
    m_configBinding->setFocusedMusicSource(filepath);
    if (isNetworkUrl(filepath)) return;
    const auto selected = m_configBinding->localTrack(filepath);
    if (!selected) return;
    const auto& track = *selected;
    publishFocusedMusic(track);
}

void Oran7MediaClient::restoreLocalMusicOrder()
{
    m_libraryService->setPlayOrder(m_configBinding->localOrder());
}

void Oran7MediaClient::restoreWindowSize(QQmlApplicationEngine &engine)
{
    m_configBinding->loadWindowSize(engine);
}

void Oran7MediaClient::restoreWindowPosition(QQmlApplicationEngine &engine)
{
    m_configBinding->loadWindowPosition(engine);
}

void Oran7MediaClient::restoreVolume()
{
    int value = m_configBinding->volume();
    setVolume(value);
    if (value < 0 || value > 100) return;
    emit playerVolumeConfigLoaded(value);
}

void Oran7MediaClient::saveLocalMusicOrder()
{
    m_configBinding->saveLocalOrder(m_libraryService->playOrder());
}

void Oran7MediaClient::selectMusicTrack(const QVariantMap& fields)
{
    const auto selection = Oran7Track::fromVariantMap(fields);
    if (!selection || (selection->path.isEmpty() && selection->songId == 0)) {
        WARNING_LOG << "无效的曲目选择：需要有效 path 或 songId";
        return;
    }
    // VIP 地址解析可能尚未完成；选择新歌时就停止旧歌词。
    if (!sameMusicIdentity(m_playback->currentTrack(), *selection)) m_lyricsService->stop();
    m_playback->selectTrack(*selection);
    persistCurrentMusic();
}

void Oran7MediaClient::persistCurrentMusic()
{
    const auto track = m_playback->currentTrack();
    if (track.songId > 0 || isNetworkUrl(track.path)) m_configBinding->rememberTrack(track);
}

void Oran7MediaClient::publishFocusedMusic(const Oran7Track& track)
{
    if (!sameMusicIdentity(m_playback->currentTrack(), track)) m_lyricsService->stop();
    m_playback->setCurrentTrack(track);
    emit focusedMusicRestored(track);
}

void Oran7MediaClient::resetNetworkPlayRequest()
{
    m_networkResolver->cancel();
}

void Oran7MediaClient::playNetworkSong(qint64 songId, const QString& fallbackUrl, bool isVip)
{
    if (m_playbackController->isShuttingDown()) return;
    const bool hasPlayer = m_playbackController->hasPlayer();
    const QString source = m_playbackController->source();
    // QML 已选中目标歌，暂停判定继续使用播放器目标，不能比较 currentTrack。
    const bool toggleCurrent = hasPlayer
        && ((!fallbackUrl.isEmpty() && fallbackUrl == source)
            || (songId > 0 && Oran7NetworkSongResolver::extractSongId(source) == songId));
    if (toggleCurrent) {
        m_networkResolver->retainCurrentSong(songId);
        m_playbackController->toggle();
        return;
    }
    // Opening 会同步通知 QML；先撤销旧请求，再检查是否被新的用户命令替代。
    resetNetworkPlayRequest();
    const quint64 networkGeneration = m_networkResolver->generation();
    if (!hasPlayer) m_playback->beginOpening(fallbackUrl);
    if (m_playbackController->isShuttingDown() || !m_networkResolver->isCurrent(networkGeneration)) return;
    m_networkResolver->resolve(songId, fallbackUrl, isVip);
}

void Oran7MediaClient::onPlaybackOpenFailed()
{
    m_networkResolver->playbackOpenFailed(m_playbackController->source(), currentSongId());
}

void Oran7MediaClient::saveWindowSize(QQmlApplicationEngine &engine)
{
    m_configBinding->saveWindowSize(engine);
}

void Oran7MediaClient::saveWindowPosition(QQmlApplicationEngine &engine)
{
    m_configBinding->saveWindowPosition(engine);
}

void Oran7MediaClient::saveVolume()
{
    m_configBinding->saveVolume(m_playback->volume());
}

bool Oran7MediaClient::restoreConfiguration(QQmlApplicationEngine& engine)
{
    restoreWindowPosition(engine);
    restoreWindowSize(engine);
    const auto directory = createAppDirectories();
    if (directory.isEmpty()) return false;
    m_tracks->restoreIntegratedTracks(m_configBinding->integratedMusic());
    restoreLocalMusicOrder();
    scanLocalMusic(directory);
    restoreFocusedMusic();
    restoreVolume();
    m_musicQueue->setRoamFilterLiked(m_configBinding->roamFilterLiked());
    loadScreenCaptureSettings();
    return true;
}

void Oran7MediaClient::saveConfiguration(QQmlApplicationEngine& engine)
{
    saveFocusedMusic();
    saveLocalMusicOrder();
    saveVolume();
    saveWindowSize(engine);
    saveWindowPosition(engine);
}
