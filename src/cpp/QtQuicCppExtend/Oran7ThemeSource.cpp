#include "Oran7ThemeSource_p.h"
#include "Oran7ThemeSchema_p.h"
#include "Oran7ThemeProfile.h"
#include "Oran7GlobalHelper.h"

#include <QColor>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>
#include <cmath>

namespace
{
using Type = Oran7ThemeSource::Type;
using Fields = Oran7ThemeSource::Fields;

using namespace Oran7ThemeProfile;

void diagnostic(const QString& path, const QString& field, const QString& reason)
{
    qWarning().noquote() << "主题源" << path << "/" << field << "：" << reason;
}

QString resolvePath(const QString& owner, const QString& reference)
{
    if (reference.startsWith("qrc:/"))
        return reference.mid(3);
    const QUrl url(reference);
    if (url.isLocalFile())
        return url.toLocalFile();
    if (QDir::isAbsolutePath(reference) || owner.isEmpty())
        return reference;
    return QFileInfo(owner).dir().filePath(reference);
}

std::optional<QJsonObject> readObject(QString& path)
{
    if (!QFile::exists(path) && path.startsWith(":/config/themeJson/"))
        path = QDir(Oran7GlobalHelper::getConfigDir()).filePath(path.mid(9));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        diagnostic(path, "/", file.errorString());
        return std::nullopt;
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (!doc.isObject() || doc.object().isEmpty())
    {
        diagnostic(path, "/", error.error == QJsonParseError::NoError
                                  ? QStringLiteral("需要非空 JSON 对象") : error.errorString());
        return std::nullopt;
    }
    return doc.object();
}

void overlay(QVariantMap& target, const QVariantMap& values)
{
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        target.insert(it.key(), it.value());
}

QVariantMap legacyTokens(const QJsonObject& object)
{
    auto values = object.value("__init__").toObject().value("__vars__").toObject().toVariantMap();
    overlay(values, object.value("__style__").toObject().toVariantMap());
    return values;
}

bool isProfileToken(const QVariant& value)
{
    return value.isValid() && !value.isNull() && value.metaType().id() != QMetaType::QVariantList &&
           value.metaType().id() != QMetaType::QVariantMap;
}

bool hasOnlyKeys(const QJsonObject& object, const QStringList& keys, const QString& path)
{
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!keys.contains(it.key()))
        {
            diagnostic(path, it.key(), QStringLiteral("未知结构键"));
            return false;
        }
    return true;
}

std::optional<Oran7ThemeSource> readLayer(const QJsonObject& object, const QString& path,
                                         const QString& name, Fields fields)
{
    if (object.value("__schemaVersion__") != QJsonValue(1) ||
        (!name.isEmpty() && object.value("__name__").toString() != name))
    {
        diagnostic(path, name, QStringLiteral("源版本或组件标识不匹配"));
        return std::nullopt;
    }
    QStringList keys{"__schemaVersion__", "__tokens__", "__modes__"};
    if (!name.isEmpty())
        keys << "__name__";
    if (!hasOnlyKeys(object, keys, path))
        return std::nullopt;
    Oran7ThemeSource source;
    source.path = path;
    source.name = name;
    source.versioned = true;
    source.fields = std::move(fields);
    for (auto& field : source.fields)
        field.sourcePath = path;
    auto readTokens = [&](const QJsonValue& value, const QString& field, QVariantMap& target)
    {
        if (value.isUndefined())
            return true;
        if (!value.isObject())
        {
            diagnostic(path, field, QStringLiteral("需要对象"));
            return false;
        }
        const auto raw = value.toObject();
        for (auto it = raw.begin(); it != raw.end(); ++it)
        {
            if (!source.fields.contains(it.key()) || (!it->isString() && !it->isDouble() && !it->isBool()))
            {
                diagnostic(path, field + "." + it.key(), QStringLiteral("未知字段或不支持的字面值类型"));
                return false;
            }
        }
        target = raw.toVariantMap();
        return true;
    };
    const auto modes = object.value("__modes__");
    if (!modes.isUndefined() && !modes.isObject())
    {
        diagnostic(path, "__modes__", QStringLiteral("需要明暗模式对象"));
        return std::nullopt;
    }
    const auto byMode = modes.toObject();
    if (!hasOnlyKeys(byMode, {"light", "dark"}, path) ||
        !readTokens(object.value("__tokens__"), "__tokens__", source.common) ||
        !readTokens(byMode.value("light"), "__modes__.light", source.light) ||
        !readTokens(byMode.value("dark"), "__modes__.dark", source.dark))
        return std::nullopt;
    return source;
}

QVariant replaceModeReferences(const QVariant& value, bool dark)
{
    if (value.metaType().id() != QMetaType::QString)
        return value;
    auto text = value.toString();
    if (!text.startsWith('@') && !text.startsWith('$'))
        return value;
    static const QRegularExpression reference(R"(@\s*([A-Za-z0-9_-]+))");
    auto matches = reference.globalMatch(text);
    QString result;
    qsizetype offset = 0;
    while (matches.hasNext())
    {
        const auto match = matches.next();
        result += text.mid(offset, match.capturedStart() - offset);
        auto token = match.captured(1);
        for (const auto& alias : PlaylistAliases)
            if (token == QLatin1String(alias.key))
            {
                token = QLatin1String(dark ? alias.dark : alias.light);
                break;
            }
        result += '@' + token;
        offset = match.capturedEnd();
    }
    return result + text.mid(offset);
}
} // namespace

QJsonObject Oran7ThemeSource::profileDefaults() const
{
    if (!versioned)
        return legacyDefaults(legacy, name);
    QJsonObject result;
    setLayer(result, {}, QJsonObject::fromVariantMap(common));
    setLayer(result, modeName(false), QJsonObject::fromVariantMap(light));
    setLayer(result, modeName(true), QJsonObject::fromVariantMap(dark));
    return result;
}

QVariantMap Oran7ThemeSource::raw(bool isDark, const QJsonObject& profileGroup,
                                  const QVariantMap& temporary) const
{
    auto result = versioned ? common : legacyTokens(legacy);
    if (versioned)
        overlay(result, isDark ? dark : light);
    else if (name.isEmpty())
    {
        const auto bases = legacy.value("__init__").toObject().value("__base__").toObject();
        for (auto it = bases.begin(); it != bases.end(); ++it)
        {
            const auto pair = it->toString().split('|');
            if (pair.size() == 2)
                result[it.key()] = pair[isDark ? 1 : 0];
            else
                diagnostic(path, it.key(), QStringLiteral("旧基础色需要浅色|深色值"));
        }
    }

    const auto profileCommon = layer(profileGroup).toVariantMap();
    const auto profileLight = layer(profileGroup, modeName(false)).toVariantMap();
    const auto profileDark = layer(profileGroup, modeName(true)).toVariantMap();
    const auto apply = [&](const QVariantMap& tokens)
    {
        for (auto it = tokens.cbegin(); it != tokens.cend(); ++it)
        {
            auto key = it.key();
            // 旧别名由对应的模式层生成。未知扩展只保留在 profile 中。
            if (explicitMode(name, key).isEmpty() && !key.startsWith("__") && isProfileToken(it.value()) &&
                (versioned ? hasField(key) : result.contains(key)))
                result[key] = name == "Oran7MusicPlaylistView" ? replaceModeReferences(it.value(), isDark)
                                                              : it.value();
        }
    };
    apply(profileCommon);
    apply(isDark ? profileDark : profileLight);
    overlay(result, temporary);

    if (name == "Oran7MusicPlaylistView")
        for (const auto& alias : PlaylistAliases)
        {
            const QString key = QLatin1String(alias.key);
            for (bool mode : {false, true})
            {
                const QString old = QLatin1String(mode ? alias.dark : alias.light);
                const auto& defaultMode = mode ? dark : light;
                auto value = versioned ? defaultMode.value(key, common.value(key)) : result.value(old);
                const auto& profileMode = mode ? profileDark : profileLight;
                if (versioned || value.isValid())
                {
                    const auto shared = profileCommon.value(key);
                    const auto specific = profileMode.value(key);
                    if (isProfileToken(shared))
                        value = shared;
                    if (isProfileToken(specific))
                        value = specific;
                }
                value = temporary.value(old, temporary.value(key, value));
                if (value.isValid())
                    result[old] = replaceModeReferences(value, mode);
            }
            const QString activeAlias = QLatin1String(isDark ? alias.dark : alias.light);
            if (versioned && result.contains(activeAlias))
                result[key] = '@' + activeAlias;
        }
    return result;
}

Oran7ThemeSource::Fields::const_iterator Oran7ThemeSource::findField(const QString& key) const
{
    auto field = fields.constFind(key);
    if (field == fields.cend() && name == "Oran7MusicPlaylistView")
        for (const auto& alias : PlaylistAliases)
            if (key == QLatin1String(alias.light) || key == QLatin1String(alias.dark))
            {
                field = fields.constFind(QLatin1String(alias.key));
                break;
            }
    return field;
}

bool Oran7ThemeSource::hasField(const QString& key) const
{
    if (versioned)
        return findField(key) != fields.cend();
    const auto init = legacy.value("__init__").toObject();
    const auto style = legacy.value("__style__").toObject();
    const auto vars = init.value("__vars__").toObject();
    const auto contains = [&](const QString& token) { return style.contains(token) || vars.contains(token); };
    if (name == "Oran7MusicPlaylistView")
        for (const auto& alias : PlaylistAliases)
            if (key == QLatin1String(alias.key))
                return contains(QLatin1String(alias.light)) || contains(QLatin1String(alias.dark));
    return contains(key) ||
           (name.isEmpty() && init.value("__base__").toObject().contains(key));
}

QString Oran7ThemeSource::fieldFile(const QString& key) const
{
    const auto field = findField(key);
    return field == fields.cend() ? path : field->sourcePath;
}

QString Oran7ThemeSource::fieldLocation(const QString& key, bool isDark) const
{
    auto token = key;
    const auto explicitLayer = explicitMode(name, token);
    const bool darkLayer = explicitLayer.isEmpty() ? isDark : explicitLayer == "dark";
    if ((darkLayer ? dark : light).contains(token))
        return QStringLiteral("__modes__.") + modeName(darkLayer) + '.' + token;
    return QStringLiteral("__tokens__.") + token;
}

bool Oran7ThemeSource::affects(const QString& editedKey, const QString& evaluatedKey) const
{
    if (editedKey == evaluatedKey)
        return true;
    if (name == "Oran7MusicPlaylistView")
        for (const auto& alias : PlaylistAliases)
            if (editedKey == QLatin1String(alias.key))
                return evaluatedKey == QLatin1String(alias.light) || evaluatedKey == QLatin1String(alias.dark);
    return false;
}

bool Oran7ThemeSource::accepts(const QString& key, const QVariant& value) const
{
    const auto field = findField(key);
    if (field == fields.cend())
        return false;
    const auto type = value.metaType().id();
    switch (field->type)
    {
    case Type::OptionalColor:
        if (type == QMetaType::QString && value.toString().isEmpty())
            return true;
        [[fallthrough]];
    case Type::Color:
        return type == QMetaType::QColor && value.value<QColor>().isValid();
    case Type::ColorName:
        return (type == QMetaType::QColor && value.value<QColor>().isValid()) ||
               (type == QMetaType::QString && QColor::isValidColorName(value.toString()));
    case Type::LegacyInteger:
        if (type == QMetaType::QString)
        {
            bool ok = false;
            const auto number = value.toString().toLongLong(&ok);
            return ok && number >= field->minimum && number <= field->maximum;
        }
        [[fallthrough]];
    case Type::Integer:
    case Type::Number:
    {
        if (type != QMetaType::Int && type != QMetaType::LongLong && type != QMetaType::Double)
            return false;
        const double number = value.toDouble();
        return std::isfinite(number) && number >= field->minimum && number <= field->maximum &&
               (field->type == Type::Number || std::floor(number) == number);
    }
    case Type::Boolean:
        return type == QMetaType::Bool;
    case Type::String:
        return type == QMetaType::QString;
    case Type::Resource:
        return type == QMetaType::QString &&
               (value.toString().isEmpty() || QDir::isAbsolutePath(value.toString()) || QUrl(value.toString()).isValid());
    case Type::Choice:
        return type == QMetaType::QString && field->choices.contains(value.toString());
    }
    return false;
}

std::optional<Oran7ThemeSource> Oran7ThemeSources::readComponent(const QString& reference, const QString& name)
{
    auto path = resolvePath({}, reference);
    // 旧自定义 Index 仍可引用此前公开的内置资源路径。
    if (path == QStringLiteral(":/config/themeJson/") + name + ".json" && !QFile::exists(path))
        path = QStringLiteral(":/config/themeJson/components/") + name + ".json";
    const auto object = readObject(path);
    if (!object)
        return std::nullopt;
    if (object->contains("__schemaVersion__"))
    {
        auto fields = Oran7ThemeSchema::component(name);
        if (fields.isEmpty())
        {
            diagnostic(path, name, QStringLiteral("组件未登记字段规范"));
            return std::nullopt;
        }
        return readLayer(*object, path, name, std::move(fields));
    }
    if (!object->value("__init__").isObject() && !object->value("__style__").isObject())
    {
        diagnostic(path, name, QStringLiteral("无法识别旧组件源，检查源版本及令牌容器"));
        return std::nullopt;
    }
    Oran7ThemeSource source;
    source.path = path;
    source.name = name;
    source.legacy = *object;
    return source;
}

std::optional<Oran7ThemeSources> Oran7ThemeSources::read(const QString& reference, const QStringList& groups)
{
    auto path = resolvePath({}, reference);
    const auto index = readObject(path);
    if (!index)
        return std::nullopt;
    Oran7ThemeSources result;
    if (index->contains("__schemaVersion__"))
    {
        if (index->value("__schemaVersion__") != QJsonValue(1) ||
            !hasOnlyKeys(*index, {"__schemaVersion__", "__foundations__", "__colors__", "__component__"}, path))
        {
            diagnostic(path, "__schemaVersion__", QStringLiteral("不支持的主题清单"));
            return std::nullopt;
        }
        const bool complete = path == DefaultIndex;
        auto readGlobal = [&](const QString& key, const Fields& fields) -> std::optional<Oran7ThemeSource>
        {
            const auto ref = index->value(key).toString();
            if (ref.isEmpty())
            {
                diagnostic(path, key, QStringLiteral("缺少资源引用"));
                return std::nullopt;
            }
            auto file = resolvePath(path, ref);
            const auto object = readObject(file);
            return object ? readLayer(*object, file, {}, fields) : std::nullopt;
        };
        auto base = readGlobal("__foundations__", Oran7ThemeSchema::foundations(complete));
        const auto palette = readGlobal("__colors__", Oran7ThemeSchema::colors(complete));
        if (!base || !palette)
            return std::nullopt;
        for (auto it = palette->fields.begin(); it != palette->fields.end(); ++it)
            if (base->fields.contains(it.key()))
            {
                diagnostic(path, it.key(), QStringLiteral("全局令牌在两个文件中重复声明"));
                return std::nullopt;
            }
        overlay(base->common, palette->common);
        overlay(base->light, palette->light);
        overlay(base->dark, palette->dark);
        base->fields.insert(palette->fields);
        base->path = path;
        result.primary = std::move(*base);
    }
    else
    {
        result.primary.path = path;
        result.primary.legacy = *index;
    }
    const auto refs = index->value("__component__").toObject();
    if (refs.size() != groups.size())
    {
        diagnostic(path, "__component__", QStringLiteral("组件清单与已注册分组不一致"));
        return std::nullopt;
    }
    for (const auto& group : groups)
    {
        const auto ref = refs.value(group).toString();
        if (ref.isEmpty())
        {
            diagnostic(path, "__component__." + group, QStringLiteral("缺少已注册组件资源"));
            return std::nullopt;
        }
        auto component = readComponent(resolvePath(path, ref), group);
        if (!component)
            return std::nullopt;
        result.components.insert(group, std::move(*component));
    }
    return result;
}
