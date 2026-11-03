#include "Oran7UiPreferences.h"
#include "Oran7AppConfigManager.h"
#include <QGuiApplication>
#include <QFontDatabase>
#include <QQmlEngine>

Oran7UiPreferences::Oran7UiPreferences(Oran7AppConfigManager& config, QObject* parent)
    : QObject(parent), m_config(config)
{
    const auto saved = config.snapshot().appearance;
    m_language = saved.language;
    m_fontFamily = saved.fontFamily;
    m_fontPixelSize = saved.fontPixelSize;
    if (!m_english.load(":/i18n/oran7_en_US.qm"))
    {
        error(QStringLiteral("无法加载英文翻译资源"));
        m_language = "zh_CN";
    }
    if (m_language == "en_US")
        m_installed = QCoreApplication::installTranslator(&m_english);
    applyFont();
}
Oran7UiPreferences::~Oran7UiPreferences()
{
    if (m_installed)
        QCoreApplication::removeTranslator(&m_english);
}
void Oran7UiPreferences::attachEngine(QQmlEngine* engine)
{
    m_engine = engine;
}
QStringList Oran7UiPreferences::fontFamilies() const
{
    return QFontDatabase::families();
}
bool Oran7UiPreferences::error(const QString& text)
{
    m_lastError = text;
    emit lastErrorChanged();
    return false;
}
QString Oran7UiPreferences::translate(const QString& source) const
{
    return QCoreApplication::translate("Oran7Ui", source.toUtf8().constData());
}
bool Oran7UiPreferences::selectLanguage(const QString& language)
{
    if (language != "zh_CN" && language != "en_US")
        return error(QStringLiteral("不支持的界面语言"));
    if (language == m_language)
        return true;
    if (language == "en_US" && m_english.isEmpty())
        return error(QStringLiteral("无法加载英文翻译资源"));
    const auto previous = m_config.snapshot();
    auto next = previous;
    next.appearance.language = language;
    m_config.updateConfig(next);
    if (!m_config.saveConfig())
    {
        m_config.updateConfig(previous);
        return error(QStringLiteral("语言设置保存失败"));
    }
    if (m_installed)
    {
        QCoreApplication::removeTranslator(&m_english);
        m_installed = false;
    }
    if (language == "en_US")
        m_installed = QCoreApplication::installTranslator(&m_english);
    m_language = language;
    m_lastError.clear();
    emit lastErrorChanged();
    // 翻译器就绪后刷新绑定，命令产生的状态文案由页面按语言通知重新派生。
    if (m_engine)
        m_engine->retranslate();
    emit languageChanged();
    return true;
}
bool Oran7UiPreferences::selectFont(const QString& family, int pixelSize)
{
    if (!fontFamilies().contains(family) || pixelSize < 10 || pixelSize > 24)
        return error(QStringLiteral("无效的字体或字号"));
    if (family == m_fontFamily && pixelSize == m_fontPixelSize)
        return true;
    const auto previous = m_config.snapshot();
    auto next = previous;
    next.appearance.fontFamily = family;
    next.appearance.fontPixelSize = pixelSize;
    m_config.updateConfig(next);
    if (!m_config.saveConfig())
    {
        m_config.updateConfig(previous);
        return error(QStringLiteral("字体设置保存失败"));
    }
    m_fontFamily = family;
    m_fontPixelSize = pixelSize;
    m_lastError.clear();
    applyFont();
    emit fontChanged();
    emit lastErrorChanged();
    return true;
}
QFont Oran7UiPreferences::font() const
{
    return QGuiApplication::font();
}

void Oran7UiPreferences::applyFont()
{
    auto families = QStringList{m_fontFamily, "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI"};
    const auto installed = fontFamilies();
    if (!installed.contains(m_fontFamily))
    {
        for (const auto& family : families)
            if (installed.contains(family))
            {
                m_fontFamily = family;
                break;
            }
        if (!installed.contains(m_fontFamily) && !installed.isEmpty())
            m_fontFamily = installed.first();
        families.prepend(m_fontFamily);
    }
    families.removeDuplicates();
    QFont font;
    font.setFamilies(families);
    font.setPixelSize(m_fontPixelSize);
    QGuiApplication::setFont(font);
}
