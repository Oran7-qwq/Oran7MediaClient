#pragma once
#include <QObject>
#include <QFont>
#include <QPointer>
#include <QTranslator>
#include <QStringList>
class QQmlEngine;
class Oran7AppConfigManager;

#ifdef Oran7Core_EXPORTS
#define ORAN7_UI_PREFERENCES_EXPORT Q_DECL_EXPORT
#else
#define ORAN7_UI_PREFERENCES_EXPORT Q_DECL_IMPORT
#endif

// 应用拥有的界面偏好服务；QML 借用，测试可注入独立配置管理器。
class ORAN7_UI_PREFERENCES_EXPORT Oran7UiPreferences : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(QFont font READ font NOTIFY fontChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY fontChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize NOTIFY fontChanged)
    Q_PROPERTY(QStringList fontFamilies READ fontFamilies CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
  public:
    explicit Oran7UiPreferences(Oran7AppConfigManager& config, QObject* parent = nullptr);
    ~Oran7UiPreferences() override;
    void attachEngine(QQmlEngine* engine);
    QString language() const { return m_language; }
    QFont font() const;
    QString fontFamily() const { return m_fontFamily; }
    int fontPixelSize() const { return m_fontPixelSize; }
    QStringList fontFamilies() const;
    QString lastError() const { return m_lastError; }
    Q_INVOKABLE bool selectLanguage(const QString& language);
    Q_INVOKABLE bool selectFont(const QString& family, int pixelSize);
    Q_INVOKABLE QString translate(const QString& source) const;
  signals:
    void languageChanged();
    void fontChanged();
    void lastErrorChanged();

  private:
    void applyFont();
    bool error(const QString& text);
    Oran7AppConfigManager& m_config;
    QPointer<QQmlEngine> m_engine;
    QTranslator m_english;
    QString m_language, m_fontFamily, m_lastError;
    int m_fontPixelSize{};
    bool m_installed = false;
};
