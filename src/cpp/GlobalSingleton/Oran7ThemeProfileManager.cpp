#include "Oran7ThemeProfileManager.h"
#include "Oran7GlobalHelper.h"
#include "Oran7ThemeProfile.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace
{
struct Oran7ProfileFile
{
    QByteArray bytes;
    QJsonObject document;
    QJsonObject normalized;
};

bool entryExists(const QString& path)
{
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

std::optional<Oran7ProfileFile> readProfile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    auto bytes = file.readAll();
    const auto json = QJsonDocument::fromJson(bytes);
    if (file.error() != QFileDevice::NoError || !json.isObject())
        return std::nullopt;
    auto normalized = Oran7ThemeProfile::normalize(json.object());
    if (!normalized)
        return std::nullopt;
    return Oran7ProfileFile{std::move(bytes), json.object(), std::move(*normalized)};
}

bool writeAtomically(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

enum class Oran7ProfileWrite { Failed, Unchanged, Committed };

Oran7ProfileWrite commitProfile(const QString& path, const QJsonObject& normalized, bool createOnly = false,
                               const QByteArray* expectedOldBytes = nullptr)
{
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(0))
        return Oran7ProfileWrite::Failed;
    if (entryExists(path))
    {
        // 符号链接和损坏、未来版本原件均禁止覆盖。
        if (createOnly || QFileInfo(path).isSymLink())
            return Oran7ProfileWrite::Failed;
        const auto old = readProfile(path);
        if (!old || (expectedOldBytes && old->bytes != *expectedOldBytes))
            return Oran7ProfileWrite::Failed;
        if (old->document == normalized)
            return Oran7ProfileWrite::Unchanged;
        if (!Oran7ThemeProfile::isV3(old->document))
        {
            const auto digest = QCryptographicHash::hash(old->bytes, QCryptographicHash::Sha256).toHex();
            const auto backup = path + ".bak-" + QString::fromLatin1(digest);
            if (entryExists(backup))
            {
                QFile file(backup);
                if (!QFileInfo(backup).isFile() || QFileInfo(backup).isSymLink() ||
                    !file.open(QIODevice::ReadOnly) || file.readAll() != old->bytes ||
                    file.error() != QFileDevice::NoError)
                    return Oran7ProfileWrite::Failed;
            }
            else if (!writeAtomically(backup, old->bytes))
                return Oran7ProfileWrite::Failed;
        }
    }
    else if (expectedOldBytes)
        return Oran7ProfileWrite::Failed;
    return writeAtomically(path, QJsonDocument(normalized).toJson())
        ? Oran7ProfileWrite::Committed : Oran7ProfileWrite::Failed;
}
} // namespace

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
    const auto path = m_directory.isEmpty() ? Oran7GlobalHelper::getConfigDir() : m_directory;
    if (!QDir(path).exists())
        QDir().mkpath(path);
    return path;
}

void Oran7ThemeProfileManager::initializeDefaultTheme()
{
    const auto path = profileFilePath("profile-default");
    if (!entryExists(path))
        commitProfile(path, QJsonObject{{"__profileVersion__", 3}}, true);
}

QString Oran7ThemeProfileManager::profileFilePath(const QString& name) const
{
    return QDir(themeProfilesDir()).filePath(name + ".json");
}

QJsonObject Oran7ThemeProfileManager::loadThemeProfile(const QString& name)
{
    if (!isValidProfileName(name))
        return {};
    const auto path = profileFilePath(name);
    if (!entryExists(path))
        return {};
    const auto source = readProfile(path);
    if (!source)
    {
        qWarning() << "主题 profile 无效或版本不支持，保留原件:" << name;
        return {};
    }
    if (!Oran7ThemeProfile::isV3(source->document))
    {
        const auto result = commitProfile(path, source->normalized, false, &source->bytes);
        if (result == Oran7ProfileWrite::Failed)
            qWarning() << "主题 profile 迁移未落盘，保留原件:" << name;
        else if (result == Oran7ProfileWrite::Committed)
            emit themeProfileChanged(name);
    }
    return source->normalized;
}

bool Oran7ThemeProfileManager::isThemeProfileReadable(const QString& name) const
{
    return isValidProfileName(name) && bool(readProfile(profileFilePath(name)));
}

bool Oran7ThemeProfileManager::saveThemeProfile(const QString& name, const QJsonObject& theme,
                                               const QJsonObject* defaults, QJsonObject* saved)
{
    if (!isValidProfileName(name))
        return false;
    const auto* baseline = defaults ? defaults : &Oran7ThemeProfile::legacyProfileDefaults();
    const auto normalized = Oran7ThemeProfile::normalize(theme, baseline);
    if (!normalized)
        return false;
    const auto result = commitProfile(profileFilePath(name), *normalized);
    if (result == Oran7ProfileWrite::Failed)
        return false;
    if (saved)
        *saved = *normalized;
    if (result == Oran7ProfileWrite::Committed)
        emit themeProfileChanged(name);
    return true;
}

QStringList Oran7ThemeProfileManager::getAvailableThemeProfiles() const
{
    QStringList profiles;
    const auto files = QDir(themeProfilesDir()).entryInfoList({"profile-*.json"}, QDir::Files, QDir::Name);
    for (const auto& file : files)
        if (isValidProfileName(file.completeBaseName()))
            profiles.append(file.completeBaseName());
    if (profiles.removeAll("profile-default"))
        profiles.prepend("profile-default");
    return profiles;
}

bool Oran7ThemeProfileManager::deleteThemeProfile(const QString& name)
{
    if (!isValidProfileName(name) || name == "profile-default")
        return false;
    const auto path = profileFilePath(name);
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(0) || !QFile::remove(path))
        return false;
    lock.unlock();
    emit availableThemeProfilesChanged();
    return true;
}

bool Oran7ThemeProfileManager::duplicateThemeProfile(const QString& sourceName, const QString& newName)
{
    if (!isValidProfileName(sourceName) || !isValidProfileName(newName))
        return false;
    const auto source = readProfile(profileFilePath(sourceName));
    if (!source || commitProfile(profileFilePath(newName), source->normalized, true) != Oran7ProfileWrite::Committed)
        return false;
    emit themeProfileChanged(newName);
    emit availableThemeProfilesChanged();
    return true;
}

bool Oran7ThemeProfileManager::exportThemeProfile(const QString& name, const QString& filePath)
{
    if (!isValidProfileName(name) || filePath.isEmpty())
        return false;
    const auto source = readProfile(profileFilePath(name));
    const QFileInfo target(filePath);
    const QFileInfo storage(themeProfilesDir());
    // 导出路径不得绕过管理目录内的覆盖、备份和命名规则。
    const auto targetDir = target.dir().canonicalPath();
    if (!source || target.isSymLink() ||
        (target.fileName().startsWith("profile-", Qt::CaseInsensitive) && !targetDir.isEmpty() &&
         targetDir.compare(storage.canonicalFilePath(), Qt::CaseInsensitive) == 0))
        return false;
    return writeAtomically(filePath, QJsonDocument(source->normalized).toJson());
}

QString Oran7ThemeProfileManager::importThemeProfile(const QString& filePath, const QString& newName)
{
    const auto source = readProfile(filePath);
    auto name = newName.isEmpty() ? QFileInfo(filePath).completeBaseName() : newName;
    if (!name.startsWith("profile-"))
        name.prepend("profile-");
    if (!source || !isValidProfileName(name))
        return {};
    auto candidate = name;
    for (int suffix = 1; entryExists(profileFilePath(candidate)); ++suffix)
        candidate = name + '-' + QString::number(suffix);
    if (commitProfile(profileFilePath(candidate), source->normalized, true) != Oran7ProfileWrite::Committed)
        return {};
    emit themeProfileChanged(candidate);
    emit availableThemeProfilesChanged();
    return candidate;
}

bool Oran7ThemeProfileManager::isValidProfileName(const QString& name) const
{
    static const QRegularExpression pattern(QStringLiteral("\\Aprofile-[a-zA-Z0-9_-]+\\z"));
    return pattern.match(name).hasMatch();
}
