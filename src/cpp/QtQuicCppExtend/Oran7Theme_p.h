#pragma once
#include "Oran7Theme.h"
#include "Oran7ThemeSource_p.h"
#include <QJsonObject>
#include <functional>

class Oran7ThemePrivate
{
  public:
    Oran7ThemePrivate(Oran7Theme* q, Oran7ThemeProfileManager& profiles, Oran7AppConfigManager& appConfig)
        : q_ptr(q), profiles(profiles), appConfig(appConfig) {}
    Q_DECLARE_PUBLIC(Oran7Theme)
    Oran7Theme* q_ptr;
    Oran7ThemeProfileManager& profiles;
    Oran7AppConfigManager& appConfig;
    struct Oran7ThemeGroup
    {
        QVariantMap* values;
        std::function<void()> notify;
    };
    QMap<QString, Oran7ThemeGroup> groups;
    QString indexPath = Oran7ThemeSources::DefaultIndex;
    QString m_currentThemeProfile = "profile-default";
    Oran7Theme::DarkMode m_darkMode = Oran7Theme::Dark;
    bool lastEffectiveDark = true;
    QVariantMap m_sizeHintMap{{"small", 0.8}, {"normal", 1.0}, {"large", 1.25}};
    Oran7ThemeSources sources;
    QJsonObject profile;
    QVariantMap primary;
    QVariantMap transientGlobal;
    QMap<bool, QVariantMap> transientBaseColors;
    QMap<QString, QVariantMap> transient;
    void initializeComponentPropertyHash();
    void reload(const QString& path);
    void applySources(Oran7ThemeSources next, const QString& path, QJsonObject nextProfile);
    QVariantMap globalPreview(bool dark) const;
    void clearPreview();
    void rebuild(const QString& component = {});
    void refreshDark(bool oldDark, Oran7Theme::DarkMode oldMode);
    bool saveProfile(const QJsonObject& value);
    bool resetGroup(const QString& component, QString token, bool currentModeOnly);
};
