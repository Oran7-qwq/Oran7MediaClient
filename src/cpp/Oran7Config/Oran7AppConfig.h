#pragma once
#include <QJsonObject>
#include <QVariantList>
#include <QStringList>

// 配置快照是值对象；默认值只来自 default.json，不在各调用点重复定义。
struct Oran7WindowConfig { int width{}, height{}, x{}, y{}; bool maximized{}; };
struct Oran7AppearanceConfig { QString themeProfile, language, fontFamily; int fontPixelSize{}; };
struct Oran7GeneralConfig { bool developerMode{}; };
struct Oran7PlayerConfig { int volume{}; bool roamFilterLiked{}; };
struct Oran7ScreenCaptureConfig { int outputIndex{}, fps{}; bool drawMouse{}, debugLog{}; };
struct Oran7NeteaseAuthConfig { QString musicU, csrf, nickname; };
struct Oran7QQMusicAuthConfig { QVariantList cookies; QString userId, nickname; };
struct Oran7BilibiliAuthConfig { QString sessdata, dedeUserId, biliJct, userName; };
struct Oran7AuthConfig { Oran7NeteaseAuthConfig netease; Oran7BilibiliAuthConfig bilibili; Oran7QQMusicAuthConfig qqMusic; };
struct Oran7LocalLibraryConfig { QVariantList playOrder, integrated; QString sortType, lastModified; };
struct Oran7LastMediaConfig {
    QString path, title, artist, album, cover, timesize;
    QString provider, providerId, mediaMid;
    qint64 nativeId{};
    qint64 songId{};
    bool isVip{}, sourceIsVip{};
};
struct Oran7RecordingConfig {
    QString format;
    int mode{}, width{}, height{}, fps{}, bitrateKbps{};
    bool audioEnabled{};
    QString outputDirectory, filePrefix, whipUrl, bearerToken, roomId, publisherId, stunUrl;
    QString turnUrl, turnUser, turnPassword, signalingUrl, signalingUser, signalingPassword;
};
struct Oran7LiveConfig {
    QString platform, server, streamKey;
    int width{}, height{}, fps{}, bitrateKbps{};
    bool audioEnabled{};
    bool microphoneEnabled{};
    int systemVolume{}, microphoneVolume{};
};
struct Oran7AudioConfig {
    int version{2};
    bool enabled{};
    QString presetId;
    double preampDb{};
    double outputGainDb{};
    double limiterCeilingDb{-1.0};
    QList<double> bandGainsDb;
    QVariantList customPresets;

    double highPassHz{20.0};
    double lowPassHz{20000.0};
    double bassBoostDb{};
    double bassFreqHz{120.0};
    double stereoWidth{1.0};
    int crossfeed{};
    bool compressorEnabled{};
    double compressorThresholdDb{-18.0};
    double compressorRatio{3.0};
    double compressorAttackMs{10.0};
    double compressorReleaseMs{150.0};
    double compressorKneeDb{3.0};
    double compressorMakeupDb{};
};

struct Oran7VisualizerConfig {
    static constexpr double kSmoothing = 1.0;
    static constexpr int kFps = 60;
    bool enabled{}, alwaysOnTop{}, stretchToEdges{};
    int barWidth{}, barGap{}, height{}, maxHeight{}, bottomOffset{}, minFrequency{}, maxFrequency{};
    double opacity{}, gain{};
    QString screen;

    static Oran7VisualizerConfig fromJson(const QJsonObject& value);
    QJsonObject toJson() const;
    static QJsonObject normalize(const QJsonObject& input, const QJsonObject& defaults);
};

struct Oran7AppConfig {
    static constexpr int kVersion = 2;
    Oran7WindowConfig window;
    Oran7AppearanceConfig appearance;
    Oran7GeneralConfig general;
    Oran7PlayerConfig player;
    Oran7ScreenCaptureConfig capture;
    Oran7AuthConfig auth;
    Oran7LocalLibraryConfig library;
    Oran7LastMediaConfig lastMedia;
    Oran7RecordingConfig recording;
    Oran7LiveConfig live;
    Oran7AudioConfig audio;
    Oran7VisualizerConfig visualizer;
    QStringList recentFiles;
    QVariantList videoHistory;
    int recentFilesMaxCount{};

    // fromJson 接收已归一化文档；toJson 保留未知键和旧列表行格式。
    static Oran7AppConfig fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    static bool normalize(const QJsonObject& input, const QJsonObject& defaults,
                          QJsonObject* output, QString* error = nullptr);
private:
    QJsonObject m_extensions;
};
