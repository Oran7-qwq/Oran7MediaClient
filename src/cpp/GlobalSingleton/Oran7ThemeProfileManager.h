#ifndef ORAN7THEMEPROFILEMANAGER_H
#define ORAN7THEMEPROFILEMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QStringList>

#include "GlobalHelper.h"
class Oran7Theme;

/**
 * @brief Oran7ThemeProfileManager - 主题配置文件管理器
 *
 * 负责管理用户主题配置文件的存储、加载、保存等操作
 * 主题配置文件存储在 QStandardPaths::AppDataLocation 目录下
 * 文件命名格式: profile-*.json (如 profile-default.json, profile-1.json)
 */
class Oran7ThemeProfileManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return Oran7ThemeProfileManager 单例引用
     */
    static Oran7ThemeProfileManager& instance();

    /**
     * @brief 获取主题配置文件存储目录
     * @return 主题目录路径
     */
    QString themeProfilesDir() const;

    /**
     * @brief 确保主题目录存在，不存在则创建
     */
    void ensureThemeProfilesDir();

    /**
     * @brief 首次运行时初始化默认主题
     *        从资源文件复制默认主题配置到用户目录
     */
    void initializeDefaultTheme(Oran7Theme *q);

    /**
     * @brief 加载指定主题配置
     * @param profileName 主题配置名称 (如 "profile-default")
     * @return 主题配置 JSON 对象，如果文件不存在则返回空对象
     */
    QJsonObject loadThemeProfile(const QString& profileName);

    /**
     * @brief 保存主题配置
     * @param profileName 主题配置名称
     * @param theme 主题配置 JSON 对象
     * @return 保存成功返回 true
     */
    bool saveThemeProfile(const QString& profileName, const QJsonObject& theme);

    /**
     * @brief 获取所有可用的主题配置列表
     * @return 主题配置名称列表
     */
    QStringList getAvailableThemeProfiles() const;

    /**
     * @brief 删除指定的主题配置
     * @param profileName 主题配置名称
     * @return 删除成功返回 true
     */
    bool deleteThemeProfile(const QString& profileName);

    /**
     * @brief 复制主题配置
     * @param sourceName 源主题配置名称
     * @param newName 新主题配置名称
     * @return 复制成功返回 true
     */
    bool duplicateThemeProfile(const QString& sourceName, const QString& newName);

    /**
     * @brief 导出主题配置到指定路径
     * @param profileName 主题配置名称
     * @param filePath 导出文件路径
     * @return 导出成功返回 true
     */
    bool exportThemeProfile(const QString& profileName, const QString& filePath);

    /**
     * @brief 从指定路径导入主题配置
     * @param filePath 导入文件路径
     * @param newName 新主题配置名称（为空时使用原文件名）
     * @return 导入成功返回主题配置名称，失败返回空字符串
     */
    QString importThemeProfile(const QString& filePath, const QString& newName = "");

signals:
    /**
     * @brief 主题配置改变信号
     * @param profileName 主题配置名称
     */
    void themeProfileChanged(const QString& profileName);

    /**
     * @brief 主题配置列表改变信号
     */
    void availableThemeProfilesChanged();

private:
    explicit Oran7ThemeProfileManager(QObject* parent = nullptr);
    ~Oran7ThemeProfileManager(){INFO_LOG<<"[DTOR] Oran7ThemeProfileManager::~Oran7ThemeProfileManager() finished.";};
    Oran7ThemeProfileManager(const Oran7ThemeProfileManager&) = delete;
    Oran7ThemeProfileManager& operator=(const Oran7ThemeProfileManager&) = delete;

    /**
     * @brief 根据主题名称获取完整文件路径
     * @param profileName 主题配置名称
     * @return 完整文件路径
     */
    QString profileFilePath(const QString& profileName) const;

    /**
     * @brief 合并两个 JSON 对象（递归）
     * @param target 目标对象（会被修改）
     * @param source 源对象
     */
    void mergeMissingJsonObject(QJsonObject& target,const QJsonObject& source);

    /**
     * @brief 验证主题配置名称是否有效
     * @param profileName 主题配置名称
     * @return 有效返回 true
     */
    bool isValidProfileName(const QString& profileName) const;
};

#endif // ORAN7THEMEPROFILEMANAGER_H
