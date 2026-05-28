#ifndef ORAN7THEME_P_H
#define ORAN7THEME_P_H

#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include "Oran7Theme.h"

/**
 * @brief Index.json tools functions
 */
enum class TokenFunction : uint16_t
{
    GenColor,
    GenFontFamily,
    GenFontSize,
    GenFontLineHeight,
    GenRadius,

    Darker,
    Lighter,
    Brightness,
    Alpha,
    OnBackground,

    Multiply
};

/**
 *@brief global Component Property HashTable
 */
using ComponentPropertyHash = QHash<QString,QVariantMap *>;
Q_GLOBAL_STATIC(ComponentPropertyHash,g_componentTable)

struct  Oran7ThemeData{
    struct Component{
        QString  path;
        QVariantMap *tokenMap = nullptr;
        QMap<QString,QVariant> installTokenMap;
    };
    QObject *themeObject;
    QMap<QString,Oran7ThemeData::Component> componentMap;
};

class Oran7ThemePrivate
{
public:
    Oran7ThemePrivate(Oran7Theme *q): q_ptr(q){}
    ~Oran7ThemePrivate();

    Q_DECLARE_PUBLIC(Oran7Theme);
    Oran7Theme *q_ptr {nullptr};

    static Oran7ThemePrivate *get(Oran7Theme *theme){return theme->d_func();};

    void initializeComponentPropertyHash();

public:
    QString themIndexJsonPath = ":/config/themeJson/Index.json";
    QJsonObject m_indexJsonObject;
    QMap<QString,QVariant> m_indexTokenTable;
    QMap<QString,QMap<QString,QVariant>> m_componentTokenTable;

    QMap<QObject* ,Oran7ThemeData> m_defaultTheme;
    QMap<QObject* ,Oran7ThemeData> m_customTheme;//Discard

    Oran7Theme::DarkMode m_darkMode = Oran7Theme::DarkMode::Light;
    QVariantMap m_sizeHintMap;//global scale size
    QString m_currentThemeProfile = QString("Profile-default");  // 当前使用的主题配置名

public:
    void parse$(QMap<QString,QVariant> &out,const QString &tokenName,const QString &expr);

    QColor colorFromIndexTable(const QString &tokenName);
    qreal numberFromIndexTable(const QString &tokenName);
    void parseIndexExpr(const QString &tokenName, const QString &expr);
    void parseComponentExpr(QVariantMap *tokenMapPtr, const QString &tokenName, const QVariant &expr);

    void reloadIndexTheme();
    void reloadComponentTheme(const QMap<QObject *, Oran7ThemeData> &dataMap);
    bool reloadComponentImport(QJsonObject &style, const QString &componentName);
    void reloadComponentThemeFile(QObject *themeObject, const QString &componentName, const Oran7ThemeData::Component &componentTheme);
    void reloadDefaultComponentTheme();
    void reloadCustomComponentTheme();

    void registerDefaultComponentTheme(const QString &component, const QString &themePath);
    void registerComponentTheme(QObject *themeObject,
                                const QString &component,
                                QVariantMap *themeMap,
                                const QString &themePath,
                                QMap<QObject *, Oran7ThemeData> &dataMap);

    // 合并两个 JSON 对象（递归）
    static void mergeJsonObject(QJsonObject& target, const QJsonObject& source);

    // 从 Oran7ThemeProfileManager 加载当前主题
    void loadCurrentThemeProfile();
};

#endif // ORAN7THEME_P_H
