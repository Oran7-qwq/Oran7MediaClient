#include "Oran7ThemeProfileManager.h"
#include "GlobalHelper.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

Oran7ThemeProfileManager& Oran7ThemeProfileManager::instance()
{
    static Oran7ThemeProfileManager instance;
    return instance;
}

Oran7ThemeProfileManager::Oran7ThemeProfileManager(QObject* parent)
    : QObject(parent)
{
}

QString Oran7ThemeProfileManager::themeProfilesDir() const
{
    QString configDir = GlobalHelper::getConfigDir();
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return configDir;
}

void Oran7ThemeProfileManager::ensureThemeProfilesDir()
{
    QDir dir(themeProfilesDir());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

void Oran7ThemeProfileManager::initializeDefaultTheme(Oran7Theme *q)
{
    INFO_LOG << "Enter Oran7ThemeProfileManager::initializeDefaultTheme().";
    QString profilesDir = themeProfilesDir();
    QString defaultProfilePath = profilesDir + "/profile-default.json";
    CONFIG_LOG<<"ThemeProfilesDir:"<<defaultProfilePath;

#if !DEVELOPER_MODE
    if(QFile(defaultProfilePath).exists())
        return;
#endif

   /*! @note 加载 Index.json 获取组件列表 */
    QFile indexFile(":/config/themeJson/Index.json");
    if (!indexFile.exists() || !indexFile.open(QIODevice::ReadOnly)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Cannot find Index.json in resources";
        return;
    }

    QJsonParseError error;
    QJsonDocument indexDoc = QJsonDocument::fromJson(indexFile.readAll(), &error);
    indexFile.close();

    if (error.error != QJsonParseError::NoError || !indexDoc.isObject()) {
        ERROR_LOG << "Oran7ThemeProfileManager: Failed to parse Index.json";
        return;
    }

    QJsonObject indexObj = indexDoc.object();

    /*! @note 构建默认主题配置->包含 Index.json 的 __style__ 和所有组件的完整配置 */
    QJsonObject defaultTheme;

    if (indexObj.contains("__style__")) {
        defaultTheme["__style__"] = indexObj["__style__"];
    }

    /*! @note 加载所有组件主题 */
    if (indexObj.contains("__component__"))
    {
        QJsonObject components;
        QJsonObject componentRefs = indexObj["__component__"].toObject();

        for (auto it = componentRefs.constBegin(); it != componentRefs.constEnd(); ++it)
        {
            QString componentName = it.key();
            QString componentPath = it.value().toString();

            QFile componentFile(componentPath);
            if (componentFile.exists() && componentFile.open(QIODevice::ReadOnly))
            {
                QJsonDocument componentDoc = QJsonDocument::fromJson(componentFile.readAll());
                componentFile.close();

                if (!componentDoc.isNull() && componentDoc.isObject()) {
                    components[componentName] = componentDoc.object();
                    CONFIG_LOG << "Oran7ThemeProfileManager: Loaded component" << componentName << "from" << componentPath;
                }
                else
                    WARNING_LOG << "Oran7ThemeProfileManager: Failed to loaded component" << componentName << "from" << componentPath;
            } else
                WARNING_LOG << "Oran7ThemeProfileManager: Component file not found" << componentPath;
        }

        if (!components.isEmpty()) {
            defaultTheme["__component__"] = components;
        }
        else
            WARNING_LOG << "Oran7ThemeProfileManager::initializeDefaultTheme Faild :components is empty!";
    }

#if DEVELOPER_MODE
    /*! @note 开发者模式：合并缺失的键，保留现有配置 */
    INFO_LOG << "Developer mode: Merging missing keys into existing profile";
    QJsonObject userConfigTheme;

    if (QFile::exists(defaultProfilePath))
    {
        QFile userConfigFile(defaultProfilePath);
        if (userConfigFile.open(QIODevice::ReadOnly))
        {
            QJsonParseError loadError;
            QJsonDocument existingDoc = QJsonDocument::fromJson(userConfigFile.readAll(), &loadError);
            userConfigFile.close();

            if (loadError.error == QJsonParseError::NoError && existingDoc.isObject()) {
                userConfigTheme = existingDoc.object();
                INFO_LOG << "Developer mode: Loaded existing profile-default.json";
            } else {
                WARNING_LOG << "Developer mode: Failed to parse existing profile, will recreate";
            }
        }
    }

    /*! @note 将模板合并到现有配置中，只添加缺失的键 */
    mergeMissingJsonObject(userConfigTheme, defaultTheme);
    defaultTheme = userConfigTheme;
#endif

    /*! @note 保存为 profile-default.json */
    QJsonDocument doc(defaultTheme);
    QFile userConfigFile(defaultProfilePath);
    if (userConfigFile.open(QIODevice::WriteOnly)) {
        userConfigFile.write(doc.toJson(QJsonDocument::Indented));
        userConfigFile.close();
        CONFIG_LOG << "Oran7ThemeProfileManager: Initialized default theme profile with"
                     << (defaultTheme.contains("__component__") ? defaultTheme["__component__"].toObject().size() : 0)
                     << "components";
    } else {
        ERROR_LOG << "Oran7ThemeProfileManager: Failed to create profile-default.json";
    }
}

QString Oran7ThemeProfileManager::profileFilePath(const QString& profileName) const
{
    QString dir = themeProfilesDir();
    return QDir(dir).filePath(profileName.endsWith(".json") ? profileName : profileName + ".json");
}

QJsonObject Oran7ThemeProfileManager::loadThemeProfile(const QString& profileName)
{
    QString filePath = profileFilePath(profileName);

    if (!QFile::exists(filePath)) {
        // 文件不存在，返回空对象
        return QJsonObject();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Cannot open theme profile" << profileName;
        return QJsonObject();
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull() || !doc.isObject()) {
        ERROR_LOG << "Oran7ThemeProfileManager: Invalid theme profile" << profileName;
        return QJsonObject();
    }

    return doc.object();
}

bool Oran7ThemeProfileManager::saveThemeProfile(const QString& profileName, const QJsonObject& theme)
{
    if (!isValidProfileName(profileName)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Invalid profile name" << profileName;
        return false;
    }

    QString filePath = profileFilePath(profileName);
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Cannot save theme profile" << profileName;
        return false;
    }

    QJsonDocument doc(theme);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    emit themeProfileChanged(profileName);
    //CONFIG_LOG << "Oran7ThemeProfileManager >> Saved theme profileDir:" << filePath;

    return true;
}

QStringList Oran7ThemeProfileManager::getAvailableThemeProfiles() const
{
    QStringList profiles;
    QDir dir(themeProfilesDir());

    QStringList filters;
    filters << "profile-*.json";
    dir.setNameFilters(filters);

    QFileInfoList fileList = dir.entryInfoList(QDir::Files);
    for (const QFileInfo& fileInfo : std::as_const(fileList)) {
        QString fileName = fileInfo.baseName();  // 去掉 .json 后缀
        if (fileName.startsWith("profile-")) {
            profiles.append(fileName);
        }
    }

    // 排序，profile-default 始终在前面
    profiles.sort();
    if (profiles.contains("profile-default")) {
        profiles.removeAll("profile-default");
        profiles.prepend("profile-default");
    }

    return profiles;
}

bool Oran7ThemeProfileManager::deleteThemeProfile(const QString& profileName)
{
    // 不允许删除默认主题
    if (profileName == "profile-default") {
        WARNING_LOG << "Oran7ThemeProfileManager: Cannot delete default profile";
        return false;
    }

    if (!isValidProfileName(profileName)) {
        return false;
    }

    QString filePath = profileFilePath(profileName);

    if (!QFile::exists(filePath)) {
        return false;
    }

    if (QFile::remove(filePath)) {
        emit availableThemeProfilesChanged();
        CONFIG_LOG << "Oran7ThemeProfileManager: Deleted theme profile" << profileName;
        return true;
    }

    ERROR_LOG << "Oran7ThemeProfileManager: Failed to delete theme profile" << profileName;
    return false;
}

bool Oran7ThemeProfileManager::duplicateThemeProfile(const QString& sourceName, const QString& newName)
{
    if (!isValidProfileName(newName)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Invalid new profile name" << newName;
        return false;
    }

    QString sourcePath = profileFilePath(sourceName);
    QString newPath = profileFilePath(newName);

    if (!QFile::exists(sourcePath)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Source profile not found" << sourceName;
        return false;
    }

    if (QFile::exists(newPath)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Target profile already exists" << newName;
        return false;
    }

    if (QFile::copy(sourcePath, newPath)) {
        emit availableThemeProfilesChanged();
        CONFIG_LOG << "Oran7ThemeProfileManager: Duplicated theme profile" << sourceName << "to" << newName;
        return true;
    }

    ERROR_LOG << "Oran7ThemeProfileManager: Failed to duplicate theme profile" << sourceName;
    return false;
}

bool Oran7ThemeProfileManager::exportThemeProfile(const QString& profileName, const QString& filePath)
{
    QJsonObject theme = loadThemeProfile(profileName);
    if (theme.isEmpty()) {
        ERROR_LOG << "Oran7ThemeProfileManager: Theme profile not found" << profileName;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Cannot export to" << filePath;
        return false;
    }

    QJsonDocument doc(theme);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    CONFIG_LOG << "Oran7ThemeProfileManager: Exported theme profile" << profileName << "to" << filePath;
    return true;
}

QString Oran7ThemeProfileManager::importThemeProfile(const QString& filePath, const QString& newName)
{
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        ERROR_LOG << "Oran7ThemeProfileManager: Cannot open import file" << filePath;
        return QString();
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull() || !doc.isObject()) {
        ERROR_LOG << "Oran7ThemeProfileManager: Invalid theme profile file" << filePath;
        return QString();
    }

    // 确定目标名称
    QString targetName = newName.isEmpty()
        ? QFileInfo(filePath).baseName()
        : newName;

    // 确保名称以 profile- 开头
    if (!targetName.startsWith("profile-")) {
        targetName = "profile-" + targetName;
    }

    // 如果文件已存在，添加数字后缀
    QString finalName = targetName;
    int counter = 1;
    while (QFile::exists(profileFilePath(finalName))) {
        finalName = targetName + "-" + QString::number(counter++);
    }

    if (saveThemeProfile(finalName, doc.object())) {
        emit availableThemeProfilesChanged();
        CONFIG_LOG << "Oran7ThemeProfileManager: Imported theme profile" << finalName;
        return finalName;
    }

    return QString();
}

void Oran7ThemeProfileManager::mergeMissingJsonObject(QJsonObject& target,const QJsonObject& source)
{
    for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
        const QString key = it.key();
        const QJsonValue sourceValue = it.value();

        if (!target.contains(key)) {
            // target 缺失：直接补 source 的默认值
            target.insert(key, sourceValue);
            continue;
        }

        QJsonValue targetValue = target.value(key);

        // 两边都是 object：递归补缺失字段
        if (targetValue.isObject() && sourceValue.isObject()) {
            QJsonObject targetObj = targetValue.toObject();
            mergeMissingJsonObject(targetObj, sourceValue.toObject());
            target.insert(key, targetObj);
        }
    }
}

bool Oran7ThemeProfileManager::isValidProfileName(const QString& profileName) const
{
    // 名称必须以 profile- 开头
    // if (!profileName.startsWith("profile-")) {
    //     return false;
    // }

    // 不允许包含特殊字符
    QRegularExpression regex("^profile-[a-zA-Z0-9_-]+$");
    return regex.match(profileName).hasMatch();
}
