#include "Oran7Theme.h"
#include "Oran7Theme_p.h"
#include "Oran7ThemeEvaluator_p.h"
#include "Oran7ThemeProfileManager.h"
#include "Oran7ThemeProfile.h"
#include "Oran7AppConfigManager.h"
#include <QGuiApplication>
#include <QStyleHints>
#include <QQmlEngine>
#include <algorithm>

Oran7Theme* Oran7Theme::s_instance = nullptr;
namespace
{
using namespace Oran7ThemeProfile;

bool sameValues(const QVariantMap& a, const QVariantMap& b)
{
    if (a.size() != b.size())
        return false;
    for (auto it = a.begin(); it != a.end(); ++it)
    {
        const auto other = b.constFind(it.key());
        if (other == b.cend() || it->metaType() != other->metaType() || it.value() != other.value())
            return false;
    }
    return true;
}
QVariantMap evaluateSource(const Oran7ThemeSource& source, QVariantMap raw, const QVariantMap& inherited, bool dark,
                          const QVariantMap& defaults = {}, bool* valid = nullptr,
                          const QString& editedKey = {})
{
    if (!source.versioned)
        return Oran7ThemeEvaluator(std::move(raw), inherited, dark, defaults).evaluate();

    const auto report = [&](const QString& key, const QString& reason)
    {
        if (valid && (editedKey.isEmpty() || source.affects(editedKey, key)))
            *valid = false;
        qWarning().noquote() << "主题源" << source.fieldFile(key) << "/" << (source.name.isEmpty() ? "Primary" : source.name)
                             << "/" << source.fieldLocation(key, dark) << "：" << reason
                             << (defaults.contains(key) ? "，使用默认值" : "，跳过该值");
    };
    for (auto it = raw.begin(); it != raw.end();)
    {
        if (!source.hasField(it.key()))
        {
            report(it.key(), QStringLiteral("未知字段"));
            it = raw.erase(it);
        }
        else
            ++it;
    }
    return Oran7ThemeEvaluator(std::move(raw), inherited, dark, defaults,
        [&](const QString& key, const QVariant& value)
        {
            if (source.accepts(key, value))
                return true;
            report(key, value.isValid() ? QStringLiteral("类型错误或超出范围")
                                        : QStringLiteral("引用或表达式无效"));
            return false;
        }).evaluate();
}

bool validDefaults(const Oran7ThemeSource& source, const QVariantMap& inherited, bool dark,
                   QVariantMap* values = nullptr)
{
    bool valid = true;
    auto raw = source.raw(dark);
    if (source.versioned)
        for (auto field = source.fields.cbegin(); field != source.fields.cend(); ++field)
            if (field->required && !raw.contains(field.key()))
            {
                qWarning().noquote() << "主题源" << source.fieldFile(field.key()) << "/" << source.name << "/"
                                     << source.fieldLocation(field.key(), dark)
                                     << "：缺少模式默认值，保留已发布主题";
                valid = false;
            }
    auto evaluated = evaluateSource(source, std::move(raw), inherited, dark, {}, &valid);
    if (values)
        *values = std::move(evaluated);
    return valid;
}

bool validBundle(const Oran7ThemeSources& sources)
{
    if (!sources.primary.versioned &&
        std::none_of(sources.components.cbegin(), sources.components.cend(),
                     [](const auto& component) { return component.versioned; }))
        return true;
    for (bool dark : {false, true})
    {
        QVariantMap primary;
        if (!validDefaults(sources.primary, {}, dark, &primary))
            return false;
        for (const auto& component : sources.components)
            if (component.versioned && !validDefaults(component, primary, dark))
                return false;
    }
    return true;
}
bool validEdit(const Oran7ThemeSource& source, const Oran7ThemeSource& primary, const QJsonObject& profile,
               const QJsonObject& overrides, const QString& key)
{
    if (!source.hasField(key))
    {
        qWarning().noquote() << "主题源" << source.path << "/" << source.name << "/" << key << "：未知字段，未保存";
        return false;
    }
    if (!source.versioned)
        return true;
    auto candidate = overrides;
    auto canonical = key;
    if (explicitMode(source.name, canonical).isEmpty())
        // 通用值可能同时被两个模式覆盖。验证时直接检查本次写入的这一层。
        for (bool dark : {false, true})
        {
            const auto mode = modeName(dark);
            auto tokens = layer(candidate, mode);
            tokens.remove(canonical);
            setLayer(candidate, mode, tokens);
        }
    for (bool dark : {false, true})
    {
        const auto globals = source.name.isEmpty() ? QVariantMap{}
            : evaluateSource(primary, primary.raw(dark, profile.value("__primary__").toObject()), {}, dark);
        bool valid = true;
        evaluateSource(source, source.raw(dark, candidate), globals, dark, {}, &valid, key);
        if (!valid)
            return false;
    }
    return true;
}
bool putToken(QJsonObject& group, const QString& component, QString token, const QVariant& input)
{
    const auto value = QJsonValue::fromVariant(input);
    if ((!value.isBool() && !value.isDouble() && !value.isString()) || token.startsWith("__"))
        return false;
    const auto mode = explicitMode(component, token);
    auto tokens = layer(group, mode);
    tokens[token] = value;
    setLayer(group, mode, tokens);
    return true;
}
void clearComponentPreview(QVariantMap& preview, const QString& component, QString token, const QStringList& modes)
{
    preview.remove(token);
    if (!explicitMode(component, token).isEmpty())
        return;
    if (component == "Oran7MusicPlaylistView")
        for (const auto& alias : PlaylistAliases)
            if (token == QLatin1String(alias.key))
            {
                if (modes.contains(modeName(false)))
                    preview.remove(QLatin1String(alias.light));
                if (modes.contains(modeName(true)))
                    preview.remove(QLatin1String(alias.dark));
                break;
            }
}
Oran7Theme::DarkMode profileMode(const QJsonObject& profile)
{
    const int mode = profile.value("__globals__").toObject().value("darkMode").toInt(-1);
    if (mode >= 0 && mode <= 2)
        return static_cast<Oran7Theme::DarkMode>(mode);
    return Oran7Theme::Dark;
}
} // namespace

void Oran7ThemePrivate::initializeComponentPropertyHash()
{
    Q_Q(Oran7Theme);
#define GROUP(Name) groups.insert(#Name, {&q->m_##Name, [q] { emit q->Name##Changed(); }})
    GROUP(Oran7ProgressSlider);
    GROUP(Oran7MainGUI);
    GROUP(Oran7CaptionBar);
    GROUP(Oran7MusicPlaylistView);
    GROUP(Oran7MusicPlayControls);
    GROUP(Oran7MusicLyricsWindow);
    GROUP(Oran7VideoPlayer);
    GROUP(Oran7ScreenCapture);
    GROUP(Oran7MusicLyricsIsland);
#undef GROUP
}

void Oran7ThemePrivate::rebuild(const QString& component)
{
    Q_Q(Oran7Theme);
    QStringList changed;
    bool primaryChanged = false;
    const bool dark = q->isDark();
    if (component.isEmpty())
    {
        const auto& source = sources.primary;
        const auto defaults = evaluateSource(source, source.raw(dark), {}, dark);
        primary = evaluateSource(source, source.raw(dark, profile.value("__primary__").toObject(), globalPreview(dark)),
                                 {}, dark, defaults);
        primaryChanged = !sameValues(primary, q->m_Primary);
        if (primaryChanged)
            q->m_Primary = primary;
    }
    const auto overrides = profile.value("__component__").toObject();
    for (auto it = groups.begin(); it != groups.end(); ++it)
    {
        const auto name = it.key();
        if (!component.isEmpty() && component != name)
            continue;
        const auto source = sources.components.constFind(name);
        if (source == sources.components.cend())
            continue;
        const auto defaults = evaluateSource(*source, source->raw(dark), primary, dark);
        auto result = evaluateSource(*source, source->raw(dark, overrides[name].toObject(), transient.value(name)),
                                     primary, dark, defaults);
        // 旧读取接口只作派生别名，深色状态只由 darkMode 决定。
        if (name == "Oran7MainGUI")
            result["isDarkMode"] = dark;
        if (!sameValues(*it->values, result))
        {
            *it->values = std::move(result);
            changed << name;
        }
    }
    // 所有结果先提交，再通知；绑定不会读到前后两套主题的混合快照。
    if (primaryChanged)
        emit q->PrimaryChanged();
    for (const auto& name : std::as_const(changed))
        groups[name].notify();
}

bool Oran7ThemePrivate::saveProfile(const QJsonObject& value)
{
    QJsonObject components;
    for (auto it = sources.components.cbegin(); it != sources.components.cend(); ++it)
        components[it.key()] = it->profileDefaults();
    const QJsonObject defaults{{"__primary__", sources.primary.profileDefaults()}, {"__component__", components}};
    if (profiles.saveThemeProfile(m_currentThemeProfile, value, &defaults, &profile))
        return true;
    qWarning() << "主题 profile 保存失败，保留当前设置:" << m_currentThemeProfile;
    return false;
}

QVariantMap Oran7ThemePrivate::globalPreview(bool dark) const
{
    auto preview = transientBaseColors.value(dark);
    for (auto it = transientGlobal.cbegin(); it != transientGlobal.cend(); ++it)
        preview[it.key()] = it.value();
    return preview;
}

void Oran7ThemePrivate::clearPreview()
{
    transient.clear();
    transientGlobal.clear();
    transientBaseColors.clear();
}
void Oran7ThemePrivate::refreshDark(bool oldDark, Oran7Theme::DarkMode oldMode)
{
    Q_Q(Oran7Theme);
    lastEffectiveDark = q->isDark();
    rebuild();
    if (oldDark != q->isDark())
        emit q->isDarkChanged();
    if (oldMode != m_darkMode)
        emit q->darkModeChanged();
}

Oran7Theme::Oran7Theme(QObject* parent)
    : Oran7Theme(Oran7ThemeProfileManager::instance(), Oran7AppConfigManager::ins(), parent)
{
}

Oran7Theme::Oran7Theme(Oran7ThemeProfileManager& profiles, Oran7AppConfigManager& appConfig, QObject* parent)
    : QObject(parent), d_ptr(new Oran7ThemePrivate(this, profiles, appConfig))
{
    Q_D(Oran7Theme);
    d->initializeComponentPropertyHash();
    d->m_currentThemeProfile = appConfig.snapshot().appearance.themeProfile;
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme scheme)
            {
                Q_D(Oran7Theme);
                if (d->m_darkMode == System)
                    d->refreshDark(d->lastEffectiveDark, d->m_darkMode);
                Q_UNUSED(scheme);
            });
    reloadTheme();
}
Oran7Theme::~Oran7Theme() = default;
Oran7Theme& Oran7Theme::instance()
{
    if (!s_instance)
        s_instance = new Oran7Theme;
    return *s_instance;
}
void Oran7Theme::cleanup()
{
    delete s_instance;
    s_instance = nullptr;
}
Oran7Theme* Oran7Theme::create(QQmlEngine*, QJSEngine*)
{
    auto* result = &instance();
    QQmlEngine::setObjectOwnership(result, QQmlEngine::CppOwnership);
    return result;
}
bool Oran7Theme::isDark() const
{
    Q_D(const Oran7Theme);
    return d->m_darkMode == System ? QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
                                   : d->m_darkMode == Dark;
}
Oran7Theme::DarkMode Oran7Theme::darkMode() const
{
    Q_D(const Oran7Theme);
    return d->m_darkMode;
}
void Oran7Theme::setDarkMode(DarkMode mode)
{
    Q_D(Oran7Theme);
    if (mode < Light || mode > System || d->m_darkMode == mode)
        return;
    auto value = d->profile;
    auto globals = value.value("__globals__").toObject();
    globals["darkMode"] = int(mode);
    value["__globals__"] = globals;
    if (!d->saveProfile(value))
        return;
    const bool oldDark = isDark();
    const auto oldMode = d->m_darkMode;
    d->m_darkMode = mode;
    d->refreshDark(oldDark, oldMode);
}
QVariantMap Oran7Theme::sizeHint() const
{
    Q_D(const Oran7Theme);
    return d->m_sizeHintMap;
}
void Oran7Theme::installSizeHintRatio(const QString& size, qreal ratio)
{
    Q_D(Oran7Theme);
    if (std::isfinite(ratio) && ratio > 0 && d->m_sizeHintMap.value(size) != ratio)
    {
        d->m_sizeHintMap[size] = ratio;
        emit sizeHintChanged();
    }
}
QString Oran7Theme::currentThemeProfile() const
{
    Q_D(const Oran7Theme);
    return d->m_currentThemeProfile;
}
QStringList Oran7Theme::availableThemeProfiles() const
{
    Q_D(const Oran7Theme);
    return d->profiles.getAvailableThemeProfiles();
}
void Oran7ThemePrivate::reload(const QString& path)
{
    auto next = Oran7ThemeSources::read(path, groups.keys());
    if (!next || !validBundle(*next))
    {
        qWarning() << "主题清单无效，保留当前主题:" << path;
        return;
    }
    profiles.initializeDefaultTheme();
    auto nextProfile = profiles.loadThemeProfile(m_currentThemeProfile);
    if (nextProfile.isEmpty())
        nextProfile = profile.isEmpty() ? QJsonObject{{"__profileVersion__", 3}} : profile;
    applySources(std::move(*next), path, std::move(nextProfile));
}

void Oran7ThemePrivate::applySources(Oran7ThemeSources next, const QString& path, QJsonObject nextProfile)
{
    Q_Q(Oran7Theme);
    const bool oldDark = q->isDark();
    const auto oldMode = m_darkMode;
    sources = std::move(next);
    indexPath = path;
    clearPreview();
    profile = std::move(nextProfile);
    m_darkMode = profileMode(profile);
    refreshDark(oldDark, oldMode);
}

void Oran7Theme::reloadTheme()
{
    Q_D(Oran7Theme);
    d->reload(d->indexPath);
}
void Oran7Theme::setCurrentThemeProfile(const QString& name)
{
    Q_D(Oran7Theme);
    if (name == d->m_currentThemeProfile)
        return;
    auto next = Oran7ThemeSources::read(d->indexPath, d->groups.keys());
    if (!next || !validBundle(*next))
        return;
    auto nextProfile = d->profiles.loadThemeProfile(name);
    if (nextProfile.isEmpty())
        return;
    const auto previous = d->appConfig.snapshot();
    auto saved = previous;
    saved.appearance.themeProfile = name;
    d->appConfig.updateConfig(saved);
    if (!d->appConfig.saveConfig())
    {
        d->appConfig.updateConfig(previous);
        return;
    }
    d->m_currentThemeProfile = name;
    d->applySources(std::move(*next), d->indexPath, std::move(nextProfile));
    emit currentThemeProfileChanged();
}
void Oran7Theme::installComponentToken(const QString& component, const QString& token, const QVariant& value)
{
    Q_D(Oran7Theme);
    if (component == "Oran7MainGUI" && token == "isDarkMode")
    {
        setDarkMode(value.toBool() ? Dark : Light);
        return;
    }
    if (!d->groups.contains(component))
        return;
    d->transient[component][token] = value;
    d->rebuild(component);
}
void Oran7Theme::saveComponentToken(const QString& component, const QString& token, const QVariant& value)
{
    Q_D(Oran7Theme);
    if (component == "Oran7MainGUI" && token == "isDarkMode")
    {
        setDarkMode(value.toBool() ? Dark : Light);
        return;
    }
    const auto source = d->sources.components.constFind(component);
    if (source == d->sources.components.cend())
        return;
    auto profile = d->profile;
    auto components = profile.value("__component__").toObject();
    auto group = components.value(component).toObject();
    if (!putToken(group, component, token, value))
        return;
    components[component] = group;
    profile["__component__"] = components;
    if (validEdit(*source, d->sources.primary, profile, group, token) && d->saveProfile(profile))
    {
        clearComponentPreview(d->transient[component], component, token, {modeName(false), modeName(true)});
        d->rebuild(component);
    }
}
void Oran7Theme::installIndexToken(const QString& token, const QString& value)
{
    Q_D(Oran7Theme);
    d->transientGlobal[token] = value;
    d->rebuild();
}
void Oran7Theme::saveIndexToken(const QString& token, const QVariant& value)
{
    Q_D(Oran7Theme);
    auto valueToSave = d->profile;
    auto group = valueToSave.value("__primary__").toObject();
    if (!putToken(group, {}, token, value))
        return;
    valueToSave["__primary__"] = group;
    if (validEdit(d->sources.primary, d->sources.primary, valueToSave, group, token) &&
        d->saveProfile(valueToSave))
    {
        d->transientGlobal.remove(token);
        for (auto& mode : d->transientBaseColors)
            mode.remove(token);
        d->rebuild();
    }
}

namespace
{
void installBaseColors(Oran7ThemePrivate& theme, const QString& key, const QString& colors)
{
    const auto pair = colors.split('|');
    if (pair.size() != 2 || !QColor(pair[0]).isValid() || !QColor(pair[1]).isValid())
        return;
    theme.transientBaseColors[false][key] = pair[0];
    theme.transientBaseColors[true][key] = pair[1];
    theme.transientGlobal.remove(key);
    theme.rebuild();
}
}
void Oran7Theme::installThemeColorTextBase(const QString& colors)
{
    Q_D(Oran7Theme);
    installBaseColors(*d, "colorTextBase", colors);
}
void Oran7Theme::installThemeColorBgBase(const QString& colors)
{
    Q_D(Oran7Theme);
    installBaseColors(*d, "colorBgBase", colors);
}
void Oran7Theme::installThemePrimaryColorBase(const QColor& color)
{
    installIndexToken("colorPrimaryBase", QString("$genColor(%1)").arg(color.name()));
}
void Oran7Theme::installThemePrimaryFontSizeBase(int value)
{
    installIndexToken("fontSizeBase", QString("$genFontSize(%1)").arg(value));
}
void Oran7Theme::installThemePrimaryFontFamiliesBase(const QString& value)
{
    installIndexToken("fontFamilyBase", QString("$genFontFamily(%1)").arg(value));
}
void Oran7Theme::installThemePrimaryRadiusBase(int value)
{
    installIndexToken("radiusBase", QString("$genRadius(%1)").arg(value));
}
void Oran7Theme::installThemePrimaryAnimationBase(int fast, int mid, int slow)
{
    Q_D(Oran7Theme);
    d->transientGlobal["durationFast"] = fast;
    d->transientGlobal["durationMid"] = mid;
    d->transientGlobal["durationSlow"] = slow;
    d->rebuild();
}
void Oran7Theme::installThemeComponentColorBase(const QString& component, const QColor& color)
{
    installComponentToken(component, "colorPrimaryBase", QString("$genColor(%1)").arg(color.name()));
}
void Oran7Theme::installIndexTheme(const QString& path)
{
    Q_D(Oran7Theme);
    const auto next = path.isEmpty() ? QString(Oran7ThemeSources::DefaultIndex) : path;
    if (next != d->indexPath)
        d->reload(next);
}
void Oran7Theme::installComponentTheme(const QString& component, const QString& path)
{
    Q_D(Oran7Theme);
    if (!d->groups.contains(component))
        return;
    auto source = Oran7ThemeSources::readComponent(path, component);
    if (!source)
        return;
    if (source->versioned)
        for (bool dark : {false, true})
        {
            const auto& primary = d->sources.primary;
            const auto globals = evaluateSource(primary, primary.raw(dark, d->profile.value("__primary__").toObject(), d->globalPreview(dark)), {}, dark);
            if (!validDefaults(*source, globals, dark))
                return;
        }
    d->sources.components.insert(component, std::move(*source));
    d->rebuild(component);
}
bool Oran7Theme::createThemeProfile(const QString& name, const QString& source)
{
    return duplicateThemeProfile(source, name);
}
bool Oran7Theme::deleteThemeProfile(const QString& name)
{
    Q_D(Oran7Theme);
    if (name == "profile-default")
        return false;
    if (name == d->m_currentThemeProfile)
    {
        setCurrentThemeProfile("profile-default");
        if (name == d->m_currentThemeProfile)
            return false;
    }
    if (!d->profiles.deleteThemeProfile(name))
        return false;
    emit availableThemeProfilesChanged();
    return true;
}
bool Oran7Theme::duplicateThemeProfile(const QString& source, const QString& name)
{
    Q_D(Oran7Theme);
    const bool ok = d->profiles.duplicateThemeProfile(source, name);
    if (ok)
        emit availableThemeProfilesChanged();
    return ok;
}
bool Oran7Theme::exportThemeProfile(const QString& name, const QString& path)
{
    Q_D(Oran7Theme);
    return d->profiles.exportThemeProfile(name, path);
}
QString Oran7Theme::importThemeProfile(const QString& path, const QString& name)
{
    Q_D(Oran7Theme);
    const auto result = d->profiles.importThemeProfile(path, name);
    if (!result.isEmpty())
        emit availableThemeProfilesChanged();
    return result;
}
void Oran7Theme::resetToDefaultTheme()
{
    Q_D(Oran7Theme);
    if (!d->saveProfile(QJsonObject{{"__profileVersion__", 3}}))
        return;
    const bool oldDark = isDark();
    const auto oldMode = d->m_darkMode;
    d->clearPreview();
    d->m_darkMode = profileMode(d->profile);
    d->refreshDark(oldDark, oldMode);
}

bool Oran7ThemePrivate::resetGroup(const QString& component, QString token, bool currentModeOnly)
{
    Q_Q(Oran7Theme);
    const auto local = sources.components.constFind(component);
    if (!component.isEmpty() && local == sources.components.cend())
        return false;
    const auto& source = component.isEmpty() ? sources.primary : *local;
    if (!token.isEmpty() && !source.hasField(token))
        return false;
    const auto originalToken = token;
    auto next = profile;
    auto components = next.value("__component__").toObject();
    auto group = component.isEmpty() ? next.value("__primary__").toObject() : components.value(component).toObject();
    const auto explicitLayer = explicitMode(component, token);
    const auto modes = !explicitLayer.isEmpty() ? QStringList{explicitLayer}
        : currentModeOnly ? QStringList{modeName(q->isDark())}
                          : QStringList{QString{}, modeName(false), modeName(true)};
    for (const auto& mode : modes)
    {
        auto tokens = layer(group, mode);
        if (token.isEmpty())
        {
            for (auto it = tokens.begin(); it != tokens.end();)
            {
                if (source.hasField(it.key()))
                    it = tokens.erase(it);
                else
                    ++it;
            }
        }
        else
            tokens.remove(token);
        setLayer(group, mode, tokens);
    }
    if (component.isEmpty())
        next["__primary__"] = group;
    else
    {
        components[component] = group;
        next["__component__"] = components;
    }
    if (!saveProfile(next))
        return false;

    if (component.isEmpty())
    {
        transientGlobal.remove(token);
        if (!currentModeOnly)
            transientBaseColors[!q->isDark()].remove(token);
        transientBaseColors[q->isDark()].remove(token);
    }
    else if (token.isEmpty())
        transient.remove(component);
    else
        clearComponentPreview(transient[component], component, originalToken, modes);
    rebuild(component);
    return true;
}

bool Oran7Theme::resetComponentToken(const QString& component, const QString& token, bool currentModeOnly)
{
    Q_D(Oran7Theme);
    if (component.isEmpty() || token.isEmpty())
        return false;
    if (component == "Oran7MainGUI" && token == "isDarkMode")
    {
        if (currentModeOnly)
            return false;
        auto next = d->profile;
        auto globals = next.value("__globals__").toObject();
        globals.remove("darkMode");
        next["__globals__"] = globals;
        if (!d->saveProfile(next))
            return false;
        const bool oldDark = isDark();
        const auto oldMode = d->m_darkMode;
        d->m_darkMode = profileMode(d->profile);
        d->refreshDark(oldDark, oldMode);
        return true;
    }
    return d->resetGroup(component, token, currentModeOnly);
}

bool Oran7Theme::resetComponentTheme(const QString& component, bool currentModeOnly)
{
    Q_D(Oran7Theme);
    return !component.isEmpty() && d->resetGroup(component, {}, currentModeOnly);
}

bool Oran7Theme::resetIndexToken(const QString& token, bool currentModeOnly)
{
    Q_D(Oran7Theme);
    return !token.isEmpty() && d->resetGroup({}, token, currentModeOnly);
}
