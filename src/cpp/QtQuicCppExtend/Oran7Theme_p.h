#pragma once
#include "Oran7Theme.h"
#include <QJsonObject>
#include <functional>

class Oran7ThemePrivate
{
  public:
    explicit Oran7ThemePrivate(Oran7Theme* q) : q_ptr(q) {}
    Q_DECLARE_PUBLIC(Oran7Theme)
    Oran7Theme* q_ptr;
    struct Oran7ThemeGroup
    {
        QVariantMap* values;
        std::function<void()> notify;
    };
    QMap<QString, Oran7ThemeGroup> groups;
    QString themIndexJsonPath = ":/config/themeJson/Index.json";
    QString m_currentThemeProfile = "profile-default";
    Oran7Theme::DarkMode m_darkMode = Oran7Theme::Dark;
    bool lastEffectiveDark = true;
    QVariantMap m_sizeHintMap{{"small", 0.8}, {"normal", 1.0}, {"large", 1.25}};
    QJsonObject index, profile;
    QVariantMap primary;
    QVariantMap transientGlobal;
    QMap<QString, QVariantMap> transient;
    void initializeComponentPropertyHash();
    void rebuild(const QString& component = {});
    void refreshDark(bool oldDark, Oran7Theme::DarkMode oldMode);
    bool saveProfile(const QJsonObject& value);
};
