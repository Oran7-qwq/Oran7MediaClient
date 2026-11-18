#pragma once

#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QVariantMap>
#include <optional>

// 主题源和 profile 的覆盖在求值前合成。
struct Oran7ThemeSource
{
    enum class Type { Color, OptionalColor, ColorName, Integer, LegacyInteger, Number, Boolean, String, Resource, Choice };
    struct Oran7Field
    {
        Type type;
        double minimum = 0;
        double maximum = 0;
        QStringList choices;
        bool required = true;
        QString sourcePath;
    };
    using Fields = QMap<QString, Oran7Field>;

    QString path;
    QString name;
    QJsonObject legacy;
    QVariantMap common, light, dark;
    Fields fields;
    bool versioned = false;

    QJsonObject profileDefaults() const;
    QVariantMap raw(bool isDark, const QJsonObject& profileGroup = {},
                    const QVariantMap& temporary = {}) const;
    bool hasField(const QString& key) const;
    bool accepts(const QString& key, const QVariant& value) const;
    bool affects(const QString& editedKey, const QString& evaluatedKey) const;
    QString fieldFile(const QString& key) const;
    QString fieldLocation(const QString& key, bool isDark) const;

  private:
    Fields::const_iterator findField(const QString& key) const;
};

struct Oran7ThemeSources
{
    static constexpr auto DefaultIndex = ":/config/themeJson/Index.json";
    Oran7ThemeSource primary;
    QMap<QString, Oran7ThemeSource> components;
    static std::optional<Oran7ThemeSources> read(const QString& path, const QStringList& groups);
    static std::optional<Oran7ThemeSource> readComponent(const QString& path, const QString& name);
};
