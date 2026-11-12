#include "Oran7AudioEffects.h"
#include "Oran7EqualizerModel.h"

#include <QCoreApplication>
#include <QModelIndex>

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

int checks = 0;

void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
    ++checks;
}

bool near(double a, double b, double tolerance = 1e-6)
{
    return std::fabs(a - b) <= tolerance;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    Oran7AudioEffects effects;
    check(effects.presets().isEmpty(), "no built-in presets");
    check(effects.equalizer() != nullptr, "equalizer model exists");
    check(effects.equalizer()->rowCount() == 10, "ten equalizer bands");
    check(effects.equalizer()->data(effects.equalizer()->index(0), Oran7EqualizerModel::LabelRole).toString()
              == QStringLiteral("31"), "first band label");
    check(!effects.enabled(), "effects disabled by default");
    check(effects.presetId() == QStringLiteral("custom"), "custom default preset");
    check(effects.suggestedPresetName() == QStringLiteral("自定义1"), "default suggested preset name");

    Oran7AudioParameters published;
    int publications = 0;
    effects.setPublisher([&](const Oran7AudioParameters& parameters) {
        published = parameters;
        ++publications;
        return true;
    });
    check(publications > 0, "initial publish");

    effects.setEnabled(true);
    check(published.enabled == 1, "enabled published");
    effects.setPreampDb(6.0);
    check(near(published.preampDb, 6.0f), "preamp published");
    check(effects.presetId() == QStringLiteral("custom"), "manual change marks custom");

    effects.equalizer()->setGain(0, 6.0);
    check(near(published.bandGainsDb[0], 6.0f), "band gain published");
    effects.equalizer()->setGain(0, 99.0);
    check(near(effects.equalizer()->gain(0), 12.0), "band gain clamped");
    effects.equalizer()->setGain(0, 6.0);

    effects.setLimiterCeilingDb(-40.0);
    check(near(effects.limiterCeilingDb(), -24.0), "ceiling clamped");
    effects.setHighPassHz(120.0);
    effects.setLowPassHz(16000.0);
    effects.setBassBoostDb(6.0);
    effects.setStereoWidth(1.4);
    effects.setCrossfeed(2);
    effects.setCompressorEnabled(true);
    effects.setCompressorThresholdDb(-24.0);
    effects.setCompressorRatio(4.0);
    effects.setCompressorMakeupDb(3.0);

    const QString customId = effects.saveCurrentAsPreset(QStringLiteral("测试预设"));
    check(!customId.isEmpty(), "custom preset id");
    check(effects.presets().size() == 1, "custom preset appended");
    check(effects.presetId() == customId, "custom preset active");
    check(effects.presets().at(0).toMap().value(QStringLiteral("custom")).toBool(),
          "custom preset flagged");

    // 改掉当前值，再应用自定义预设，确认完整快照恢复。
    effects.equalizer()->setGain(0, 0.0);
    effects.setPreampDb(0.0);
    effects.setBassBoostDb(0.0);
    effects.applyPreset(customId);
    check(near(effects.equalizer()->gain(0), 6.0), "custom preset restores bands");
    check(near(effects.preampDb(), 6.0), "custom preset restores preamp");
    check(near(effects.bassBoostDb(), 6.0), "custom preset restores A2 fields");
    check(near(effects.highPassHz(), 120.0), "custom preset restores high pass");
    check(effects.crossfeed() == 2, "custom preset restores crossfeed");

    const auto config = effects.toConfig();
    check(config.version == 2, "audio config version");
    check(config.enabled, "config enabled");
    check(config.presetId == customId, "config custom id");
    check(config.customPresets.size() == 1, "custom preset persisted in config");
    check(near(config.highPassHz, 120.0), "config high pass");
    check(near(config.lowPassHz, 16000.0), "config low pass");
    check(near(config.bassBoostDb, 6.0), "config bass");
    check(near(config.stereoWidth, 1.4), "config width");
    check(config.crossfeed == 2, "config crossfeed");
    check(config.compressorEnabled, "config compressor");

    Oran7AudioEffects restored;
    restored.loadFromConfig(config);
    check(restored.enabled(), "restored enabled");
    check(restored.presetId() == customId, "restored preset");
    check(restored.presets().size() == 1, "restored custom preset count");
    check(near(restored.equalizer()->gain(0), 6.0), "restored band");
    check(near(restored.preampDb(), 6.0), "restored preamp");
    check(near(restored.limiterCeilingDb(), -24.0), "restored ceiling");
    check(near(restored.highPassHz(), 120.0), "restored high pass");
    check(near(restored.lowPassHz(), 16000.0), "restored low pass");
    check(near(restored.bassBoostDb(), 6.0), "restored bass");
    check(near(restored.stereoWidth(), 1.4), "restored width");
    check(restored.crossfeed() == 2, "restored crossfeed");
    check(restored.compressorEnabled(), "restored compressor");

    restored.reset();
    check(restored.enabled(), "reset keeps master switch");
    check(near(restored.equalizer()->gain(0), 0.0), "reset flat band");
    check(restored.presetId() == QStringLiteral("custom"), "reset clears active preset");
    check(near(restored.highPassHz(), 20.0), "reset high pass");
    check(near(restored.lowPassHz(), 20000.0), "reset low pass");
    check(near(restored.bassBoostDb(), 0.0), "reset bass");
    check(near(restored.stereoWidth(), 1.0), "reset width");
    check(restored.crossfeed() == 0, "reset crossfeed");
    check(!restored.compressorEnabled(), "reset compressor");
    check(restored.presets().size() == 1, "reset keeps custom presets");

    check(restored.renamePreset(customId, QStringLiteral("改名预设")), "rename custom preset");
    check(restored.presets().at(0).toMap().value(QStringLiteral("label")).toString()
              == QStringLiteral("改名预设"), "renamed label");
    check(restored.deletePreset(customId), "delete custom preset");
    check(restored.presets().isEmpty(), "custom preset removed");
    check(restored.presetId() == QStringLiteral("custom"), "deleted active preset falls back to custom");

    std::printf("audio effects checks passed: %d\n", checks);
    return 0;
}
