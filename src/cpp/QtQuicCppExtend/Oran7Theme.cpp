#include "Oran7Theme.h"
#include "Oran7Theme_p.h"
#include "Oran7ThemeEvaluator_p.h"
#include "Oran7ThemeProfileManager.h"
#include "Oran7AppConfigManager.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QGuiApplication>
#include <QStyleHints>
#include <QQmlEngine>

Oran7Theme* Oran7Theme::s_instance = nullptr;
namespace
{
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
QJsonObject readObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}
QJsonObject readSource(const QString& path)
{
    auto result = readObject(path);
    if (result.isEmpty())
        result = readObject(Oran7GlobalHelper::getConfigDir() + "/themeJson/" + QFileInfo(path).fileName());
    return result;
}
void merge(QJsonObject& target, const QJsonObject& source)
{
    for (auto it = source.begin(); it != source.end(); ++it)
    {
        if (it->isObject() && target.value(it.key()).isObject())
        {
            auto child = target.value(it.key()).toObject();
            merge(child, it->toObject());
            target[it.key()] = child;
        }
        else
            target[it.key()] = it.value();
    }
}
QVariantMap tokens(const QJsonObject& object)
{
    auto result = object.value("__init__").toObject()["__vars__"].toObject().toVariantMap();
    const auto style = object.value("__style__").toObject().toVariantMap();
    for (auto it = style.begin(); it != style.end(); ++it)
        result[it.key()] = it.value();
    return result;
}
void putToken(QJsonObject& object, const QString& token, const QVariant& value)
{
    auto init = object.value("__init__").toObject();
    auto vars = init.value("__vars__").toObject();
    if (vars.contains(token))
    {
        vars[token] = QJsonValue::fromVariant(value);
        init["__vars__"] = vars;
        object["__init__"] = init;
    }
    else
    {
        auto style = object.value("__style__").toObject();
        style[token] = QJsonValue::fromVariant(value);
        object["__style__"] = style;
    }
}
Oran7Theme::DarkMode profileMode(const QJsonObject& profile)
{
    if (profile.contains("darkMode"))
    {
        const int mode = profile.value("darkMode").toInt(-1);
        if (mode >= 0 && mode <= 2)
            return static_cast<Oran7Theme::DarkMode>(mode);
    }
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
    if (component.isEmpty())
    {
        auto effective = index;
        auto overrides = profile;
        overrides.remove("__component__");
        overrides.remove("profileVersion");
        overrides.remove("darkMode");
        merge(effective, overrides);
        auto raw = tokens(effective);
        const auto base = effective.value("__init__").toObject()["__base__"].toObject();
        for (auto it = base.begin(); it != base.end(); ++it)
        {
            const auto parts = it->toString().split('|');
            if (parts.size() == 2)
                raw[it.key()] = parts[q->isDark() ? 1 : 0];
            else
                qWarning() << "主题基础色格式无效:" << it.key();
        }
        for (auto it = transientGlobal.begin(); it != transientGlobal.end(); ++it)
            raw[it.key()] = it.value();
        auto defaultRaw = tokens(index);
        const auto defaultBase = index.value("__init__").toObject()["__base__"].toObject();
        for (auto it = defaultBase.begin(); it != defaultBase.end(); ++it)
        {
            const auto parts = it->toString().split('|');
            if (parts.size() == 2)
                defaultRaw[it.key()] = parts[q->isDark() ? 1 : 0];
        }
        const auto defaults = Oran7ThemeEvaluator(defaultRaw, {}, q->isDark()).evaluate();
        primary = Oran7ThemeEvaluator(raw, {}, q->isDark(), defaults).evaluate();
        primaryChanged = !sameValues(primary, q->m_Primary);
        if (primaryChanged)
            q->m_Primary = primary;
    }
    const auto refs = index.value("__component__").toObject();
    const auto overrides = profile.value("__component__").toObject();
    for (auto it = groups.begin(); it != groups.end(); ++it)
    {
        const auto name = it.key();
        if (!component.isEmpty() && component != name)
            continue;
        auto source = readSource(refs[name].toString());
        if (source.isEmpty())
        {
            qWarning() << "主题组件资源无效:" << name;
            continue;
        }
        const auto defaults = Oran7ThemeEvaluator(tokens(source), primary, q->isDark()).evaluate();
        merge(source, overrides[name].toObject());
        auto raw = tokens(source);
        const auto temporary = transient.value(name);
        for (auto t = temporary.begin(); t != temporary.end(); ++t)
            raw[t.key()] = t.value();
        auto result = Oran7ThemeEvaluator(raw, primary, q->isDark(), defaults).evaluate();
        // 旧读取接口只作派生别名，深色状态只由 darkMode 决定。
        if (name == "Oran7MainGUI")
            result["isDarkMode"] = q->isDark();
        if (!sameValues(*it->values, result))
        {
            *it->values = std::move(result);
            changed << name;
        }
    }
    // 所有结果先提交，再通知；绑定不会读到前后两套主题的混合快照。
    if (primaryChanged)
        emit q->PrimaryChanged();
    for (const auto& name : changed)
        groups[name].notify();
}

bool Oran7ThemePrivate::saveProfile(const QJsonObject& value)
{
    if (!Oran7ThemeProfileManager::instance().saveThemeProfile(m_currentThemeProfile, value))
        return false;
    profile = Oran7ThemeProfileManager::instance().loadThemeProfile(m_currentThemeProfile);
    return true;
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

Oran7Theme::Oran7Theme(QObject* parent) : QObject(parent), d_ptr(new Oran7ThemePrivate(this))
{
    Q_D(Oran7Theme);
    d->initializeComponentPropertyHash();
    d->m_currentThemeProfile = Oran7AppConfigManager::ins().snapshot().appearance.themeProfile;
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
    value["darkMode"] = int(mode);
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
    return Oran7ThemeProfileManager::instance().getAvailableThemeProfiles();
}
void Oran7Theme::reloadTheme()
{
    Q_D(Oran7Theme);
    auto source = readSource(d->themIndexJsonPath);
    if (source.isEmpty())
    {
        qWarning() << "无法加载主题 Index";
        return;
    }
    const bool oldDark = isDark();
    const auto oldMode = d->m_darkMode;
    d->index = source;
    d->transient.clear();
    d->transientGlobal.clear();
    auto& profiles = Oran7ThemeProfileManager::instance();
    profiles.initializeDefaultTheme(this);
    d->profile = profiles.loadThemeProfile(d->m_currentThemeProfile);
    d->m_darkMode = profileMode(d->profile);
    d->refreshDark(oldDark, oldMode);
}
void Oran7Theme::setCurrentThemeProfile(const QString& name)
{
    Q_D(Oran7Theme);
    if (name == d->m_currentThemeProfile || !Oran7ThemeProfileManager::instance().isThemeProfileReadable(name))
        return;
    auto saved = Oran7AppConfigManager::ins().snapshot();
    saved.appearance.themeProfile = name;
    const auto previous = Oran7AppConfigManager::ins().snapshot();
    Oran7AppConfigManager::ins().updateConfig(saved);
    if (!Oran7AppConfigManager::ins().saveConfig())
    {
        Oran7AppConfigManager::ins().updateConfig(previous);
        return;
    }
    d->m_currentThemeProfile = name;
    reloadTheme();
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
    if (!d->groups.contains(component))
        return;
    auto profile = d->profile;
    auto components = profile.value("__component__").toObject();
    auto source = readSource(d->index.value("__component__").toObject()[component].toString());
    auto object = components[component].toObject();
    // 使用默认结构判断变量区，profile 不复制整份组件。
    if (source.value("__init__").toObject()["__vars__"].toObject().contains(token))
    {
        auto init = object.value("__init__").toObject();
        auto vars = init.value("__vars__").toObject();
        vars[token] = QJsonValue::fromVariant(value);
        init["__vars__"] = vars;
        object["__init__"] = init;
    }
    else
        putToken(object, token, value);
    components[component] = object;
    profile["__component__"] = components;
    if (d->saveProfile(profile))
    {
        d->transient[component].remove(token);
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
    auto object = d->profile;
    if (d->index.value("__init__").toObject()["__vars__"].toObject().contains(token))
    {
        auto init = object.value("__init__").toObject();
        auto vars = init.value("__vars__").toObject();
        vars[token] = QJsonValue::fromVariant(value);
        init["__vars__"] = vars;
        object["__init__"] = init;
    }
    else
        putToken(object, token, value);
    if (d->saveProfile(object))
    {
        d->transientGlobal.remove(token);
        d->rebuild();
    }
}
void Oran7Theme::installThemeColorTextBase(const QString& colors)
{
    Q_D(Oran7Theme);
    auto init = d->index.value("__init__").toObject();
    auto base = init.value("__base__").toObject();
    base["colorTextBase"] = colors;
    init["__base__"] = base;
    d->index["__init__"] = init;
    d->rebuild();
}
void Oran7Theme::installThemeColorBgBase(const QString& colors)
{
    Q_D(Oran7Theme);
    auto init = d->index.value("__init__").toObject();
    auto base = init.value("__base__").toObject();
    base["colorBgBase"] = colors;
    init["__base__"] = base;
    d->index["__init__"] = init;
    d->rebuild();
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
    const auto next = path.isEmpty() ? QString(":/config/themeJson/Index.json") : path;
    if (next != d->themIndexJsonPath)
    {
        d->themIndexJsonPath = next;
        reloadTheme();
    }
}
void Oran7Theme::installComponentTheme(const QString& component, const QString& path)
{
    Q_D(Oran7Theme);
    if (!d->groups.contains(component) || readSource(path).isEmpty())
        return;
    auto refs = d->index.value("__component__").toObject();
    refs[component] = path;
    d->index["__component__"] = refs;
    d->rebuild(component);
}
bool Oran7Theme::createThemeProfile(const QString& name, const QString& source)
{
    auto& p = Oran7ThemeProfileManager::instance();
    if (availableThemeProfiles().contains(name) || !availableThemeProfiles().contains(source))
        return false;
    const bool ok = p.saveThemeProfile(name, p.loadThemeProfile(source));
    if (ok)
        emit availableThemeProfilesChanged();
    return ok;
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
    if (!Oran7ThemeProfileManager::instance().deleteThemeProfile(name))
        return false;
    emit availableThemeProfilesChanged();
    return true;
}
bool Oran7Theme::duplicateThemeProfile(const QString& source, const QString& name)
{
    const bool ok = Oran7ThemeProfileManager::instance().duplicateThemeProfile(source, name);
    if (ok)
        emit availableThemeProfilesChanged();
    return ok;
}
bool Oran7Theme::exportThemeProfile(const QString& name, const QString& path)
{
    return Oran7ThemeProfileManager::instance().exportThemeProfile(name, path);
}
QString Oran7Theme::importThemeProfile(const QString& path, const QString& name)
{
    const auto result = Oran7ThemeProfileManager::instance().importThemeProfile(path, name);
    if (!result.isEmpty())
        emit availableThemeProfilesChanged();
    return result;
}
void Oran7Theme::resetToDefaultTheme()
{
    Q_D(Oran7Theme);
    if (d->saveProfile({}))
        reloadTheme();
}
