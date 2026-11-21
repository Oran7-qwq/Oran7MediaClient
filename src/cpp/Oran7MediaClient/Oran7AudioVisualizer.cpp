#include "Oran7AudioVisualizer.h"
#include "Oran7AudioSpectrumAnalyzer.h"

#include <QGuiApplication>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <windows.h>

using Clock = std::chrono::steady_clock;

namespace {
constexpr double lowBandLimit = 500.0;
constexpr double lowBandWidthScale = .60;
constexpr int maxBassBars = 9;
constexpr float sustainedHeight = .46f;
constexpr float bassSustainedHeight = .60f;
constexpr float bassTransientHeight = .88f;
constexpr float transientHeight = .44f;

float updateEnvelope(float level, float& sustained, float rise, float fall, bool accented)
{
    const float threshold = accented ? (.055f + .07f * sustained) * .70f : .044f + .06f * sustained;
    const float excess = std::max(0.f, level - sustained - threshold);
    const float transient = 1.8f * excess / (1.f + excess);
    sustained += (level - sustained) * (level > sustained ? rise : fall);
    return transient;
}

int bassBarCount(int count, int low, int high, int rate)
{
    const double fraction = lowBandWidthScale * std::log(lowBandLimit / low) / std::log(double(high) / low);
    // 每个独立低频柱至少留出一个 FFT 频点的宽度。
    const int resolved = int((lowBandLimit - low) * Oran7AudioSpectrumAnalyzer::size / rate);
    return std::min({maxBassBars, count - 1, std::max(1, qRound(count * fraction)), std::max(1, resolved)});
}

float smoothRamp(float position, float start, float end)
{
    const float t = std::clamp((position - start) / (end - start), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

void shareTrebleTail(QVector<Oran7SpectrumBar>& targets)
{
    const int count = int(targets.size());
    const int tailBars = std::min(8, count / 10);
    constexpr int radius = 2;
    const int firstTail = count - tailBars;
    const int offset = std::max(tailBars + radius, qRound(count * .15));
    for (int i = firstTail; i < count; ++i) {
        float sustained = 0.f, transient = 0.f;
        const int center = i - offset;
        for (int j = center - radius; j <= center + radius; ++j) {
            sustained += targets[j].sustained;
            transient += targets[j].transient;
        }
        // 最右几柱可少量跟随左边高频，相邻采样窗错开，较强的本频段仍优先。
        const float position = float(i - firstTail + 1) / tailBars;
        const float amount = .52f * smoothRamp(position, 0.f, 1.f) / (2 * radius + 1);
        auto& bar = targets[i];
        bar.sustained = std::max(bar.sustained, std::min(.025f, sustained * amount));
        bar.transient = std::max(bar.transient, std::min(.022f, transient * amount));
    }
}
}

struct Oran7AudioVisualizer::Worker
{
    std::mutex mutex;
    std::condition_variable changed;
    std::shared_ptr<Oran7AudioSpectrumTap> tap;
    Oran7VisualizerConfig config;
    QVector<Oran7SpectrumBar> latest;
    Clock::time_point computedAt{};
    quint64 revision = 0, frame = 0, consumed = 0;
    int bars = 0;
    float latestMusicPulse = 0.f;
    bool latestBassAccent = false;
    bool stopping = false;
    std::thread thread;

    void run()
    {
        constexpr int hop = 512;
        Oran7AudioSpectrumAnalyzer bass;
        Oran7AudioSpectrumAnalyzer fast(Oran7AudioSpectrumAnalyzer::fastSize);
        std::shared_ptr<Oran7AudioSpectrumTap> previous;
        Oran7AudioSpectrumTap::Block block;
        block.count = 0;
        int offset = 0;
        std::uint32_t epoch = 0;
        bool primed = false;
        bool loudnessReady = false;
        float shortLoudness = 0.f, averageLoudness = 0.f;
        QVector<float> sustained, transients, smoothed, edges, recentPeaks, rightFade, trebleLift, trebleOverlap;
        QVector<Oran7SpectrumBar> targets;
        auto lastAnalysis = Clock::time_point{};
        int low = 0, high = 0, mappedRate = 0, bassBars = 0;
        auto nextHop = Clock::time_point{};
        std::unique_lock<std::mutex> lock(mutex);
        while (!stopping) {
            if (!config.enabled || bars <= 0 || !tap || !(tap->epoch() & 1u)) {
                const auto current = revision;
                changed.wait(lock, [&] { return stopping || revision != current; });
                continue;
            }
            const auto source = tap;
            const auto settings = config;
            const auto current = revision;
            const int count = bars;
            lock.unlock();
            const auto started = Clock::now();
            const auto generation = source->epoch();
            const bool discontinuity = previous != source || epoch != generation;
            if (discontinuity) {
                bass.reset();
                fast.reset();
                offset = block.count = 0;
                primed = false;
                loudnessReady = false;
                nextHop = started;
                recentPeaks.fill(0);
                sustained.fill(0);
                lastAnalysis = {};
                previous = source;
                epoch = generation;
            }
            if (started < nextHop) {
                lock.lock();
                changed.wait_until(lock, nextHop, [&] { return stopping || revision != current; });
                continue;
            }
            int remaining = primed ? hop : Oran7AudioSpectrumAnalyzer::fastSize;
            int appended = 0, rate = 0;
            float sampleEnergy = 0.f;
            // 分时分析同一次回调交付的样本，保留设备缓冲内的短促变化。
            for (unsigned reads = 0; (generation & 1u) && remaining > 0;) {
                if (offset == block.count) {
                    if (reads++ >= Oran7AudioSpectrumTap::capacity || !source->pop(block)) break;
                    offset = 0;
                }
                if (block.epoch != generation) {
                    offset = block.count;
                    continue;
                }
                const int take = std::min(remaining, block.count - offset);
                rate = block.rate;
                const auto* samples = block.samples.data() + offset;
                fast.append(samples, take, rate);
                bass.append(samples, take, rate);
                for (int i = 0; i < take; ++i) {
                    const float sample = std::isfinite(samples[i]) ? std::clamp(samples[i], -1.f, 1.f) : 0.f;
                    sampleEnergy += sample * sample;
                }
                remaining -= take;
                appended += take;
                offset += take;
            }
            const bool ready = appended > 0 && fast.analyze();
            if (appended) {
                const int advance = primed ? appended : std::min(appended, hop);
                nextHop += std::chrono::microseconds(1000000LL * advance / rate);
            } else {
                nextHop = started + std::chrono::milliseconds(4);
            }
            primed |= ready;
            const bool remapped = ready && (edges.size() != count + 1 || low != settings.minFrequency ||
                                            high != settings.maxFrequency || mappedRate != rate);
            if (remapped) {
                low = settings.minFrequency;
                high = settings.maxFrequency;
                if (mappedRate != rate) loudnessReady = false;
                mappedRate = rate;
                edges.resize(count + 1);
                recentPeaks.fill(0, count);
                sustained.fill(0, count);
                transients.resize(count);
                bassBars = bassBarCount(count, low, high, rate);
                const double split = bassBars > 0 ? lowBandLimit : low;
                edges[0] = float(low);
                for (int i = 1; i <= bassBars; ++i)
                    edges[i] = float(low + (split - low) * i / bassBars);
                for (int i = bassBars + 1; i <= count; ++i) {
                    const double position = double(i - bassBars) / (count - bassBars);
                    edges[i] = float(split * std::pow(high / split, position));
                }
                rightFade.resize(count);
                trebleLift.resize(count);
                trebleOverlap.resize(count);
                for (int i = 0; i < count; ++i) {
                    const float position = float(i) / std::max(1, count - 1);
                    rightFade[i] = smoothRamp(position, .50f, .92f);
                    trebleLift[i] = smoothRamp(position, .66f, .94f);
                    trebleOverlap[i] = smoothRamp(position, .88f, 1.f);
                }
            }
            float musicPulse = 0.f;
            if (ready) {
                const float rms = std::sqrt(sampleEnergy / appended);
                const float seconds = float(appended) / rate;
                if (!loudnessReady) {
                    shortLoudness = averageLoudness = rms;
                    loudnessReady = true;
                }
                // 以短时响度相对近期平均值的变化表现整段音乐的起落。
                const float responseTime = rms > shortLoudness ? .022f : .066f;
                shortLoudness += (rms - shortLoudness) * (1.f - std::exp(-seconds / responseTime));
                const float lift = (shortLoudness - averageLoudness) / std::max(.035f, averageLoudness);
                averageLoudness += (rms - averageLoudness) * (1.f - std::exp(-seconds / .58f));
                // 音乐转弱时也收低柱高，给下一次抬升留出空间。
                musicPulse = smoothRamp(lift, .028f, .44f) - .55f * smoothRamp(-lift, .045f, .45f);
                const bool bassReady = bassBars > 0 && bass.analyze();
                targets.resize(count);
                const float gain = float(settings.gain);
                const float elapsed = lastAnalysis == Clock::time_point{}
                    ? float(appended) / rate : std::chrono::duration<float>(started - lastAnalysis).count();
                const float retention = std::exp(-elapsed / .24f);
                const float rise = 1.f - std::exp(-elapsed / .12f);
                const float fall = 1.f - std::exp(-elapsed / .17f);
                const float bassRise = 1.f - std::exp(-elapsed / .095f);
                const float bassFall = 1.f - std::exp(-elapsed / .27f);
                lastAnalysis = started;
                for (int i = 0; i < count; ++i) {
                    const bool leadingBass = i < bassBars;
                    const bool accented = leadingBass && i == 0;
                    const float level = (leadingBass && bassReady ? bass : fast).band(edges[i], edges[i + 1], gain);
                    auto& peak = recentPeaks[i];
                    peak = std::max(level, peak * retention);
                    const float response = barResponse(level, peak);
                    float trebleLevel = level;
                    if (const float overlap = trebleOverlap[i]; overlap > 0.f) {
                        // 音源缺少极高频时，最右几柱也可轻微响应邻近的高频。
                        const float nearby = fast.band(edges[i] * (1.f - .34f * overlap), edges[i], gain);
                        trebleLevel = std::max(level, nearby * .68f * overlap);
                    }
                    const float weakTreble = trebleLift[i] * std::max(0.f, trebleLevel * .34f - response);
                    transients[i] = updateEnvelope(response + weakTreble, sustained[i],
                                                   accented ? bassRise : rise, accented ? bassFall : fall, accented);
                }
                smoothBands(sustained, bassBars, smoothed);
                for (int i = 0; i < count; ++i)
                    targets[i].sustained = smoothed[i];
                smoothBands(transients, bassBars, smoothed);
                for (int i = 0; i < count; ++i) {
                    const bool accented = i == 0 && bassBars > 0;
                    const float fade = rightFade[i];
                    const float quietDamping = .80f * fade - .34f * trebleLift[i];
                    const auto taper = [fade, quietDamping](float level) {
                        const float quietness = 1.f - std::min(level / .24f, 1.f);
                        return level * (1.f - .60f * fade) * (1.f - quietDamping * quietness);
                    };
                    targets[i].sustained = taper(targets[i].sustained) * (accented ? bassSustainedHeight : sustainedHeight);
                    const float accentHeight = accented ? bassTransientHeight : transientHeight + .05f * (1.f - fade);
                    targets[i].transient = taper(smoothed[i]) * accentHeight;
                }
                shareTrebleTail(targets);
            }
            lock.lock();
            if ((ready || discontinuity) && revision == current && source->epoch() == generation) {
                // 仅最左柱在绘制帧之间保留鼓点的短峰值。
                if (ready && bassBars > 0 && !discontinuity && !remapped && frame != consumed && frame - consumed <= 2 &&
                    started - computedAt < std::chrono::milliseconds(26) && latest.size() == count) {
                    targets.front().transient = std::max(latest.constFirst().transient, targets.front().transient);
                }
                latest = ready ? targets : QVector<Oran7SpectrumBar>{};
                latestMusicPulse = musicPulse;
                latestBassAccent = ready && bassBars > 0;
                computedAt = started;
                ++frame;
            }
            changed.wait_until(lock, nextHop, [&] { return stopping || revision != current; });
        }
    }
};

Oran7AudioVisualizer::Oran7AudioVisualizer(const Oran7VisualizerConfig& defaults, QObject* parent)
    : QObject(parent), m_worker(std::make_unique<Worker>()), m_defaults(defaults), m_config(defaults)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Oran7AudioVisualizer::publish);
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &Oran7AudioVisualizer::refreshLayout);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &Oran7AudioVisualizer::refreshLayout);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] {
        QTimer::singleShot(0, this, &Oran7AudioVisualizer::refreshLayout);
    });
}

Oran7AudioVisualizer::~Oran7AudioVisualizer() { shutdown(); }

Oran7VisualizerConfig Oran7AudioVisualizer::effectiveConfig() const
{
    auto settings = m_config;
    if (!m_developerMode) {
        settings.minFrequency = m_defaults.minFrequency;
        settings.maxFrequency = m_defaults.maxFrequency;
    }
    return settings;
}

float Oran7AudioVisualizer::barHeightScale(int index)
{
    // 前三柱留出高度，再向右逐步收低，最后接回普通柱高。
    constexpr int transitionBars = 13;
    constexpr float heightScales[transitionBars] = {
        1.58f, 1.55f, 1.42f, 1.00f, .94f, .89f, .85f, .83f, .82f, .87f, .92f, .96f, .99f
    };
    return index >= 0 && index < transitionBars ? heightScales[index] : 1.f;
}

float Oran7AudioVisualizer::barResponse(float level, float recentPeak)
{
    // 强峰出现后暂时抬高该频段的阈值，收住较弱的尾音。
    const float activity = std::clamp((recentPeak - .55f) / .45f, 0.f, 1.f);
    const float threshold = .30f + .15f * activity;
    const float atThreshold = .10f * threshold;
    if (level <= threshold) {
        const float relative = level / threshold;
        return atThreshold * relative * relative;
    }
    const float relative = (level - threshold) / (1.f - threshold);
    return atThreshold + (1.f - atThreshold) * relative * (.82f + .18f * relative);
}

void Oran7AudioVisualizer::smoothBands(const QVector<float>& levels, int bassBars, QVector<float>& output)
{
    const int count = int(levels.size());
    bassBars = std::clamp(bassBars, 0, count);
    output.resize(count);
    for (int i = 0; i < bassBars; ++i) output[i] = levels[i];
    for (int i = bassBars; i < count; ++i) {
        // 中段弱音分摊到左右四柱，和独立低频的交界也做同样的过渡。
        constexpr int radius = 4;
        const int first = std::max(i - radius, 0);
        const int last = std::min(i + radius, count - 1);
        const float center = levels[i];
        // 距离越近、强度越相近，参与平均的权重越高。
        float sum = 0.f, weightSum = 0.f;
        for (int j = first; j <= last; ++j) {
            const float similarity = .34f + .66f * std::max(0.f, 1.f - std::abs(levels[j] - center) / .32f);
            const float weight = similarity * (radius + 1 - std::abs(j - i));
            sum += levels[j] * weight;
            weightSum += weight;
        }
        const float t = std::clamp((center - .18f) / .37f, 0.f, 1.f);
        const float mix = .88f - .70f * t * t * (3.f - 2.f * t);
        output[i] = center + (sum / weightSum - center) * mix;
    }
}

void Oran7AudioVisualizer::setDeveloperMode(bool enabled)
{
    if (m_developerMode == enabled) return;
    m_developerMode = enabled;
    if (m_config.minFrequency != m_defaults.minFrequency || m_config.maxFrequency != m_defaults.maxFrequency)
        refreshCapture(true);
    emit settingsChanged();
}

void Oran7AudioVisualizer::load(const Oran7VisualizerConfig& config)
{
    m_config = config;
    refreshLayout();
    emit settingsChanged();
}

void Oran7AudioVisualizer::setOption(const QString& key, const QVariant& value)
{
    if (!m_developerMode && (key == QStringLiteral("min_frequency") || key == QStringLiteral("max_frequency")))
        return;
    const auto before = m_config.toJson();
    if (!before.contains(key)) return;
    auto after = before;
    after[key] = QJsonValue::fromVariant(value);
    const auto checked = Oran7VisualizerConfig::normalize(after, before);
    if (checked == before) return;
    load(Oran7VisualizerConfig::fromJson(checked));
    emit edited();
}

void Oran7AudioVisualizer::setTap(std::shared_ptr<Oran7AudioSpectrumTap> tap)
{
    if (!m_worker || m_tap == tap) return;
    if (m_tap) m_tap->setActive(false);
    m_tap = std::move(tap);
    refreshCapture(bool(m_tap));
}

void Oran7AudioVisualizer::setPlaying(bool playing, bool reset)
{
    m_playing = playing;
    refreshCapture(reset);
}

void Oran7AudioVisualizer::setHostWindow(QWindow* window)
{
    if (m_host == window) return;
    if (m_host) disconnect(m_host, nullptr, this, nullptr);
    m_host = window;
    if (m_host) {
        connect(m_host, &QWindow::screenChanged, this, &Oran7AudioVisualizer::refreshLayout);
        connect(m_host, &QObject::destroyed, this, &Oran7AudioVisualizer::refreshLayout);
    }
    refreshLayout();
}

void Oran7AudioVisualizer::placeOverlay(QWindow* window)
{
    if (!window || !m_screen || m_geometry.isEmpty()) return;
    // 先选定目标屏，再一次性设定逻辑几何，避免跨屏时用错 DPI。
    if (window->screen() != m_screen) window->setScreen(m_screen);
    if (window->geometry() != m_geometry) window->setGeometry(m_geometry);
}

void Oran7AudioVisualizer::prepareOverlay(QWindow* window)
{
    if (!window || QGuiApplication::platformName() != QStringLiteral("windows")) return;
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    const auto style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    SetWindowPos(hwnd, m_config.alwaysOnTop ? HWND_TOPMOST : HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void Oran7AudioVisualizer::refreshLayout()
{
    auto* screen = m_config.screen == QStringLiteral("mainWindow") && m_host ? m_host->screen() : nullptr;
    if (!screen) screen = QGuiApplication::primaryScreen();
    const bool screenChanged = screen != m_screen;
    if (screenChanged) {
        if (m_screen) disconnect(m_screen, nullptr, this, nullptr);
        m_screen = screen;
        if (m_screen) {
            connect(m_screen, &QScreen::geometryChanged, this, &Oran7AudioVisualizer::refreshLayout);
            connect(m_screen, &QScreen::availableGeometryChanged, this, &Oran7AudioVisualizer::refreshLayout);
            connect(m_screen, &QScreen::logicalDotsPerInchChanged, this, &Oran7AudioVisualizer::refreshLayout);
        }
    }
    QRect geometry;
    int bars = 0;
    int uncappedHeight = 0;
    if (screen && m_config.barWidth > 0) {
        const auto work = screen->availableGeometry();
        const int bottom = std::max(work.top() + 1, work.bottom() + 1 - m_config.bottomOffset);
        const auto heightScale = barHeightScale(0);
        uncappedHeight = std::min(qRound(m_config.height * heightScale), bottom - work.top());
        const int height = m_config.maxHeight > 0 ? std::min(uncappedHeight, m_config.maxHeight) : uncappedHeight;
        geometry = QRect(work.x(), bottom - height, work.width(), height);
        bars = std::max(1, (geometry.width() + m_config.barGap) / (m_config.barWidth + m_config.barGap));
    }
    if (screenChanged || geometry != m_geometry || bars != m_barCount || uncappedHeight != m_uncappedOverlayHeight) {
        m_geometry = geometry;
        m_barCount = bars;
        m_uncappedOverlayHeight = uncappedHeight;
        emit layoutChanged();
    }
    refreshCapture();
}

void Oran7AudioVisualizer::refreshCapture(bool reset)
{
    if (!m_worker) return;
    const bool enabled = m_config.enabled && m_barCount > 0;
    const bool sampling = enabled && m_playing && m_tap;
    if (m_tap) {
        if (reset && sampling) m_tap->invalidate();
        m_tap->setActive(sampling);
    }
    {
        std::lock_guard<std::mutex> lock(m_worker->mutex);
        m_worker->config = effectiveConfig();
        m_worker->tap = m_tap;
        m_worker->bars = m_barCount;
        ++m_worker->revision;
        if (reset || !sampling) {
            m_worker->latest.clear();
            m_worker->latestMusicPulse = 0.f;
            m_worker->latestBassAccent = false;
            m_frame = m_worker->consumed = ++m_worker->frame;
        }
    }
    m_worker->changed.notify_one();
    if (enabled && !m_worker->thread.joinable())
        m_worker->thread = std::thread([worker = m_worker.get()] { worker->run(); });
    if (reset || !sampling || m_target.size() != m_barCount) {
        m_target.clear();
        m_targetMusicPulse = 0.f;
        m_targetBassAccent = false;
        m_targetAt = {};
    }
    if (enabled && (sampling || !m_levels.isEmpty())) {
        if (!m_timer.isActive()) {
            m_animationClock.start();
            m_nextFrameAt = Clock::now();
            scheduleNextFrame();
        }
    } else {
        m_timer.stop();
        m_animationClock.invalidate();
    }
    if (reset || !enabled) {
        m_musicPulse = 0.f;
        if (!m_levels.isEmpty()) {
            m_levels.clear();
            emit frameReady();
        }
    }
    updateShowing();
}

void Oran7AudioVisualizer::scheduleNextFrame()
{
    using FramePeriod = std::chrono::duration<int, std::ratio<1, Oran7VisualizerConfig::kFps>>;
    constexpr auto interval = std::chrono::duration_cast<Clock::duration>(FramePeriod{1});
    const auto now = Clock::now();
    m_nextFrameAt += interval;
    // 按绝对时刻安排 60 Hz 更新，以毫秒定时，延迟时跳过已错过的帧。
    if (m_nextFrameAt <= now) m_nextFrameAt += interval * ((now - m_nextFrameAt) / interval + 1);
    m_timer.start(std::chrono::ceil<std::chrono::milliseconds>(m_nextFrameAt - now));
}

void Oran7AudioVisualizer::publish()
{
    scheduleNextFrame();
    {
        std::lock_guard<std::mutex> lock(m_worker->mutex);
        if (m_frame != m_worker->frame) {
            m_frame = m_worker->consumed = m_worker->frame;
            m_target = m_worker->latest;
            m_targetMusicPulse = m_worker->latestMusicPulse;
            m_targetBassAccent = m_worker->latestBassAccent;
            m_targetAt = m_worker->computedAt;
        }
    }
    const double dt = m_animationClock.nsecsElapsed() * 1e-9;
    m_animationClock.start();
    const auto targetAge = Clock::now() - m_targetAt;
    const bool matchingTarget = m_target.size() == m_barCount;
    const bool fresh = matchingTarget && targetAge < std::chrono::milliseconds(80);
    const bool holdBass = m_playing && m_tap;
    // 给邻近低频的持续高度留出短暂交接时间，防止分析帧迟到时先掉底再跳回。
    const bool keepLeftBody = !fresh && holdBass && matchingTarget && targetAge < std::chrono::milliseconds(135);
    if (m_levels.isEmpty() && !fresh) {
        if (!m_playing || !m_tap) m_timer.stop();
        return;
    }
    const bool hadEnergy = !m_levels.isEmpty();
    if (m_levels.size() != m_barCount) m_levels.fill({}, m_barCount);
    const auto blend = [dt](double seconds) { return float(1 - std::exp(-dt / seconds)); };
    const float sustainedAttack = blend(.052);
    const float transientAttack = blend(.027);
    constexpr double smoothing = Oran7VisualizerConfig::kSmoothing;
    const float idleRelease = blend(.045 + smoothing * .09);
    const float transientRelease = fresh ? blend(.102 + smoothing * .07) : idleRelease;
    const float sustainedRelease = fresh ? blend(.095 + smoothing * .095) : idleRelease;
    const float bassSustainedAttack = blend(.055);
    const float bassSustainedRelease = holdBass ? blend(.17) : idleRelease;
    const float bassTransientAttack = blend(.008);
    const float bassTransientRelease = holdBass ? blend(.09) : idleRelease;
    const auto approach = [](float current, float target, float attack, float release) {
        const float next = current + (target - current) * (target > current ? attack : release);
        // 只有当前高度和目标都很低时才归零，微弱的上升可以逐帧累积。
        return target < .002f && next < .002f ? 0.f : next;
    };
    const float targetPulse = fresh || keepLeftBody ? m_targetMusicPulse : 0.f;
    const bool pulseGrowing = std::abs(targetPulse) > std::abs(m_musicPulse) || targetPulse * m_musicPulse < 0.f;
    m_musicPulse += (targetPulse - m_musicPulse) * blend(pulseGrowing ? .024 : .092);
    if (std::abs(targetPulse) < .002f && std::abs(m_musicPulse) < .002f) m_musicPulse = 0.f;
    bool energy = false;
    for (int i = 0; i < m_levels.size(); ++i) {
        const auto target = fresh ? m_target[i] : Oran7SpectrumBar{};
        auto& bar = m_levels[i];
        const bool accented = i == 0 && m_targetBassAccent;
        const float targetBody = keepLeftBody && i > 0 && i < maxBassBars ? m_target[i].sustained : target.sustained;
        bar.sustained = approach(bar.sustained, targetBody, accented ? bassSustainedAttack : sustainedAttack,
                                 accented ? bassSustainedRelease : sustainedRelease);
        bar.transient = approach(bar.transient, target.transient, accented ? bassTransientAttack : transientAttack,
                                 accented ? bassTransientRelease : transientRelease);
        energy |= bar.sustained > 0 || bar.transient > 0;
    }
    if (!energy) {
        m_levels.clear();
        m_musicPulse = 0.f;
        if (!m_playing || !m_tap) m_timer.stop();
    }
    if (energy || hadEnergy) emit frameReady();
    updateShowing();
}

void Oran7AudioVisualizer::updateShowing()
{
    const bool showing = m_config.enabled && !m_geometry.isEmpty() &&
        ((m_playing && m_tap) || !m_levels.isEmpty()) && m_worker;
    if (m_showing == showing) return;
    m_showing = showing;
    emit showingChanged();
}

void Oran7AudioVisualizer::shutdown()
{
    if (!m_worker) return;
    if (m_tap) m_tap->setActive(false);
    m_tap.reset();
    m_timer.stop();
    {
        std::lock_guard<std::mutex> lock(m_worker->mutex);
        m_worker->stopping = true;
    }
    m_worker->changed.notify_one();
    if (m_worker->thread.joinable()) m_worker->thread.join();
    m_worker.reset();
    m_levels.clear();
    m_musicPulse = 0.f;
    updateShowing();
}
