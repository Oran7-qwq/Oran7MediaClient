#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <functional>
#include <memory>

#include "../Oran7Config/Oran7AppConfig.h"
#include "../Oran7MediaPlayer/Audio/Oran7AudioParameters.h"
#include "Oran7EqualizerModel.h"

/**
 * @brief 音效参数与预设的 GUI 权威对象（EXE，Client 独占）。
 *
 * 只产出 Oran7AudioParameters 值对象并发布到当前播放器队列；
 * 不持有播放器、不直接触碰音频线程、不写配置文件。
 */
class Oran7AudioEffects : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString presetId READ presetId NOTIFY presetIdChanged)
    Q_PROPERTY(double preampDb READ preampDb WRITE setPreampDb NOTIFY preampDbChanged)
    Q_PROPERTY(double outputGainDb READ outputGainDb WRITE setOutputGainDb NOTIFY outputGainDbChanged)
    Q_PROPERTY(double limiterCeilingDb READ limiterCeilingDb WRITE setLimiterCeilingDb NOTIFY limiterCeilingDbChanged)
    Q_PROPERTY(double highPassHz READ highPassHz WRITE setHighPassHz NOTIFY highPassHzChanged)
    Q_PROPERTY(double lowPassHz READ lowPassHz WRITE setLowPassHz NOTIFY lowPassHzChanged)
    Q_PROPERTY(double bassBoostDb READ bassBoostDb WRITE setBassBoostDb NOTIFY bassBoostDbChanged)
    Q_PROPERTY(double stereoWidth READ stereoWidth WRITE setStereoWidth NOTIFY stereoWidthChanged)
    Q_PROPERTY(int crossfeed READ crossfeed WRITE setCrossfeed NOTIFY crossfeedChanged)
    Q_PROPERTY(bool compressorEnabled READ compressorEnabled WRITE setCompressorEnabled NOTIFY compressorEnabledChanged)
    Q_PROPERTY(double compressorThresholdDb READ compressorThresholdDb WRITE setCompressorThresholdDb NOTIFY compressorThresholdDbChanged)
    Q_PROPERTY(double compressorRatio READ compressorRatio WRITE setCompressorRatio NOTIFY compressorRatioChanged)
    Q_PROPERTY(double compressorMakeupDb READ compressorMakeupDb WRITE setCompressorMakeupDb NOTIFY compressorMakeupDbChanged)
    Q_PROPERTY(Oran7EqualizerModel* equalizer READ equalizer CONSTANT)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY presetsChanged)

public:
    using Publisher = std::function<bool(const Oran7AudioParameters&)>;

    explicit Oran7AudioEffects(QObject* parent = nullptr);

    bool enabled() const { return m_enabled; }
    QString presetId() const { return m_presetId; }
    double preampDb() const { return m_preampDb; }
    double outputGainDb() const { return m_outputGainDb; }
    double limiterCeilingDb() const { return m_limiterCeilingDb; }
    double highPassHz() const { return m_highPassHz; }
    double lowPassHz() const { return m_lowPassHz; }
    double bassBoostDb() const { return m_bassBoostDb; }
    double stereoWidth() const { return m_stereoWidth; }
    int crossfeed() const { return m_crossfeed; }
    bool compressorEnabled() const { return m_compressorEnabled; }
    double compressorThresholdDb() const { return m_compressorThresholdDb; }
    double compressorRatio() const { return m_compressorRatio; }
    double compressorMakeupDb() const { return m_compressorMakeupDb; }
    Oran7EqualizerModel* equalizer() const { return m_equalizer.get(); }
    QVariantList presets() const;

    void setPublisher(Publisher publisher);
    Oran7AudioParameters parameters() const;

    Q_INVOKABLE void applyPreset(const QString& presetId);
    Q_INVOKABLE void reset();
    Q_INVOKABLE QString suggestedPresetName() const;
    Q_INVOKABLE QString saveCurrentAsPreset(const QString& name = QString());
    Q_INVOKABLE bool renamePreset(const QString& presetId, const QString& name);
    Q_INVOKABLE bool deletePreset(const QString& presetId);

    void loadFromConfig(const Oran7AudioConfig& config);
    Oran7AudioConfig toConfig() const;

public slots:
    void setEnabled(bool enabled);
    void setPreampDb(double value);
    void setOutputGainDb(double value);
    void setLimiterCeilingDb(double value);
    void setHighPassHz(double value);
    void setLowPassHz(double value);
    void setBassBoostDb(double value);
    void setStereoWidth(double value);
    void setCrossfeed(int value);
    void setCompressorEnabled(bool enabled);
    void setCompressorThresholdDb(double value);
    void setCompressorRatio(double value);
    void setCompressorMakeupDb(double value);

signals:
    void enabledChanged();
    void presetIdChanged();
    void preampDbChanged();
    void outputGainDbChanged();
    void limiterCeilingDbChanged();
    void highPassHzChanged();
    void lowPassHzChanged();
    void bassBoostDbChanged();
    void stereoWidthChanged();
    void crossfeedChanged();
    void compressorEnabledChanged();
    void compressorThresholdDbChanged();
    void compressorRatioChanged();
    void compressorMakeupDbChanged();
    void presetsChanged();

private:
    void markCustom();
    void publish();
    void publishCurrent();
    void requestPublishRetry();
    QVariantMap settingsSnapshot() const;
    void applySettings(const QVariantMap& settings);
    int customPresetIndex(const QString& presetId) const;

    std::unique_ptr<Oran7EqualizerModel> m_equalizer;
    Publisher m_publisher;
    QTimer m_retryTimer;
    Oran7AudioParameters m_pending{};
    QString m_presetId = QStringLiteral("custom");
    double m_preampDb = 0.0;
    double m_outputGainDb = 0.0;
    double m_limiterCeilingDb = -1.0;
    double m_highPassHz = 20.0;
    double m_lowPassHz = 20000.0;
    double m_bassBoostDb = 0.0;
    double m_bassFreqHz = 120.0;
    double m_stereoWidth = 1.0;
    int m_crossfeed = 0;
    bool m_compressorEnabled = false;
    double m_compressorThresholdDb = -18.0;
    double m_compressorRatio = 3.0;
    double m_compressorAttackMs = 10.0;
    double m_compressorReleaseMs = 150.0;
    double m_compressorKneeDb = 3.0;
    double m_compressorMakeupDb = 0.0;
    QVariantList m_customPresets;
    quint32 m_revision = 0;
    bool m_enabled = false;
    bool m_applyingPreset = false;
    bool m_publishPending = false;
};
