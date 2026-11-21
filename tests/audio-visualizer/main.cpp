#include "Oran7AudioSpectrumAnalyzer.h"
#include "Oran7AudioVisualizer.h"
#include "Oran7SpectrumItem.h"

#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QSGGeometryNode>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void check(bool condition, const char* reason)
{
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", reason);
    std::exit(1);
}

class Oran7SpectrumGeometryProbe : public Oran7SpectrumItem
{
public:
    using Oran7SpectrumItem::pulseHeightScale;
    using Oran7SpectrumItem::shapeLeftHeight;
    using Oran7SpectrumItem::updatePaintNode;
};

template<class Predicate>
static bool waitFor(Predicate done)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 3000) QTest::qWait(10);
    return done();
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    constexpr int sampleRate = 48000;
    auto tap = std::make_shared<Oran7AudioSpectrumTap>();
    std::array<std::int16_t, Oran7AudioSpectrumAnalyzer::size * 2> pcm{};
    for (int i = 0; i < Oran7AudioSpectrumAnalyzer::size; ++i) {
        const auto sample = std::int16_t(20000 * std::sin(6.283185307179586 * 1000 * i / sampleRate));
        pcm[2 * i] = pcm[2 * i + 1] = sample;
    }
    Oran7AudioSpectrumTap::Block block;
    tap->push(pcm.data(), int(pcm.size() / 2), 2, sampleRate, tap->epoch());
    check(!tap->pop(block), "disabled tap adds no work to the queue");
    tap->setActive(true);
    const auto epoch = tap->epoch();
    for (unsigned i = 0; i < Oran7AudioSpectrumTap::capacity + 1; ++i)
        tap->push(pcm.data(), int(pcm.size() / 2), 2, sampleRate, epoch);
    Oran7AudioSpectrumAnalyzer analyzer;
    Oran7AudioSpectrumAnalyzer fastAnalyzer(Oran7AudioSpectrumAnalyzer::fastSize);
    unsigned blocks = 0;
    while (tap->pop(block)) {
        check(block.epoch == epoch && block.rate == sampleRate, "queue preserves block metadata");
        analyzer.append(block);
        if (blocks == 0) {
            fastAnalyzer.append(block);
            check(fastAnalyzer.analyze() && fastAnalyzer.band(975, 1025, 1.f) > .85f && !analyzer.analyze(),
                  "1 kHz transient can be displayed before the longer window fills");
        }
        ++blocks;
    }
    check(blocks == Oran7AudioSpectrumTap::capacity && analyzer.analyze(), "full queue drops new samples and FFT runs");
    const float peak = analyzer.band(980, 1020, 1.f);
    check(peak > .85f && analyzer.band(220, 280, 1.f) < .1f && analyzer.band(1800, 2400, 1.f) < .1f,
          "1 kHz signal peaks in the expected frequency range");
    check(analyzer.band(980, 1020, 4.f) > analyzer.band(980, 1020, 2.f),
          "strong peaks remain distinguishable at high sensitivity");
    std::printf("1 kHz band level: %.3f\n", peak);
    tap->invalidate();
    tap->push(pcm.data(), 32, 2, sampleRate, epoch);
    check(!tap->pop(block), "stale seek generation is rejected");
    const float stereo[] = {1.f, -1.f, .25f, .75f};
    tap->push(stereo, 2, 2, sampleRate, tap->epoch());
    check(tap->pop(block) && block.count == 2 && block.samples[0] == 0.f && block.samples[1] == .5f,
          "float PCM is downmixed without altering the device output");

    QFile file(QStringLiteral(APP_ROOT) + "/config/default.json");
    check(file.open(QIODevice::ReadOnly), "open packaged defaults");
    const auto defaults = QJsonDocument::fromJson(file.readAll()).object();
    QJsonObject normalized;
    check(Oran7AppConfig::normalize({}, defaults, &normalized), "legacy profiles gain visualizer defaults");
    const auto initial = Oran7AppConfig::fromJson(normalized).visualizer;
    check(!initial.enabled && initial.barWidth == 4 && initial.barGap == 2, "visualizer starts disabled");
    check(initial.maxHeight == 0, "older settings gain an unlimited height cap by default");
    QJsonObject edits{{"visualizer", QJsonObject{{"opacity", .75}, {"gain", 1.25}, {"smoothing", .55}, {"fps", 24},
                                                 {"bar_width", 500}, {"future", "kept"}}}};
    check(Oran7AppConfig::normalize(edits, defaults, &normalized), "normalize edited settings");
    auto cfg = Oran7AppConfig::fromJson(normalized);
    check(cfg.visualizer.opacity == .75 && cfg.visualizer.gain == 1.25 &&
          cfg.visualizer.barWidth == initial.barWidth &&
          cfg.toJson().value("visualizer").toObject().value("future").toString() == "kept",
          "fractional and unknown settings roundtrip, invalid width uses the default");
    const auto visualizerJson = normalized.value("visualizer").toObject();
    check(visualizerJson.value("smoothing").toDouble() == 1.0 && visualizerJson.value("fps").toInt() == 60,
          "older settings use the fixed smoothing and frame rate");

    Oran7AudioVisualizer visualizer(initial);
    visualizer.setOption("smoothing", .2);
    visualizer.setOption("fps", 30);
    check(visualizer.settings().value("smoothing").toDouble() == 1.0 && visualizer.settings().value("fps").toInt() == 60,
          "runtime edits cannot override the fixed smoothing or frame rate");
    auto savedFrequency = initial;
    savedFrequency.minFrequency = 80;
    savedFrequency.maxFrequency = 12000;
    visualizer.load(savedFrequency);
    check(visualizer.settings().value("min_frequency").toInt() == 200 &&
          visualizer.settings().value("max_frequency").toInt() == 20000,
          "ordinary mode uses the listening preset even with stored custom values");
    visualizer.setOption("min_frequency", 90);
    check(visualizer.config().minFrequency == 80, "ordinary mode ignores edits to the hidden range");
    visualizer.setDeveloperMode(true);
    check(visualizer.settings().value("min_frequency").toInt() == 80 &&
          visualizer.settings().value("max_frequency").toInt() == 12000,
          "developer mode restores the custom range");
    visualizer.setOption("min_frequency", 100);
    visualizer.setDeveloperMode(false);
    check(visualizer.config().minFrequency == 100 && visualizer.settings().value("min_frequency").toInt() == 200,
          "returning to the fixed preset preserves developer edits");
    visualizer.load(initial);
    const auto work = app.primaryScreen()->availableGeometry();
    check(visualizer.overlayGeometry().bottom() == work.bottom() &&
          visualizer.barCount() == (visualizer.overlayGeometry().width() + 2) / 6, "desktop position and bar count");
    visualizer.setOption("height", 800);
    visualizer.setOption("gain", 8);
    visualizer.setOption("max_height", 1280);
    check(Oran7AppConfig::normalize({{"visualizer", visualizer.config().toJson()}}, defaults, &normalized),
          "expanded spectrum settings normalize");
    const auto expanded = Oran7AppConfig::fromJson(normalized).visualizer;
    check(expanded.height == 800 && expanded.gain == 8 && expanded.maxHeight == 1280,
          "the raised limits and the height cap survive saving and reloading");
    visualizer.setOption("max_height", 80);
    check(visualizer.overlayGeometry().height() == 80 && visualizer.overlayGeometry().bottom() == work.bottom(),
          "a height cap reduces the overlay surface without moving its baseline");
    visualizer.setOption("max_height", 1281);
    check(visualizer.config().maxHeight == 80, "out-of-range height caps are rejected");
    visualizer.load(initial);
    visualizer.setOption("height", 88);
    check(visualizer.config().height == 88 && visualizer.overlayGeometry().height() ==
          qRound(88 * Oran7AudioVisualizer::barHeightScale(0)) &&
          visualizer.overlayGeometry().bottom() == work.bottom(), "left peaks use extra height above the fixed bottom edge");
    visualizer.setOption("bar_width", 24);
    check(visualizer.config().barWidth == 24 &&
          visualizer.barCount() == (visualizer.overlayGeometry().width() + 2) / 26, "24 px width updates the bar count");
    check(Oran7AppConfig::normalize({{"visualizer", visualizer.config().toJson()}}, defaults, &normalized) &&
          Oran7AppConfig::fromJson(normalized).visualizer.barWidth == 24, "24 px width survives saving and reloading");
    visualizer.setOption("bar_width", 25);
    check(visualizer.config().barWidth == 24, "width above the new limit is rejected");
    visualizer.setOption("bar_width", 6);
    check(visualizer.barCount() == (visualizer.overlayGeometry().width() + 2) / 8, "width changes update the bar count");
    visualizer.setOption("enabled", true);
    visualizer.setTap(tap);
    visualizer.setPlaying(true, false);
    {
        Oran7SpectrumGeometryProbe spectrum;
        spectrum.setVisualizer(&visualizer);
        spectrum.setSize(visualizer.overlayGeometry().size());
        std::unique_ptr<QSGNode> node(spectrum.updatePaintNode(nullptr, nullptr));
        check(visualizer.showing() && visualizer.levels().isEmpty() && bool(node), "spectrum is ready for silent playback");
        const auto* vertices = static_cast<QSGGeometryNode*>(node.get())->geometry()->vertexDataAsColoredPoint2D();
        for (int i = 0; i < visualizer.barCount(); ++i)
            check(vertices[i * 12 + 1].y - vertices[i * 12].y == 4.f &&
                  vertices[i * 12].a == qRound(255 * visualizer.config().opacity * .38),
                  "silent bars keep a visible, uniform four pixel baseline");
    }
    tap->push(pcm.data(), int(pcm.size() / 2), 2, sampleRate, tap->epoch());
    check(waitFor([&] { return visualizer.showing() && !visualizer.levels().isEmpty(); }), "worker publishes a real PCM frame");
    const auto& levels = visualizer.levels();
    const auto byHeight = [](const Oran7SpectrumBar& a, const Oran7SpectrumBar& b) { return a.height() < b.height(); };
    const int index = int(std::max_element(levels.cbegin(), levels.cend(), byHeight) - levels.cbegin());
    constexpr int bassBars = 9;
    const double lowWidth = double(bassBars) / levels.size();
    const auto scale = &Oran7AudioVisualizer::barHeightScale;
    check(scale(0) == 1.58f && scale(1) > 1.5f && scale(2) > 1.35f &&
          scale(0) > scale(1) && scale(1) > scale(2), "the first three bars retain a high, sloped contour");
    check(scale(2) - scale(3) > scale(0) - scale(2), "the main drop starts after the third bar");
    for (int i = 3; i < 8; ++i) {
        const float drop = scale(i) - scale(i + 1);
        check(drop > 0.f && drop < scale(i - 1) - scale(i), "the contour gradually flattens after the drop");
    }
    for (int i = 8; i < 13; ++i) {
        const float recovery = scale(i + 1) - scale(i);
        check(recovery > 0.f && recovery < .08f, "the contour rejoins ordinary height without a step");
    }
    check(scale(13) == 1.f && scale(14) == 1.f, "ordinary height resumes beyond the left contour");
    const auto shapeLeft = &Oran7SpectrumGeometryProbe::shapeLeftHeight;
    constexpr float unevenHeights[] = {191, 116, 120, 67, 65, 36, 52, 63, 39, 39, 44};
    float previousHeight = unevenHeights[0];
    for (int i = 1; i < int(std::size(unevenHeights)); ++i) {
        const float height = shapeLeft(i, unevenHeights[i], 170.f, 35.f);
        check(height < previousHeight && height > 35.f, "uneven left heights form a descending contour");
        previousHeight = height;
    }
    check(shapeLeft(3, 100.f, 170.f, 35.f) - shapeLeft(3, 67.f, 170.f, 35.f) > 7.f,
          "bars retain more of their own movement inside the contour");
    check(std::abs(shapeLeft(3, 67.f, 35.51f, 35.f) - shapeLeft(3, 67.f, 35.49f, 35.f)) < .2f &&
          std::abs(shapeLeft(3, 67.f, 35.1f, 35.f) - 67.f) < .2f,
          "weak bass fades the contour out continuously without a sudden dip");
    check(shapeLeft(0, 191.f, 170.f, 35.f) == 191.f && shapeLeft(16, 55.f, 170.f, 35.f) == 55.f &&
          shapeLeft(3, 67.f, 30.f, 35.f) == 67.f && shapeLeft(2, 0.f, 0.f, 0.f) == 0.f,
          "the contour preserves the first beat, ordinary bars and weak or silent bass");
    const auto pulseScale = &Oran7SpectrumGeometryProbe::pulseHeightScale;
    check(pulseScale(0, 100, 1.f) == 1.f && pulseScale(90, 100, 1.f) == 1.f &&
          pulseScale(8, 100, 1.f) > pulseScale(49, 100, 1.f) && pulseScale(49, 100, 1.f) > 1.2f &&
          pulseScale(8, 100, 0.f) == 1.f, "music accents lift existing left and middle bars with a smooth fade to the right");
    check(pulseScale(8, 100, -.55f) < pulseScale(49, 100, -.55f) && pulseScale(49, 100, -.55f) < .9f &&
          pulseScale(0, 100, -.55f) == 1.f && pulseScale(90, 100, -.55f) == 1.f,
          "quiet parts lower the left and middle bars while keeping the first and far-right bars independent");
    const auto response = &Oran7AudioVisualizer::barResponse;
    check(response(0.f, 1.f) == 0.f && response(.2f, .2f) < .02f && response(.85f, .85f) > .7f &&
          response(.5f, 1.f) < response(.5f, .5f) && response(1.f, 1.f) == 1.f,
          "quiet levels stay low, strong peaks rise and the threshold adapts after a loud peak");
    QVector<float> bands(40, 0.f), smoothed;
    bands[8] = bands[24] = .16f;
    bands[34] = .82f;
    Oran7AudioVisualizer::smoothBands(bands, bassBars, smoothed);
    check(std::equal(bands.cbegin(), bands.cbegin() + bassBars, smoothed.cbegin()) &&
          smoothed[9] > smoothed[12] && smoothed[12] > 0.f && smoothed[13] == 0.f,
          "the lowest bands stay independent and their neighbors form a short taper");
    check(smoothed[24] < bands[24] && smoothed[20] > 0.f && smoothed[28] > 0.f && smoothed[29] == 0.f &&
          smoothed[34] > .72f && smoothed[33] > 0.f && smoothed[33] < .10f,
          "mid and high bands share energy within four neighbors while strong peaks keep most of their height");
    QVector<float> quiet(7, .02f), strong(7, .62f);
    quiet[3] = .12f;
    strong[3] = .72f;
    Oran7AudioVisualizer::smoothBands(quiet, 0, smoothed);
    const float quietReduction = quiet[3] - smoothed[3];
    Oran7AudioVisualizer::smoothBands(strong, 0, smoothed);
    check(quietReduction > .02f && strong[3] - smoothed[3] < quietReduction * .3f,
          "similar neighbors influence strong bars much less than quiet bars");
    const double expectedPosition = lowWidth + (1 - lowWidth) * std::log(1000.0 / 500) / std::log(20000.0 / 500);
    check(std::abs(double(index) / levels.size() - expectedPosition) < .04, "1 kHz follows the expanded mid and high region");
    std::array<std::int16_t, 1536 * 2> quietTone{};
    for (std::size_t j = 0; j < quietTone.size(); ++j) quietTone[j] = pcm[j] / 6;
    for (int chunk = 0; chunk < 16; ++chunk) {
        tap->push(quietTone.data(), 1536, 2, sampleRate, tap->epoch());
        QTest::qWait(32);
    }
    const auto sustained = visualizer.levels().value(index);
    check(sustained.sustained > .04f && sustained.height() < .28f && sustained.transient < .08f,
          "a steady midrange tone settles into a modest sustained height");
    tap->push(pcm.data(), int(pcm.size() / 2), 2, sampleRate, tap->epoch());
    check(waitFor([&] { return visualizer.levels().value(index).transient > .125f; }) &&
          visualizer.levels().value(index).transient < .4f && visualizer.levels().value(index).sustained < .3f,
          "a sudden increase creates a moderated midrange transient");
    {
        Oran7SpectrumGeometryProbe spectrum;
        spectrum.setVisualizer(&visualizer);
        const auto renderedHeights = [&] {
            spectrum.setSize(visualizer.overlayGeometry().size());
            std::unique_ptr<QSGNode> node(spectrum.updatePaintNode(nullptr, nullptr));
            const auto* vertices = static_cast<QSGGeometryNode*>(node.get())->geometry()->vertexDataAsColoredPoint2D();
            QVector<float> result(visualizer.barCount());
            for (int i = 0; i < result.size(); ++i) result[i] = vertices[i * 12 + 1].y - vertices[i * 12 + 6].y;
            return result;
        };
        const auto originalHeights = renderedHeights();
        visualizer.setOption("max_height", 12);
        const auto limitedHeights = renderedHeights();
        check(originalHeights[index] > 12.f, "a visible peak exceeds the chosen height cap");
        for (int i = 0; i < originalHeights.size(); ++i)
            check(std::abs(limitedHeights[i] - std::min(12.f, originalHeights[i])) < .01f,
                  "the height cap clips only the portion above the limit");
        visualizer.setOption("max_height", 0);
    }
    auto maximum = [&] {
        const auto& bars = visualizer.levels();
        return bars.isEmpty() ? 0.f : std::max_element(bars.cbegin(), bars.cend(), byHeight)->height();
    };
    const float beforePause = maximum();
    int decayFrames = 0;
    const auto decay = QObject::connect(&visualizer, &Oran7AudioVisualizer::frameReady, [&] { ++decayFrames; });
    visualizer.setPlaying(false, false);
    check(visualizer.showing(), "pause preserves the last frame for decay");
    QTest::qWait(175);
    check(decayFrames >= 4 && maximum() < beforePause * .4f,
          "the display continues animating between PCM updates with a quicker release");
    QObject::disconnect(decay);
    check(waitFor([&] { return !visualizer.showing() && visualizer.levels().isEmpty(); }), "paused spectrum decays");
    const auto feedTone = [&](double frequency, int amplitude, int chunks) {
        for (int frame = 0; frame < Oran7AudioSpectrumAnalyzer::size; ++frame) {
            const auto sample = std::int16_t(amplitude * std::sin(6.283185307179586 * frequency * frame / sampleRate));
            pcm[2 * frame] = pcm[2 * frame + 1] = sample;
        }
        for (int chunk = 0; chunk < chunks; ++chunk) {
            tap->push(pcm.data(), int(pcm.size() / 2), 2, sampleRate, tap->epoch());
            QTest::qWait(43);
        }
    };
    constexpr int bassBins[bassBars] = {9, 10, 12, 13, 15, 16, 18, 19, 21};
    std::array<std::array<float, bassBars>, bassBars> bassResponses{};
    for (int band = 0; band < bassBars; ++band) {
        visualizer.setPlaying(true, true);
        check(visualizer.levels().isEmpty(), "seek clears the visible old spectrum");
        feedTone(double(bassBins[band] * sampleRate) / Oran7AudioSpectrumAnalyzer::size, 20000, 8);
        for (int i = 0; i < bassBars; ++i) bassResponses[band][i] = visualizer.levels().value(i).sustained;
    }
    for (int band = 0; band < bassBars; ++band) {
        check(bassResponses[band][band] > .24f, "each bass tone builds a sustained bar at normal gain");
        for (int other = 0; other < bassBars; ++other) {
            if (other != band)
                check(bassResponses[band][band] > bassResponses[other][band] + .02f,
                      "each lowest bass band responds most to its own tone");
        }
    }
    const float previousBass = visualizer.levels().value(bassBars - 1).sustained;
    QTest::qWait(100);
    check(visualizer.levels().value(bassBars - 1).sustained > previousBass * .90f,
          "a short gap between analysis frames does not drop the neighboring bass bodies");
    feedTone(0, 0, 7);
    const float releasedBass = visualizer.levels().value(bassBars - 1).sustained;
    check(releasedBass > previousBass * .36f && releasedBass < previousBass * .85f,
          "a bass bar carries through a short pause and then falls");
    visualizer.setPlaying(true, true);
    const double bassFrequency = double(bassBins[0] * sampleRate) / Oran7AudioSpectrumAnalyzer::size;
    feedTone(bassFrequency, 1500, 10);
    float beat = 0.f, neighboringBeat = 0.f;
    const auto beatFrames = QObject::connect(&visualizer, &Oran7AudioVisualizer::frameReady, [&] {
        beat = std::max(beat, visualizer.levels().value(0).transient);
        for (int i = 1; i < bassBars; ++i)
            neighboringBeat = std::max(neighboringBeat, visualizer.levels().value(i).transient);
    });
    feedTone(bassFrequency, 20000, 1);
    feedTone(bassFrequency, 1500, 5);
    QObject::disconnect(beatFrames);
    check(neighboringBeat < beat * .65f, "only the first bar accents the bass beat");
    check(beat > .24f && visualizer.levels().value(0).transient < beat * .4f,
          "a short bass beat rises above the sustained tone and releases promptly");
    visualizer.setOption("gain", 4);
    visualizer.setPlaying(true, true);
    const int rightStart = visualizer.barCount() * 19 / 20;
    const auto rightMaximum = [&] {
        float maximum = 0.f;
        for (int i = rightStart; i < visualizer.levels().size(); ++i)
            maximum = std::max(maximum, visualizer.levels()[i].height());
        return maximum;
    };
    feedTone(984.375, 20000, 7);
    check(rightMaximum() == 0.f, "a midrange tone does not move the treble bars");
    visualizer.setPlaying(true, true);
    const int bars = visualizer.barCount();
    const int sourceBar = qRound((bars - 1) * .85);
    const double sourceHz = 500.0 * std::pow(40.0, (sourceBar + .5 - bassBars) / (bars - bassBars));
    const double fftBinHz = double(sampleRate) / Oran7AudioSpectrumAnalyzer::fastSize;
    feedTone(qRound(sourceHz / fftBinHz) * fftBinHz, 1200, 8);
    check(visualizer.levels().size() == bars && visualizer.levels().back().height() > .006f &&
          visualizer.levels().back().height() < .045f,
          "the last columns receive a small share from the high frequencies to their left");
    visualizer.setPlaying(true, true);
    feedTone(14531.25, 650, 7);
    check(visualizer.levels().size() == visualizer.barCount() &&
          visualizer.levels().back().height() > .005f && visualizer.levels().back().height() < .025f,
          "a nearby high frequency moves the last bar gently when the highest frequencies are absent");
    visualizer.setPlaying(true, true);
    feedTone(18515.625, 160, 7);
    const float quietRight = rightMaximum();
    check(quietRight > .004f && quietRight < .025f,
          "a weak high frequency gives the far right a small amount of motion");
    feedTone(18515.625, 650, 7);
    check(rightMaximum() > quietRight + .006f && rightMaximum() < .075f,
          "a modest high tone adds some motion above the low baseline");
    feedTone(18515.625, 18000, 7);
    check(rightMaximum() > .075f && rightMaximum() < .25f,
          "strong high frequencies can still rise modestly above the right baseline");
    visualizer.setPlaying(true, true);
    check(visualizer.musicPulse() == 0.f, "a playback discontinuity clears the music accent");
    feedTone(984.375, 6000, 10);
    check(std::abs(visualizer.musicPulse()) < .08f, "a steady part remains visually calm");
    feedTone(984.375, 7900, 3);
    const float accent = visualizer.musicPulse();
    feedTone(984.375, 3500, 6);
    check(accent > .38f && visualizer.musicPulse() < -.20f,
          "moderate changes in loudness move the left and middle motion both up and down");
    visualizer.setPlaying(false, false);
    visualizer.setOption("enabled", false);

    QQmlPropertyMap theme;
    theme.insert("Oran7MainGUI", QVariantMap{{"themeColor", QColor("#80beff")}});
    qmlRegisterSingletonInstance("Oran7UI.Impl", 1, 0, "Oran7Theme", &theme);
    qmlRegisterUncreatableType<Oran7AudioVisualizer>("Client", 1, 0, "Oran7AudioVisualizer", "由测试持有");
    qmlRegisterType<Oran7SpectrumItem>("Client", 1, 0, "Oran7SpectrumItem");
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(APP_ROOT) +
        "/src/qml/Components/Oran7GlobalComponents/Oran7AudioVisualizerWindow.qml"));
    if (component.isError()) std::fprintf(stderr, "%s", qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({{"visualizer", QVariant::fromValue(&visualizer)}}));
    auto* overlay = qobject_cast<QWindow*>(object.get());
    check(overlay && (overlay->flags() & Qt::WindowTransparentForInput) &&
          (overlay->flags() & Qt::WindowDoesNotAcceptFocus) && !overlay->isVisible() &&
          overlay->geometry() == visualizer.overlayGeometry(),
          "production overlay has passthrough flags and stays hidden while disabled");
    visualizer.shutdown();
}
