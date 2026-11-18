#include "Oran7ThemeProfile.h"

#include <QFile>
#include <QJsonDocument>

namespace Oran7ThemeProfile
{
QString modeName(bool dark)
{
    return dark ? QStringLiteral("dark") : QStringLiteral("light");
}

QString explicitMode(const QString& component, QString& token)
{
    if (component == "Oran7MusicPlaylistView")
        for (const auto& alias : PlaylistAliases)
        {
            const bool dark = token == QLatin1String(alias.dark);
            if (dark || token == QLatin1String(alias.light))
            {
                token = QLatin1String(alias.key);
                return modeName(dark);
            }
        }
    return {};
}

QJsonObject layer(const QJsonObject& group, const QString& mode)
{
    return mode.isEmpty() ? group.value("__tokens__").toObject()
                          : group.value("__modes__").toObject().value(mode).toObject();
}

void setLayer(QJsonObject& group, const QString& mode, const QJsonObject& tokens)
{
    const auto update = [&](QJsonObject& object, const QString& key)
    {
        if (tokens.isEmpty())
            object.remove(key);
        else
            object[key] = tokens;
    };
    if (mode.isEmpty())
    {
        update(group, "__tokens__");
        return;
    }
    auto modes = group.value("__modes__").toObject();
    update(modes, mode);
    if (modes.isEmpty())
        group.remove("__modes__");
    else
        group["__modes__"] = modes;
}

namespace
{
const QJsonObject& publishedLegacySource()
{
    // 历史差量必须以发布过的默认值计算，不能随新版主题的配色调整而移动基准。
    static const auto source = []
    {
        QFile file(":/config/themeJson/legacy-profile-defaults.json");
        if (!file.open(QIODevice::ReadOnly))
            return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    return source;
}

QJsonObject legacyTokens(const QJsonObject& source, const QString& key)
{
    return key == "__style__" ? source.value(key).toObject()
                             : source.value("__init__").toObject().value(key).toObject();
}

bool reserved(const QString& key)
{
    return key.startsWith("__") && key.endsWith("__");
}

QJsonObject convertGroup(QJsonObject source, const QString& component, const QJsonObject& defaults)
{
    QJsonObject common, light, dark;
    auto oldStyle = source.take("__style__").toObject();
    auto oldInit = source.take("__init__").toObject();
    auto oldVars = oldInit.take("__vars__").toObject();
    const auto defaultStyle = legacyTokens(defaults, "__style__");
    const auto defaultVars = legacyTokens(defaults, "__vars__");
    if (component == "Oran7MainGUI")
        for (const auto& key : {"isDarkMode", "OpenSettingWin", "settingContent_visiable"})
        {
            oldStyle.remove(key);
            oldVars.remove(key);
        }

    auto effective = oldStyle;
    for (auto it = oldVars.begin(); it != oldVars.end(); ++it)
        if (!effective.contains(it.key()) && !defaultStyle.contains(it.key()))
            effective[it.key()] = it.value();
    if (component.isEmpty())
    {
        auto bases = oldInit.take("__base__").toObject();
        const auto defaultBases = legacyTokens(defaults, "__base__");
        for (const auto& key : {"colorTextBase", "colorBgBase"})
        {
            const auto pair = bases.value(key).toString().split('|');
            if (pair.size() == 2)
            {
                light[key] = pair[0];
                dark[key] = pair[1];
                bases.remove(key);
            }
            // 旧基色对会遮蔽同名通用 token；无效基色对沿用旧源的有效缺省值。
            if (defaultBases.contains(key) || light.contains(key))
                effective.remove(key);
        }
        if (!bases.isEmpty())
            oldInit["__base__"] = bases;
    }
    for (auto it = effective.begin(); it != effective.end(); ++it)
    {
        const auto& defaultsForKey = oldStyle.contains(it.key()) ? defaultStyle : defaultVars;
        if (it.value() == defaultsForKey.value(it.key()))
            continue;
        auto key = it.key();
        const auto mode = explicitMode(component, key);
        auto& tokens = mode.isEmpty() ? common : (mode == "light" ? light : dark);
        tokens[key] = it.value();
    }
    QJsonObject result;
    setLayer(result, {}, common);
    setLayer(result, modeName(false), light);
    setLayer(result, modeName(true), dark);
    if (!oldInit.isEmpty())
        source["__init__"] = oldInit;
    if (!source.isEmpty())
        result["__legacy__"] = source;
    return result;
}

bool objectMember(const QJsonObject& parent, const QString& key)
{
    return !parent.contains(key) || parent.value(key).isObject();
}

bool validLegacyGroup(const QJsonObject& group)
{
    const auto init = group.value("__init__").toObject();
    return objectMember(group, "__init__") && objectMember(group, "__style__") &&
           objectMember(init, "__vars__") && objectMember(init, "__base__");
}

bool validGroup(const QJsonObject& group)
{
    const auto modes = group.value("__modes__").toObject();
    return objectMember(group, "__tokens__") && objectMember(group, "__modes__") &&
           objectMember(modes, "light") && objectMember(modes, "dark");
}

QJsonValue defaultToken(const QJsonObject& group, const QString& mode, const QString& key)
{
    const auto local = layer(group, mode);
    return local.contains(key) ? local.value(key) : layer(group).value(key);
}

QJsonObject trimGroup(QJsonObject group, const QJsonObject& defaults)
{
    auto common = layer(group);
    for (auto it = common.begin(); it != common.end();)
    {
        if (!reserved(it.key()) && it.value() == defaultToken(defaults, "light", it.key()) &&
            it.value() == defaultToken(defaults, "dark", it.key()))
            it = common.erase(it);
        else
            ++it;
    }
    setLayer(group, {}, common);
    for (const auto& mode : {modeName(false), modeName(true)})
    {
        auto tokens = layer(group, mode);
        for (auto it = tokens.begin(); it != tokens.end();)
        {
            const auto fallback = common.contains(it.key()) ? common.value(it.key())
                                                            : defaultToken(defaults, mode, it.key());
            if (!reserved(it.key()) && it.value() == fallback)
                it = tokens.erase(it);
            else
                ++it;
        }
        setLayer(group, mode, tokens);
    }
    return group;
}

void setGroup(QJsonObject& parent, const QString& key, const QJsonObject& group)
{
    if (group.isEmpty())
        parent.remove(key);
    else
        parent[key] = group;
}
} // namespace

QJsonObject legacyDefaults(const QJsonObject& source, const QString& component)
{
    return convertGroup(source, component, {});
}

const QJsonObject& legacyProfileDefaults()
{
    static const auto defaults = []
    {
        auto source = publishedLegacySource();
        const auto groups = source.take("__component__").toObject();
        QJsonObject components;
        for (auto it = groups.begin(); it != groups.end(); ++it)
            components[it.key()] = legacyDefaults(it.value().toObject(), it.key());
        return QJsonObject{{"__primary__", legacyDefaults(source)}, {"__component__", components}};
    }();
    return defaults;
}

bool isV3(const QJsonObject& profile)
{
    return profile.value("__profileVersion__") == QJsonValue(3) && !profile.contains("profileVersion");
}

std::optional<QJsonObject> normalize(QJsonObject profile, const QJsonObject* defaults)
{
    if (!objectMember(profile, "__component__"))
        return std::nullopt;
    auto components = profile.value("__component__").toObject();
    if (!isV3(profile))
    {
        if (profile.contains("__profileVersion__") ||
            (profile.contains("profileVersion") && profile.value("profileVersion") != QJsonValue(2)) ||
            !validLegacyGroup(profile))
            return std::nullopt;

        const auto& oldSource = publishedLegacySource();
        if (oldSource.isEmpty())
            return std::nullopt;
        const auto oldComponents = oldSource.value("__component__").toObject();
        const auto oldDark = legacyTokens(components.value("Oran7MainGUI").toObject(), "__style__").value("isDarkMode");
        auto mode = profile.take("darkMode");
        if (mode.isUndefined() && oldDark.isBool())
            mode = oldDark.toBool() ? 1 : 0;
        profile.remove("profileVersion");
        profile.remove("__component__");
        auto primary = convertGroup(profile, {}, oldSource);
        // 旧根级扩展保留原键。仅结构名发生冲突时放进旧格式扩展。
        profile = primary.take("__legacy__").toObject();
        QJsonObject conflicts;
        for (const auto& key : {"__profileVersion__", "__globals__", "__primary__", "__component__"})
            if (profile.contains(key))
                conflicts[key] = profile.take(key);
        if (!conflicts.isEmpty())
        {
            auto extras = profile.value("__legacy__").toObject();
            extras["__structuredKeys__"] = conflicts;
            profile["__legacy__"] = extras;
        }
        setGroup(profile, "__primary__", primary);
        if (!mode.isUndefined())
            profile["__globals__"] = QJsonObject{{"darkMode", mode}};
        for (auto it = components.begin(); it != components.end(); ++it)
        {
            if (!oldComponents.contains(it.key()))
                continue;
            if (!it->isObject() || !validLegacyGroup(it->toObject()))
                return std::nullopt;
            it.value() = convertGroup(it->toObject(), it.key(), oldComponents.value(it.key()).toObject());
        }
        defaults = &legacyProfileDefaults();
    }

    profile["__profileVersion__"] = 3;
    if (!objectMember(profile, "__globals__") || !objectMember(profile, "__primary__"))
        return std::nullopt;
    auto primary = profile.value("__primary__").toObject();
    if (!validGroup(primary))
        return std::nullopt;
    const auto source = defaults ? *defaults : QJsonObject{};
    setGroup(profile, "__primary__", trimGroup(primary, source.value("__primary__").toObject()));
    setGroup(profile, "__globals__", profile.value("__globals__").toObject());
    const auto componentDefaults = source.value("__component__").toObject();
    const auto knownComponents = publishedLegacySource().value("__component__").toObject();
    for (auto it = components.begin(); it != components.end();)
    {
        if (!knownComponents.contains(it.key()) && !componentDefaults.contains(it.key()))
        {
            ++it;
            continue;
        }
        if (!it->isObject() || !validGroup(it->toObject()))
            return std::nullopt;
        const auto group = trimGroup(it->toObject(), componentDefaults.value(it.key()).toObject());
        if (group.isEmpty())
            it = components.erase(it);
        else
        {
            it.value() = group;
            ++it;
        }
    }
    setGroup(profile, "__component__", components);
    return profile;
}
} // namespace Oran7ThemeProfile
