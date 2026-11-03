#include "Oran7ConfigBinding.h"
#include "Oran7AppConfigManager.h"
#include "Oran7GlobalHelper.h"
#include "Oran7MediaInfoReader.h"
#include "Oran7NetworkSongResolver.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QQmlApplicationEngine>
#include <QRectF>
#include <utility>

namespace
{
QObject* oran7MainWindow(QQmlApplicationEngine& engine)
{
    for (auto* root : engine.rootObjects())
        if (root->objectName() == QStringLiteral("__Oran7Window__")) return root;
    // 启动异步孵化的主窗由引擎 parent 持有，不属于 ApplicationEngine.load 的根列表。
    return engine.findChild<QObject*>(QStringLiteral("__Oran7Window__"), Qt::FindDirectChildrenOnly);
}
std::optional<QRect> oran7NormalGeometry(QQmlApplicationEngine& engine)
{
    auto* window = oran7MainWindow(engine);
    if (!window) return std::nullopt;
    // 退出可能早于防抖定时器；普通窗口在保存边界再采集一次。
    if (window->metaObject()->indexOfMethod("captureNormalGeometry()") >= 0)
        QMetaObject::invokeMethod(window, "captureNormalGeometry", Qt::DirectConnection);
    const auto geometry = window->property("normalWindowGeometry").toRectF().toRect();
    if (!geometry.isValid()) return std::nullopt;
    return geometry;
}
}

Oran7ConfigBinding::Oran7ConfigBinding(Oran7AppConfigManager& config, MetadataReader reader)
    : m_config(config), m_reader(reader ? std::move(reader) : MetadataReader(&Oran7MediaInfoReader::analyzeMediaFileInfo)),
      m_audioDirectory(Oran7GlobalHelper::getConfigDir() + "/audioTemp") {}

QString Oran7ConfigBinding::createDirectories()
{
    const QStringList directories{QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
        m_audioDirectory, m_audioDirectory + "/audioCover", lyricsDirectory()};
    for (const auto& directory : directories) {
        if (!QDir().mkpath(directory)) {
            WARNING_LOG << "无法创建应用目录：" << directory;
            return {};
        }
    }
    return QDir(m_audioDirectory).absolutePath();
}

std::optional<Oran7Track> Oran7ConfigBinding::localTrack(const QString& path)
{
    const auto info = m_reader(path);
    if (info.value("success").toInt() != 0) return std::nullopt;
    QString cover = info.value("cover").toString();
    if (!cover.startsWith("file:///")) cover = "file:///" + QDir::toNativeSeparators(cover);
    return Oran7Track{path, info.value("title").toString(), info.value("artist").toString(),
        info.value("album").toString(), QUrl(cover).toString(QUrl::FullyEncoded),
        info.value("duration").toString(), -1, false};
}

std::optional<Oran7Track> Oran7ConfigBinding::restoredTrack()
{
    const auto saved = m_config.snapshot().lastMedia;
    const auto songId = saved.songId;
    QString path = m_config.getLastUsedFile();
    if (songId > 0 || Oran7NetworkSongResolver::isNetworkUrl(path)) {
        if (!Oran7NetworkSongResolver::isNetworkUrl(path)) path.clear();
        return Oran7Track{path, saved.title,
            saved.artist,
            saved.album,
            saved.cover,
            saved.timesize, songId,
            saved.isVip};
    }
    if (path.isEmpty()) return std::nullopt;
    return localTrack(path);
}

void Oran7ConfigBinding::rememberTrack(const Oran7Track& track)
{
    auto saved = m_config.snapshot();
    saved.lastMedia.title = track.title;
    saved.lastMedia.artist = track.artist;
    saved.lastMedia.album = track.album;
    saved.lastMedia.cover = track.cover;
    saved.lastMedia.timesize = track.timesize;
    saved.lastMedia.songId = track.songId;
    saved.lastMedia.isVip = track.isVip;
    m_config.updateConfig(saved);
    m_config.saveConfig();
}

void Oran7ConfigBinding::completePersistedDetails(const Oran7Track& track)
{
    auto saved = m_config.snapshot();
    if (saved.lastMedia.songId != track.songId) return;
    saved.lastMedia.album = track.album;
    saved.lastMedia.timesize = track.timesize;
    m_config.updateConfig(saved);
    m_config.saveConfig();
}

void Oran7ConfigBinding::rememberMusicSource(const QString& path, bool isVip)
{
    m_lastMusicSource = path;
    if (Oran7NetworkSongResolver::isNetworkUrl(path)) m_config.setLastUsedFile(path, isVip);
    else {
        auto saved = m_config.snapshot();
        saved.lastMedia.songId = 0;
        m_config.updateConfig(saved);
        m_config.setLastUsedFile(path, false);
        m_config.saveConfig();
    }
}

void Oran7ConfigBinding::setFocusedMusicSource(const QString& path)
{
    m_lastMusicSource = Oran7NetworkSongResolver::isNetworkUrl(path) ? path : QFileInfo(path).absoluteFilePath();
}

void Oran7ConfigBinding::saveLastFocused(bool isVip)
{
    if (!m_lastMusicSource.isEmpty()) m_config.setLastUsedFile(m_lastMusicSource, isVip);
}

QVariantList Oran7ConfigBinding::localOrder() const { return m_config.snapshot().library.playOrder; }
void Oran7ConfigBinding::saveLocalOrder(const QVariantList& order) {
    auto saved = m_config.snapshot(); saved.library.playOrder = order;
    m_config.updateConfig(saved); m_config.saveConfig();
}
int Oran7ConfigBinding::volume() const { return m_config.snapshot().player.volume; }
bool Oran7ConfigBinding::roamFilterLiked() const { return m_config.snapshot().player.roamFilterLiked; }
bool Oran7ConfigBinding::saveRoamFilterLiked(bool enabled)
{
    auto saved = m_config.snapshot();
    if (saved.player.roamFilterLiked == enabled) return true;
    saved.player.roamFilterLiked = enabled;
    m_config.updateConfig(saved);
    return m_config.saveConfig();
}
void Oran7ConfigBinding::saveVolume(int volume) {
    auto saved = m_config.snapshot(); saved.player.volume = volume; m_config.updateConfig(saved);
}

QVariantList Oran7ConfigBinding::integratedMusic()
{
    return m_config.snapshot().library.integrated;
}

void Oran7ConfigBinding::saveIntegratedMusic(const QVariantList& list)
{
    auto saved = m_config.snapshot(); saved.library.integrated = list; m_config.updateConfig(saved);
    m_config.saveConfig();
}

void Oran7ConfigBinding::loadWindowSize(QQmlApplicationEngine &engine)
{
    int AppWindow_width = m_config.snapshot().window.width;
    int AppWindow_height = m_config.snapshot().window.height;

    QObject* AppWindow = oran7MainWindow(engine);
    if(AppWindow)
    {
        AppWindow->setProperty("width",AppWindow_width);
        AppWindow->setProperty("height",AppWindow_height);
    }

    CONFIG_LOG<<"LoadConfig of set AppWindow_width"<<AppWindow_width<<"; AppWindow_height"<<AppWindow_height;
}

void Oran7ConfigBinding::loadWindowPosition(QQmlApplicationEngine &engine)
{
    int AppWindow_PosX = m_config.snapshot().window.x;
    int AppWindow_PosY = m_config.snapshot().window.y;

    QObject* AppWindow = oran7MainWindow(engine);
    if(AppWindow)
    {
        AppWindow->setProperty("x",AppWindow_PosX);
        AppWindow->setProperty("y",AppWindow_PosY);
    }
}

void Oran7ConfigBinding::saveWindowSize(QQmlApplicationEngine &engine)
{
    const auto geometry = oran7NormalGeometry(engine);
    if (!geometry) return;
    auto saved = m_config.snapshot();
    saved.window.width = geometry->width(); saved.window.height = geometry->height();
    m_config.updateConfig(saved);
}

void Oran7ConfigBinding::saveWindowPosition(QQmlApplicationEngine &engine)
{
    const auto geometry = oran7NormalGeometry(engine);
    if (!geometry) return;
    auto saved = m_config.snapshot(); saved.window.x = geometry->x(); saved.window.y = geometry->y();
    m_config.updateConfig(saved);
}
