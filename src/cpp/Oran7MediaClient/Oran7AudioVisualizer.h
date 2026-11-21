#pragma once

#include "../Oran7Config/Oran7AppConfig.h"

#include <QObject>
#include <QElapsedTimer>
#include <QPointer>
#include <QRect>
#include <QScreen>
#include <QTimer>
#include <QVector>
#include <QWindow>
#include <chrono>
#include <memory>

class Oran7AudioSpectrumTap;

struct Oran7SpectrumBar
{
    float sustained = 0.f;
    float transient = 0.f;
    float height() const { return sustained + (1.f - sustained) * transient; }
};

class Oran7AudioVisualizer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QRect overlayGeometry READ overlayGeometry NOTIFY layoutChanged)
    Q_PROPERTY(int barCount READ barCount NOTIFY layoutChanged)
    Q_PROPERTY(bool showing READ showing NOTIFY showingChanged)

public:
    explicit Oran7AudioVisualizer(const Oran7VisualizerConfig& defaults, QObject* parent = nullptr);
    ~Oran7AudioVisualizer() override;
    QVariantMap settings() const { return effectiveConfig().toJson().toVariantMap(); }
    const Oran7VisualizerConfig& config() const { return m_config; }
    const QVector<Oran7SpectrumBar>& levels() const { return m_levels; }
    float musicPulse() const { return m_musicPulse; }
    QRect overlayGeometry() const { return m_geometry; }
    int uncappedOverlayHeight() const { return m_uncappedOverlayHeight; }
    int barCount() const { return m_barCount; }
    static float barHeightScale(int index);
    static float barResponse(float level, float recentPeak);
    static void smoothBands(const QVector<float>& levels, int bassBars, QVector<float>& output);
    bool showing() const { return m_showing; }
    void load(const Oran7VisualizerConfig& config);
    void setDeveloperMode(bool enabled);
    void setTap(std::shared_ptr<Oran7AudioSpectrumTap> tap);
    void setPlaying(bool playing, bool reset);
    void shutdown();
    Q_INVOKABLE void setOption(const QString& key, const QVariant& value);
    Q_INVOKABLE void setHostWindow(QWindow* window);
    Q_INVOKABLE void placeOverlay(QWindow* window);
    Q_INVOKABLE void prepareOverlay(QWindow* window);

signals:
    void settingsChanged();
    void edited();
    void layoutChanged();
    void frameReady();
    void showingChanged();

private:
    struct Worker;
    Oran7VisualizerConfig effectiveConfig() const;
    void refreshLayout();
    void refreshCapture(bool reset = false);
    void scheduleNextFrame();
    void publish();
    void updateShowing();
    std::unique_ptr<Worker> m_worker;
    const Oran7VisualizerConfig m_defaults;
    Oran7VisualizerConfig m_config;
    std::shared_ptr<Oran7AudioSpectrumTap> m_tap;
    QTimer m_timer;
    QPointer<QWindow> m_host;
    QPointer<QScreen> m_screen;
    QVector<Oran7SpectrumBar> m_levels, m_target;
    QElapsedTimer m_animationClock;
    std::chrono::steady_clock::time_point m_targetAt{};
    std::chrono::steady_clock::time_point m_nextFrameAt{};
    QRect m_geometry;
    int m_barCount = 0;
    int m_uncappedOverlayHeight = 0;
    float m_musicPulse = 0.f, m_targetMusicPulse = 0.f;
    quint64 m_frame = 0;
    bool m_targetBassAccent = false;
    bool m_developerMode = false, m_playing = false, m_showing = false;
};
