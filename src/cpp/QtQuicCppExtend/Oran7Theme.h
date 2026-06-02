#ifndef ORAN7THEME_H
#define ORAN7THEME_H

#include <QObject>
#include <QtQml/qqml.h>
#include <QColor>
#include <QVariantMap>

#include "Oran7Definitions.h"
#include "GlobalHelper.h"

QT_FORWARD_DECLARE_CLASS(Oran7ThemeProfileManager)

QT_FORWARD_DECLARE_CLASS(Oran7ThemePrivate)

class Oran7Theme : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Oran7Theme)

    Q_PROPERTY(bool isDark READ isDark NOTIFY isDarkChanged FINAL)
    Q_PROPERTY(DarkMode darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged FINAL)
    Q_PROPERTY(QVariantMap sizeHint READ sizeHint NOTIFY sizeHintChanged FINAL)
    Q_PROPERTY(QString currentThemeProfile READ currentThemeProfile WRITE setCurrentThemeProfile NOTIFY currentThemeProfileChanged FINAL)
    Q_PROPERTY(QStringList availableThemeProfiles READ availableThemeProfiles NOTIFY availableThemeProfilesChanged FINAL)

    ORAN7_PROPERTY_INIT(bool, animationEnabled, setAnimationEnabled, true);

    /**
     *  @brief Index.josn properties
     */
    ORAN7_PROPERTY_READONLY(QVariantMap,Primary);

    /**
     * @brief 注册组件时：
     * 1.Add in Oran7Theme.h：ORAN7_PROPERTY_READONLY(QVariantMap,ComponentName)
     * 2.Add in Oran7Theme.cpp Oran7ThemePrivate::initializeComponentPropertyHash()：ADD_COMPONENT_PROPERTY(ComponentName)
     * 3.Add in Index.json '__component__'："ComponentName":"<Component.jons of Path>"
     * 4.Add link in cmake for <Component.jons of Path>
     */
    ORAN7_PROPERTY_READONLY(QVariantMap,Oran7ProgressSlider);
    ORAN7_PROPERTY_READONLY(QVariantMap,Oran7MainGUI);
    ORAN7_PROPERTY_READONLY(QVariantMap,Oran7CaptionBar);
    ORAN7_PROPERTY_READONLY(QVariantMap,Oran7MusicPlaylistView)
    ORAN7_PROPERTY_READONLY(QVariantMap,Oran7MusicPlayControls)

public:
    ~Oran7Theme();

    static Oran7Theme& instance();
    static Oran7Theme *create(QQmlEngine *, QJSEngine *);
    static void cleanup(){
        if(s_instance){
            delete s_instance;
            s_instance = nullptr;
            INFO_LOG << "[DTOR] Oran7Theme::ins cleanup finished.";
        }
    }

    enum DarkMode{
        Light = 0,
        Dark,
        System
    };
    Q_ENUM(DarkMode);

signals:
    void isDarkChanged();
    void darkModeChanged();
    void sizeHintChanged();
    void currentThemeProfileChanged();
    void availableThemeProfilesChanged();

public:
    bool isDark() const;

    DarkMode darkMode() const;
    void setDarkMode(const Oran7Theme::DarkMode mode);

    QVariantMap sizeHint() const;

public:
    QString currentThemeProfile() const;
    void setCurrentThemeProfile(const QString& profileName);
    QStringList availableThemeProfiles() const;

    /**
     * @brief saveComponentToken
     * @param component
     * @param token
     * @param value
     */
    Q_INVOKABLE void saveComponentToken(const QString& component, const QString& token, const QVariant& value);
    /**
     * @brief saveIndexToken
     * @param token
     * @param value
     */
    Q_INVOKABLE void saveIndexToken(const QString& token, const QVariant& value);

    Q_INVOKABLE bool createThemeProfile(const QString& profileName, const QString& sourceProfile = "profile-default");
    Q_INVOKABLE bool deleteThemeProfile(const QString& profileName);
    Q_INVOKABLE bool duplicateThemeProfile(const QString& sourceName, const QString& newName);
    Q_INVOKABLE bool exportThemeProfile(const QString& profileName, const QString& filePath);
    Q_INVOKABLE QString importThemeProfile(const QString& filePath, const QString& newName = "");
    Q_INVOKABLE void resetToDefaultTheme();

public:
    /**
     * @brief Registers
     * @param themeObject ：ThemeObject pointer.
     * @param component : Component of name.
     * @param themeMap : Component theme of map.
     * @param  themePath : Component theme json path.
     *
     * @note Dicard 2026/5/26
     */
    void registerCustomComponentTheme(QObject *themeObject,const QString& component,QVariantMap* themeMap,const QString &themePath);

    /**
     * @brief Reloade globalTheme property (Recalculate)
     */
    Q_INVOKABLE void reloadTheme();

    /**
     * @brief 设置文本基础色{Oran7Theme.Primary.colorTextBase}
     * @param lightAndDark 明亮和暗黑模式颜色字符串,类似于{#000|#fff}
     */
    Q_INVOKABLE void installThemeColorTextBase(const QString &lightAndDark);
    /**
     * @brief 设置背景基础色{HusTheme.Primary.colorBgBase}
     * @param lightAndDark 明亮和暗黑模式颜色字符串,类似于{#fff|#000}
     */
    Q_INVOKABLE void installThemeColorBgBase(const QString &lightAndDark);
    /**
     * @brief 设置主基础色{HusTheme.Primary.colorPrimaryBase}
     * @param color 主基础颜色
     */
    Q_INVOKABLE void installThemePrimaryColorBase(const QColor &colorBase);
    /**
     * @brief 设置字体基础大小{HusTheme.Primary.fontSizeBase}
     * @param fontSizeBase 基础字体像素大小
     */
    Q_INVOKABLE void installThemePrimaryFontSizeBase(int fontSizeBase);
    /**
     * @brief 设置基础字体族{HusTheme.Primary.fontFamilyBase}
     * @param familiesBase 基础字体族
     */
    Q_INVOKABLE void installThemePrimaryFontFamiliesBase(const QString &familiesBase);
    /**
     * @brief 设置圆角半径基础大小{HusTheme.Primary.radiusBase}
     * @param radiusBase 基础圆角半径大小
     */
    Q_INVOKABLE void installThemePrimaryRadiusBase(int radiusBase);
    /**
     * @brief 设置动画基础速度
     * @param durationFast [Fast 动画持续时间(ms)]
     * @param durationMid  [Mid  动画持续时间(ms)]
     * @param duratoinSlow [Slow 动画持续时间(ms)]
     */
    Q_INVOKABLE void installThemePrimaryAnimationBase(int durationFast, int durationMid, int durationSlow);
    /**
     * @brief 设置尺寸提示比率
     * @param size  尺寸名
     * @param ratio 比率
     */
    Q_INVOKABLE void installSizeHintRatio(const QString &size, qreal ratio);


    /**
     * @brief 设置Index主题
     * @param themePath 主题路径(为空时重置为默认)
     */
    Q_INVOKABLE void installIndexTheme(const QString &themePath);
    /**
     * @brief 设置Index主题令牌
     * @param token 令牌名
     * @param value 令牌值
     * @warning 支持Token生成函数(genColor/genFont/genFontSize/genRadius)
     */
    Q_INVOKABLE void installIndexToken(const QString &token, const QString &value);

    /**
     * @brief 设置组件主题
     * @param component 组件名称
     * @param themePath 主题路径
     */
    Q_INVOKABLE void installComponentTheme(const QString &component, const QString &themePath);
    /**
     * @brief 设置组件主题令牌
     * @note This api will not save the config,but only change the memory config state
     * @param component 组件名称
     * @param token 令牌名
     * @param value 令牌值
     */
    Q_INVOKABLE void installComponentToken(const QString &component, const QString &token, const QVariant &value);

    /**
     * @brief installThemeComponentColorBase
     * @param colorBase
     */
    Q_INVOKABLE void installThemeComponentColorBase(const QString &component,const QColor &colorBase);

private:
    explicit Oran7Theme(QObject *parent = nullptr);
    static Oran7Theme *s_instance;

    Q_DECLARE_PRIVATE(Oran7Theme);
    QScopedPointer<Oran7ThemePrivate> d_ptr;
};

#endif // ORAN7THEME_H

/**

  ┌───────────┬───────────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │   分类    │                                                已解析的项                                                 │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ base      │ colorTextBase, colorBgBase                                                                                │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ vars      │ fontSizeBase-1~10, fontLineHeightBase-1~10, radiusBase-1~5, colorPrimaryBase-1~11, colorSuccessBase-1~11, │
  │ 生成序列  │  colorWarningBase-1~11, colorErrorBase-1~11, colorInfoBase-1~11, colorLinkBase-1~11                       │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ vars 单项 │ fontFamilyBase                                                                                            │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 字体      │ fontPrimaryFamily, fontPrimarySize~Heading5, fontPrimaryHeight, fontPrimaryLineHeightHeading1~5           │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 圆角      │ radiusPrimary, radiusPrimaryLG/SM/XS/Outer                                                                │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 时长      │ durationFast/Mid/Slow                                                                                     │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 文本颜色  │ colorTextPrimary/Secondary/Tertiary/Quaternary                                                            │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 填充颜色  │ colorFill, colorFillPrimary/Secondary/Tertiary/Quaternary                                                 │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 背景颜色  │ colorBgSolid/Hover/Active, colorBgContainer/Elevated/Layout/Spotlight/Blur/Mask                           │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 边框      │ colorBorder, colorBorderSecondary                                                                         │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Primary   │ colorPrimary, colorPrimaryBg/Hover/Active, colorPrimaryBorder/Hover/Active,                               │
  │ 系列      │ colorPrimaryText/Hover/Active, colorPrimaryContainerBg/Hover/Active, colorPrimaryContainerBgDisabled,     │
  │           │ colorPrimaryTextDisabled                                                                                  │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Success   │ colorSuccess, colorSuccessBg/Hover, colorSuccessBorder/Hover, colorSuccessText/Hover/Active               │
  │ 系列      │                                                                                                           │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Error     │ colorError, colorErrorBg/Hover/Active, colorErrorBorder/Hover, colorErrorText/Hover/Active                │
  │ 系列      │                                                                                                           │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Warning   │ colorWarning, colorWarningBg/Hover, colorWarningBorder/Hover, colorWarningText/Hover/Active               │
  │ 系列      │                                                                                                           │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Info 系列 │ colorInfo, colorInfoBg/Hover, colorInfoBorder/Hover, colorInfoText/Hover/Active                           │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Link 系列 │ colorLink, colorLinkHover/Active                                                                          │
  ├───────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Main 系列 │ colorMain-1~9                                                                                            │
  └───────────┴───────────────────────────────────────────────────────────────────────────────────────────────────────────┘

*/
