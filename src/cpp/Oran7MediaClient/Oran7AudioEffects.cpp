#include "Oran7AudioEffects.h"
#include "Oran7EqualizerModel.h"

#include <QQmlEngine>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {

double clampValue(double value, double minimum, double maximum)
{
    if (!std::isfinite(value)) return minimum;
    return std::max(minimum, std::min(maximum, value));
}

} // namespace

Oran7AudioEffects::Oran7AudioEffects(QObject* parent)
    : QObject(parent), m_equalizer(std::make_unique<Oran7EqualizerModel>())
{
    QQmlEngine::setObjectOwnership(m_equalizer.get(), QQmlEngine::CppOwnership);

    connect(m_equalizer.get(), &Oran7EqualizerModel::gainChanged, this,
        [this](int, double) {
            if (!m_applyingPreset) markCustom();
            publish();
        });

    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(16);
    connect(&m_retryTimer, &QTimer::timeout, this, &Oran7AudioEffects::publish);
}

QVariantList Oran7AudioEffects::presets() const
{
    QVariantList result;
    for (const auto& item : m_customPresets)
    {
        const auto preset = item.toMap();
        if (preset.value(QStringLiteral("id")).toString().isEmpty()) continue;
        result.append(QVariantMap{{QStringLiteral("id"), preset.value(QStringLiteral("id"))},
                                  {QStringLiteral("label"), preset.value(QStringLiteral("name"))},
                                  {QStringLiteral("custom"), true}});
    }
    return result;
}

int Oran7AudioEffects::customPresetIndex(const QString& presetId) const
{
    for (int i = 0; i < m_customPresets.size(); ++i)
    {
        if (m_customPresets.at(i).toMap().value(QStringLiteral("id")).toString() == presetId)
            return i;
    }
    return -1;
}

QVariantMap Oran7AudioEffects::settingsSnapshot() const
{
    QVariantList bands;
    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
        bands.append(m_equalizer->gain(i));

    return QVariantMap{
        {QStringLiteral("preampDb"), m_preampDb},
        {QStringLiteral("outputGainDb"), m_outputGainDb},
        {QStringLiteral("limiterCeilingDb"), m_limiterCeilingDb},
        {QStringLiteral("bandGainsDb"), bands},
        {QStringLiteral("highPassHz"), m_highPassHz},
        {QStringLiteral("lowPassHz"), m_lowPassHz},
        {QStringLiteral("bassBoostDb"), m_bassBoostDb},
        {QStringLiteral("bassFreqHz"), m_bassFreqHz},
        {QStringLiteral("stereoWidth"), m_stereoWidth},
        {QStringLiteral("crossfeed"), m_crossfeed},
        {QStringLiteral("compressorEnabled"), m_compressorEnabled},
        {QStringLiteral("compressorThresholdDb"), m_compressorThresholdDb},
        {QStringLiteral("compressorRatio"), m_compressorRatio},
        {QStringLiteral("compressorAttackMs"), m_compressorAttackMs},
        {QStringLiteral("compressorReleaseMs"), m_compressorReleaseMs},
        {QStringLiteral("compressorKneeDb"), m_compressorKneeDb},
        {QStringLiteral("compressorMakeupDb"), m_compressorMakeupDb}};
}

void Oran7AudioEffects::applySettings(const QVariantMap& settings)
{
    const auto number = [&settings](const char* key, double fallback) {
        const auto value = settings.value(QString::fromLatin1(key));
        return value.isValid() ? value.toDouble() : fallback;
    };

    m_preampDb = clampValue(number("preampDb", 0.0), -24.0, 24.0);
    m_outputGainDb = clampValue(number("outputGainDb", 0.0), -24.0, 24.0);
    m_limiterCeilingDb = clampValue(number("limiterCeilingDb", -1.0), -24.0, 0.0);
    m_highPassHz = clampValue(number("highPassHz", 20.0), 0.0, 1000.0);
    m_lowPassHz = clampValue(number("lowPassHz", 20000.0), 1000.0, 24000.0);
    if (m_highPassHz >= m_lowPassHz) { m_highPassHz = 20.0; m_lowPassHz = 20000.0; }
    m_bassBoostDb = clampValue(number("bassBoostDb", 0.0), 0.0, 12.0);
    m_bassFreqHz = clampValue(number("bassFreqHz", 120.0), 40.0, 300.0);
    m_stereoWidth = clampValue(number("stereoWidth", 1.0), 0.0, 2.0);
    m_crossfeed = std::max(0, std::min(3, settings.value(QStringLiteral("crossfeed"), 0).toInt()));
    m_compressorEnabled = settings.value(QStringLiteral("compressorEnabled"), false).toBool();
    m_compressorThresholdDb = clampValue(number("compressorThresholdDb", -18.0), -60.0, 0.0);
    m_compressorRatio = clampValue(number("compressorRatio", 3.0), 1.0, 20.0);
    m_compressorAttackMs = clampValue(number("compressorAttackMs", 10.0), 0.1, 100.0);
    m_compressorReleaseMs = clampValue(number("compressorReleaseMs", 150.0), 10.0, 1000.0);
    m_compressorKneeDb = clampValue(number("compressorKneeDb", 3.0), 0.0, 12.0);
    m_compressorMakeupDb = clampValue(number("compressorMakeupDb", 0.0), -12.0, 12.0);

    const auto bands = settings.value(QStringLiteral("bandGainsDb")).toList();
    QList<double> gains;
    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
        gains.append(i < bands.size() ? bands.at(i).toDouble() : 0.0);
    m_equalizer->setGains(gains);
}

void Oran7AudioEffects::setPublisher(Publisher publisher)
{
    m_publisher = std::move(publisher);
    publish();
}

Oran7AudioParameters Oran7AudioEffects::parameters() const
{
    Oran7AudioParameters parameters;
    parameters.revision = m_revision;
    parameters.enabled = m_enabled ? 1u : 0u;
    parameters.preampDb = static_cast<float>(m_preampDb);
    parameters.outputGainDb = static_cast<float>(m_outputGainDb);
    parameters.limiterCeilingDb = static_cast<float>(m_limiterCeilingDb);
    parameters.highPassHz = static_cast<float>(m_highPassHz);
    parameters.lowPassHz = static_cast<float>(m_lowPassHz);
    parameters.bassBoostDb = static_cast<float>(m_bassBoostDb);
    parameters.bassFreqHz = static_cast<float>(m_bassFreqHz);
    parameters.stereoWidth = static_cast<float>(m_stereoWidth);
    parameters.crossfeed = static_cast<std::uint32_t>(m_crossfeed);
    parameters.compressorEnabled = m_compressorEnabled ? 1u : 0u;
    parameters.compressorThresholdDb = static_cast<float>(m_compressorThresholdDb);
    parameters.compressorRatio = static_cast<float>(m_compressorRatio);
    parameters.compressorAttackMs = static_cast<float>(m_compressorAttackMs);
    parameters.compressorReleaseMs = static_cast<float>(m_compressorReleaseMs);
    parameters.compressorKneeDb = static_cast<float>(m_compressorKneeDb);
    parameters.compressorMakeupDb = static_cast<float>(m_compressorMakeupDb);
    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
        parameters.bandGainsDb[i] = static_cast<float>(m_equalizer->gain(i));
    return parameters;
}

void Oran7AudioEffects::publish()
{
    m_pending = parameters();
    m_pending.revision = ++m_revision;

    if (!m_publisher)
    {
        m_publishPending = false;
        return;
    }

    if (m_publisher(m_pending))
    {
        m_publishPending = false;
        m_retryTimer.stop();
        return;
    }

    requestPublishRetry();
}

void Oran7AudioEffects::publishCurrent()
{
    if (!m_publisher) return;
    Oran7AudioParameters parameters = this->parameters();
    m_publisher(parameters);
}

void Oran7AudioEffects::requestPublishRetry()
{
    m_publishPending = true;
    if (!m_retryTimer.isActive())
        m_retryTimer.start();
}

void Oran7AudioEffects::markCustom()
{
    if (m_applyingPreset || m_presetId == QStringLiteral("custom")) return;
    m_presetId = QStringLiteral("custom");
    emit presetIdChanged();
}

void Oran7AudioEffects::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    emit enabledChanged();
    publish();
}

void Oran7AudioEffects::setPreampDb(double value)
{
    value = clampValue(value, -24.0, 24.0);
    if (qFuzzyCompare(m_preampDb, value)) return;
    m_preampDb = value;
    markCustom();
    emit preampDbChanged();
    publish();
}

void Oran7AudioEffects::setOutputGainDb(double value)
{
    value = clampValue(value, -24.0, 24.0);
    if (qFuzzyCompare(m_outputGainDb, value)) return;
    m_outputGainDb = value;
    markCustom();
    emit outputGainDbChanged();
    publish();
}

void Oran7AudioEffects::setLimiterCeilingDb(double value)
{
    value = clampValue(value, -24.0, 0.0);
    if (qFuzzyCompare(m_limiterCeilingDb, value)) return;
    m_limiterCeilingDb = value;
    markCustom();
    emit limiterCeilingDbChanged();
    publish();
}

void Oran7AudioEffects::setHighPassHz(double value)
{
    value = clampValue(value, 0.0, 1000.0);
    if (qFuzzyCompare(m_highPassHz, value)) return;
    m_highPassHz = value;
    markCustom();
    emit highPassHzChanged();
    publish();
}

void Oran7AudioEffects::setLowPassHz(double value)
{
    value = clampValue(value, 1000.0, 24000.0);
    if (qFuzzyCompare(m_lowPassHz, value)) return;
    m_lowPassHz = value;
    markCustom();
    emit lowPassHzChanged();
    publish();
}

void Oran7AudioEffects::setBassBoostDb(double value)
{
    value = clampValue(value, 0.0, 12.0);
    if (qFuzzyCompare(m_bassBoostDb, value)) return;
    m_bassBoostDb = value;
    markCustom();
    emit bassBoostDbChanged();
    publish();
}

void Oran7AudioEffects::setStereoWidth(double value)
{
    value = clampValue(value, 0.0, 2.0);
    if (qFuzzyCompare(m_stereoWidth, value)) return;
    m_stereoWidth = value;
    markCustom();
    emit stereoWidthChanged();
    publish();
}

void Oran7AudioEffects::setCrossfeed(int value)
{
    value = std::max(0, std::min(3, value));
    if (m_crossfeed == value) return;
    m_crossfeed = value;
    markCustom();
    emit crossfeedChanged();
    publish();
}

void Oran7AudioEffects::setCompressorEnabled(bool enabled)
{
    if (m_compressorEnabled == enabled) return;
    m_compressorEnabled = enabled;
    markCustom();
    emit compressorEnabledChanged();
    publish();
}

void Oran7AudioEffects::setCompressorThresholdDb(double value)
{
    value = clampValue(value, -60.0, 0.0);
    if (qFuzzyCompare(m_compressorThresholdDb, value)) return;
    m_compressorThresholdDb = value;
    markCustom();
    emit compressorThresholdDbChanged();
    publish();
}

void Oran7AudioEffects::setCompressorRatio(double value)
{
    value = clampValue(value, 1.0, 20.0);
    if (qFuzzyCompare(m_compressorRatio, value)) return;
    m_compressorRatio = value;
    markCustom();
    emit compressorRatioChanged();
    publish();
}

void Oran7AudioEffects::setCompressorMakeupDb(double value)
{
    value = clampValue(value, -12.0, 12.0);
    if (qFuzzyCompare(m_compressorMakeupDb, value)) return;
    m_compressorMakeupDb = value;
    markCustom();
    emit compressorMakeupDbChanged();
    publish();
}

void Oran7AudioEffects::applyPreset(const QString& presetId)
{
    const int index = customPresetIndex(presetId);
    if (index < 0) return;

    const auto preset = m_customPresets.at(index).toMap();
    applySettings(preset.value(QStringLiteral("settings")).toMap());
    m_presetId = presetId;
    emit presetIdChanged();
    emit preampDbChanged();
    emit outputGainDbChanged();
    emit limiterCeilingDbChanged();
    emit highPassHzChanged();
    emit lowPassHzChanged();
    emit bassBoostDbChanged();
    emit stereoWidthChanged();
    emit crossfeedChanged();
    emit compressorEnabledChanged();
    emit compressorThresholdDbChanged();
    emit compressorRatioChanged();
    emit compressorMakeupDbChanged();
    publish();
}

QString Oran7AudioEffects::suggestedPresetName() const
{
    QSet<QString> names;
    for (const auto& item : m_customPresets)
        names.insert(item.toMap().value(QStringLiteral("name")).toString());
    for (int i = 1; ; ++i)
    {
        const QString name = QStringLiteral("自定义%1").arg(i);
        if (!names.contains(name)) return name;
    }
}

QString Oran7AudioEffects::saveCurrentAsPreset(const QString& name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) trimmed = suggestedPresetName();

    const QString id = QStringLiteral("custom-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap preset;
    preset.insert(QStringLiteral("id"), id);
    preset.insert(QStringLiteral("name"), trimmed);
    preset.insert(QStringLiteral("settings"), settingsSnapshot());
    m_customPresets.append(preset);

    m_presetId = id;
    emit presetsChanged();
    emit presetIdChanged();
    publish();
    return id;
}

bool Oran7AudioEffects::renamePreset(const QString& presetId, const QString& name)
{
    const int index = customPresetIndex(presetId);
    if (index < 0) return false;

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return false;

    auto preset = m_customPresets.at(index).toMap();
    preset.insert(QStringLiteral("name"), trimmed);
    m_customPresets[index] = preset;

    emit presetsChanged();
    if (m_presetId == presetId) emit presetIdChanged();
    return true;
}

bool Oran7AudioEffects::deletePreset(const QString& presetId)
{
    const int index = customPresetIndex(presetId);
    if (index < 0) return false;

    m_customPresets.removeAt(index);
    if (m_presetId == presetId) m_presetId = QStringLiteral("custom");

    emit presetsChanged();
    emit presetIdChanged();
    return true;
}

void Oran7AudioEffects::reset()
{
    m_applyingPreset = true;
    m_equalizer->reset();
    m_applyingPreset = false;

    m_preampDb = 0.0;
    m_outputGainDb = 0.0;
    m_limiterCeilingDb = -1.0;
    m_highPassHz = 20.0;
    m_lowPassHz = 20000.0;
    m_bassBoostDb = 0.0;
    m_bassFreqHz = 120.0;
    m_stereoWidth = 1.0;
    m_crossfeed = 0;
    m_compressorEnabled = false;
    m_compressorThresholdDb = -18.0;
    m_compressorRatio = 3.0;
    m_compressorAttackMs = 10.0;
    m_compressorReleaseMs = 150.0;
    m_compressorKneeDb = 3.0;
    m_compressorMakeupDb = 0.0;
    m_presetId = QStringLiteral("custom");

    emit presetIdChanged();
    emit preampDbChanged();
    emit outputGainDbChanged();
    emit limiterCeilingDbChanged();
    emit highPassHzChanged();
    emit lowPassHzChanged();
    emit bassBoostDbChanged();
    emit stereoWidthChanged();
    emit crossfeedChanged();
    emit compressorEnabledChanged();
    emit compressorThresholdDbChanged();
    emit compressorRatioChanged();
    emit compressorMakeupDbChanged();
    publish();
}

void Oran7AudioEffects::loadFromConfig(const Oran7AudioConfig& config)
{
    m_enabled = config.enabled;
    m_preampDb = clampValue(config.preampDb, -24.0, 24.0);
    m_outputGainDb = clampValue(config.outputGainDb, -24.0, 24.0);
    m_limiterCeilingDb = clampValue(config.limiterCeilingDb, -24.0, 0.0);
    m_highPassHz = clampValue(config.highPassHz, 0.0, 1000.0);
    m_lowPassHz = clampValue(config.lowPassHz, 1000.0, 24000.0);
    if (m_highPassHz >= m_lowPassHz) { m_highPassHz = 20.0; m_lowPassHz = 20000.0; }
    m_bassBoostDb = clampValue(config.bassBoostDb, 0.0, 12.0);
    m_bassFreqHz = clampValue(config.bassFreqHz, 40.0, 300.0);
    m_stereoWidth = clampValue(config.stereoWidth, 0.0, 2.0);
    m_crossfeed = std::max(0, std::min(3, config.crossfeed));
    m_compressorEnabled = config.compressorEnabled;
    m_compressorThresholdDb = clampValue(config.compressorThresholdDb, -60.0, 0.0);
    m_compressorRatio = clampValue(config.compressorRatio, 1.0, 20.0);
    m_compressorAttackMs = clampValue(config.compressorAttackMs, 0.1, 100.0);
    m_compressorReleaseMs = clampValue(config.compressorReleaseMs, 10.0, 1000.0);
    m_compressorKneeDb = clampValue(config.compressorKneeDb, 0.0, 12.0);
    m_compressorMakeupDb = clampValue(config.compressorMakeupDb, -12.0, 12.0);
    m_customPresets = config.customPresets;

    const QString presetId = config.presetId.isEmpty() ? QStringLiteral("custom") : config.presetId;
    if (presetId == QStringLiteral("custom") || customPresetIndex(presetId) >= 0)
        m_presetId = presetId;
    else
        m_presetId = QStringLiteral("custom");

    QList<double> gains;
    gains.reserve(Oran7AudioParameters::kBandCount);
    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
        gains.append(i < config.bandGainsDb.size() ? config.bandGainsDb.at(i) : 0.0);
    m_equalizer->setGains(gains);

    emit enabledChanged();
    emit presetIdChanged();
    emit preampDbChanged();
    emit outputGainDbChanged();
    emit limiterCeilingDbChanged();
    emit highPassHzChanged();
    emit lowPassHzChanged();
    emit bassBoostDbChanged();
    emit stereoWidthChanged();
    emit crossfeedChanged();
    emit compressorEnabledChanged();
    emit compressorThresholdDbChanged();
    emit compressorRatioChanged();
    emit compressorMakeupDbChanged();
    publish();
}

Oran7AudioConfig Oran7AudioEffects::toConfig() const
{
    Oran7AudioConfig config;
    config.version = 2;
    config.enabled = m_enabled;
    config.presetId = m_presetId;
    config.preampDb = m_preampDb;
    config.outputGainDb = m_outputGainDb;
    config.limiterCeilingDb = m_limiterCeilingDb;
    config.highPassHz = m_highPassHz;
    config.lowPassHz = m_lowPassHz;
    config.bassBoostDb = m_bassBoostDb;
    config.bassFreqHz = m_bassFreqHz;
    config.stereoWidth = m_stereoWidth;
    config.crossfeed = m_crossfeed;
    config.compressorEnabled = m_compressorEnabled;
    config.compressorThresholdDb = m_compressorThresholdDb;
    config.compressorRatio = m_compressorRatio;
    config.compressorAttackMs = m_compressorAttackMs;
    config.compressorReleaseMs = m_compressorReleaseMs;
    config.compressorKneeDb = m_compressorKneeDb;
    config.compressorMakeupDb = m_compressorMakeupDb;
    for (int i = 0; i < Oran7AudioParameters::kBandCount; ++i)
        config.bandGainsDb.append(m_equalizer->gain(i));
    config.customPresets = m_customPresets;
    return config;
}
