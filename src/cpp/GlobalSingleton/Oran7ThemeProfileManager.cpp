#include "Oran7ThemeProfileManager.h"
#include "Oran7GlobalHelper.h"
#include "Oran7Theme.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <QRegularExpression>

Oran7ThemeProfileManager& Oran7ThemeProfileManager::instance()
{
    static Oran7ThemeProfileManager instance;
    return instance;
}

Oran7ThemeProfileManager::Oran7ThemeProfileManager(QObject* parent) : QObject(parent) {}

Oran7ThemeProfileManager::Oran7ThemeProfileManager(const QString& directory, QObject* parent)
    : QObject(parent), m_directory(directory)
{
}

QString Oran7ThemeProfileManager::themeProfilesDir() const
{
    QString configDir = m_directory.isEmpty() ? Oran7GlobalHelper::getConfigDir() : m_directory;
    QDir dir(configDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
    return configDir;
}

void Oran7ThemeProfileManager::ensureThemeProfilesDir()
{
    QDir dir(themeProfilesDir());
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
}

namespace
{
QJsonObject sourceObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QJsonObject difference(const QJsonObject& value, const QJsonObject& defaults)
{
    QJsonObject result;
    for (auto it = value.begin(); it != value.end(); ++it)
    {
        if (it->isObject() && defaults[it.key()].isObject())
        {
            const auto child = difference(it->toObject(), defaults[it.key()].toObject());
            if (!child.isEmpty())
                result[it.key()] = child;
        }
        else if (it.value() != defaults[it.key()])
            result[it.key()] = it.value();
    }
    return result;
}
QJsonObject sourceDefaults()
{
    auto defaults = sourceObject(":/config/themeJson/Index.json");
    const auto refs = defaults.value("__component__").toObject();
    QJsonObject components;
    for (auto it = refs.begin(); it != refs.end(); ++it)
        components[it.key()] = sourceObject(it->toString());
    defaults["__component__"] = components;
    return defaults;
}
QJsonObject normalizeProfile(QJsonObject value)
{
    // 旧全量 profile 只移除与当前默认值完全相等的项，未知扩展保留。
    const auto legacyDark =
        value.value("__component__").toObject()["Oran7MainGUI"].toObject()["__style__"].toObject().value("isDarkMode");
    if (!value.contains("profileVersion"))
        value = difference(value, sourceDefaults());
    if (!value.contains("darkMode") && legacyDark.isBool())
        value["darkMode"] = legacyDark.toBool() ? 1 : 0;
    auto components = value.value("__component__").toObject();
    auto gui = components.value("Oran7MainGUI").toObject();
    auto style = gui.value("__style__").toObject();
    if (!value.contains("darkMode") && style.contains("isDarkMode"))
        value["darkMode"] = style.value("isDarkMode").toBool() ? 1 : 0;
    style.remove("isDarkMode");
    style.remove("OpenSettingWin");
    style.remove("settingContent_visiable");
    gui["__style__"] = style;
    components["Oran7MainGUI"] = gui;
    value["__component__"] = components;
    // 新格式同样裁掉与默认相同的覆盖，空对象不落盘。
    auto result = difference(value, sourceDefaults());
    result["profileVersion"] = 2;
    return result;
}
bool supportedProfile(const QJsonObject& value)
{
    if (!value.contains("profileVersion"))
        return true;
    return value.value("profileVersion").isDouble() && value.value("profileVersion").toDouble() == 2;
}
bool readProfile(const QString& path, QJsonObject* value)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject() || !supportedProfile(doc.object()))
        return false;
    *value = doc.object();
    return true;
}
} // namespace

void Oran7ThemeProfileManager::initializeDefaultTheme(Oran7Theme*)
{
    if (!QFile::exists(profileFilePath("profile-default")))
        saveThemeProfile("profile-default", {});
}

QString Oran7ThemeProfileManager::profileFilePath(const QString& profileName) const
{
    QString dir = themeProfilesDir();
    return QDir(dir).filePath(profileName.endsWith(".json") ? profileName : profileName + ".json");
}

QJsonObject Oran7ThemeProfileManager::loadThemeProfile(const QString& name)
{
    if (!isValidProfileName(name))
        return {};
    const auto path = profileFilePath(name);
    if (!QFile::exists(path))
        return {};
    QJsonObject value;
    if (!readProfile(path, &value))
    {
        qWarning() << "主题 profile 无效或版本不支持，保留原件:" << name;
        return {};
    }
    const auto result = normalizeProfile(value);
    if (value != result && !saveThemeProfile(name, result))
        qWarning() << "主题 profile 迁移未落盘，保留原件:" << name;
    return result;
}

bool Oran7ThemeProfileManager::isThemeProfileReadable(const QString& name) const
{
    QJsonObject value;
    return isValidProfileName(name) && readProfile(profileFilePath(name), &value);
}

bool Oran7ThemeProfileManager::saveThemeProfile(const QString& name, const QJsonObject& theme)
{
    if (!isValidProfileName(name) || !supportedProfile(theme))
        return false;
    const auto path = profileFilePath(name);
    const auto normalized = normalizeProfile(theme);
    if (QFile::exists(path))
    {
        QJsonObject original;
        if (!readProfile(path, &original))
            return false;
        if (original == normalized)
            return true;
        if (!original.contains("profileVersion"))
        {
            const auto backup = path + ".bak-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (!QFile::copy(path, backup))
                return false;
        }
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const auto bytes = QJsonDocument(normalized).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit())
        return false;
    emit themeProfileChanged(name);
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
    for (const QFileInfo& fileInfo : std::as_const(fileList))
    {
        QString fileName = fileInfo.baseName(); // 去掉 .json 后缀
        if (fileName.startsWith("profile-"))
        {
            profiles.append(fileName);
        }
    }

    // 排序，profile-default 始终在前面
    profiles.sort();
    if (profiles.contains("profile-default"))
    {
        profiles.removeAll("profile-default");
        profiles.prepend("profile-default");
    }

    return profiles;
}

bool Oran7ThemeProfileManager::deleteThemeProfile(const QString& profileName)
{
    // 不允许删除默认主题
    if (profileName == "profile-default")
    {
        WARNING_LOG << "Oran7ThemeProfileManager: Cannot delete default profile";
        return false;
    }

    if (!isValidProfileName(profileName))
    {
        return false;
    }

    QString filePath = profileFilePath(profileName);

    if (!QFile::exists(filePath))
    {
        return false;
    }

    if (QFile::remove(filePath))
    {
        emit availableThemeProfilesChanged();
        CONFIG_LOG << "Oran7ThemeProfileManager: Deleted theme profile" << profileName;
        return true;
    }

    ERROR_LOG << "Oran7ThemeProfileManager: Failed to delete theme profile" << profileName;
    return false;
}

bool Oran7ThemeProfileManager::duplicateThemeProfile(const QString& sourceName, const QString& newName)
{
    if (!isValidProfileName(newName))
    {
        ERROR_LOG << "Oran7ThemeProfileManager: Invalid new profile name" << newName;
        return false;
    }

    if (!isValidProfileName(sourceName))
        return false;
    QString sourcePath = profileFilePath(sourceName);
    QString newPath = profileFilePath(newName);

    if (!QFile::exists(sourcePath))
    {
        ERROR_LOG << "Oran7ThemeProfileManager: Source profile not found" << sourceName;
        return false;
    }

    if (QFile::exists(newPath))
    {
        ERROR_LOG << "Oran7ThemeProfileManager: Target profile already exists" << newName;
        return false;
    }

    QJsonObject source;
    if (!readProfile(sourcePath, &source))
        return false;
    if (saveThemeProfile(newName, source))
    {
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
    if (!isValidProfileName(profileName) || !QFile::exists(profileFilePath(profileName)) || theme.isEmpty())
    {
        ERROR_LOG << "Oran7ThemeProfileManager: Theme profile not found" << profileName;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
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
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        ERROR_LOG << "Oran7ThemeProfileManager: Cannot open import file" << filePath;
        return QString();
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull() || !doc.isObject() || !supportedProfile(doc.object()))
    {
        ERROR_LOG << "Oran7ThemeProfileManager: Invalid theme profile file" << filePath;
        return QString();
    }

    // 确定目标名称
    QString targetName = newName.isEmpty() ? QFileInfo(filePath).baseName() : newName;

    // 确保名称以 profile- 开头
    if (!targetName.startsWith("profile-"))
    {
        targetName = "profile-" + targetName;
    }

    // 如果文件已存在，添加数字后缀
    if (!isValidProfileName(targetName))
        return {};
    QString finalName = targetName;
    int counter = 1;
    while (QFile::exists(profileFilePath(finalName)))
    {
        finalName = targetName + "-" + QString::number(counter++);
    }

    if (saveThemeProfile(finalName, doc.object()))
    {
        emit availableThemeProfilesChanged();
        CONFIG_LOG << "Oran7ThemeProfileManager: Imported theme profile" << finalName;
        return finalName;
    }

    return QString();
}

bool Oran7ThemeProfileManager::isValidProfileName(const QString& profileName) const
{
    // 不允许包含特殊字符
    QRegularExpression regex("^profile-[a-zA-Z0-9_-]+$");
    return regex.match(profileName).hasMatch();
}
