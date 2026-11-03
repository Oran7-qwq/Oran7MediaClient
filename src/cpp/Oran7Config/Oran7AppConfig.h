#pragma once
#include <QJsonObject>
#include <QVariantList>
#include <QStringList>

// 配置快照是值对象；默认值只来自 default.json，不在各调用点重复定义。
struct Oran7WindowConfig { int width{}, height{}, x{}, y{}; bool maximized{}; };
struct Oran7AppearanceConfig { QString themeProfile, language, fontFamily; int fontPixelSize{}; };
struct Oran7PlayerConfig { int volume{}; bool roamFilterLiked{}; };
struct Oran7ScreenCaptureConfig { int outputIndex{}, fps{}; bool drawMouse{}, debugLog{}; };
struct Oran7NeteaseAuthConfig { QString musicU, csrf, nickname; };
struct Oran7BilibiliAuthConfig { QString sessdata, dedeUserId, biliJct, userName; };
struct Oran7AuthConfig { Oran7NeteaseAuthConfig netease; Oran7BilibiliAuthConfig bilibili; };
struct Oran7LocalLibraryConfig { QVariantList playOrder, integrated; QString sortType, lastModified; };
struct Oran7LastMediaConfig {
    QString path, title, artist, album, cover, timesize;
    qint64 songId{};
    bool isVip{}, sourceIsVip{};
};
struct Oran7RecordingConfig {
    int mode{}, width{}, height{}, fps{}, bitrateKbps{};
    bool audioEnabled{};
    QString outputDirectory, filePrefix, whipUrl, bearerToken, roomId, publisherId, stunUrl;
    QString turnUrl, turnUser, turnPassword, signalingUrl, signalingUser, signalingPassword;
};

struct Oran7AppConfig {
    static constexpr int kVersion = 2;
    Oran7WindowConfig window;
    Oran7AppearanceConfig appearance;
    Oran7PlayerConfig player;
    Oran7ScreenCaptureConfig capture;
    Oran7AuthConfig auth;
    Oran7LocalLibraryConfig library;
    Oran7LastMediaConfig lastMedia;
    Oran7RecordingConfig recording;
    QStringList recentFiles;
    int recentFilesMaxCount{};

    // fromJson 接收已归一化文档；toJson 保留未知键和旧列表行格式。
    static Oran7AppConfig fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    static bool normalize(const QJsonObject& input, const QJsonObject& defaults,
                          QJsonObject* output, QString* error = nullptr);
private:
    QJsonObject m_extensions;
};
