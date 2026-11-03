#include "Oran7AppConfigManager.h"
#include "Oran7GlobalHelper.h"

#include <utility>

#include <QJsonDocument>
#include <QStandardPaths>
#include <QDebug>
#include <QSaveFile>
#include <QLockFile>
#include <QDateTime>
#include <QUuid>
#include "../Oran7Config/Oran7ConfigKeys.h"
using namespace Oran7ConfigKeys;

Oran7AppConfigManager& Oran7AppConfigManager::ins()
{
    static Oran7AppConfigManager instance;
    return instance;
}

namespace {
QJsonObject readDefaults() {
    QFile resource(":/config/default.json");
    if (!resource.open(QIODevice::ReadOnly)) {
        resource.setFileName(Oran7GlobalHelper::getConfigDir() + "/default.json");
        if (!resource.open(QIODevice::ReadOnly)) return {};
    }
    return QJsonDocument::fromJson(resource.readAll()).object();
}
}

Oran7AppConfigManager::Oran7AppConfigManager(QObject* parent)
    : Oran7AppConfigManager(Oran7GlobalHelper::getConfigDir() + "/config.json", readDefaults(), parent)
{
    // 主题插件可能早于 main 的显式加载读取配置；首次访问即完成一次加载。
    loadConfig();
}

Oran7AppConfigManager::Oran7AppConfigManager(const QString& filePath, const QJsonObject& defaults, QObject* parent)
    : QObject(parent), m_filePath(filePath), m_credentials(filePath + QStringLiteral(".credentials"))
{
    if (defaults.isEmpty() || !Oran7AppConfig::normalize(defaults, defaults, &m_defaultConfig, &m_lastError)) {
        m_writable = false;
        m_lastError = QStringLiteral("默认配置资源无效，禁止写入用户配置");
    }
    m_config = m_defaultConfig;
    const auto initial = snapshot();
    if (initial.window.width <= 0 || initial.window.height <= 0 || initial.capture.fps < 1
        || initial.capture.fps > 240 || initial.recentFilesMaxCount < 1 || initial.recentFilesMaxCount > 50
        || initial.appearance.themeProfile.isEmpty()) {
        m_writable = false;
        m_lastError = QStringLiteral("默认配置缺少必要字段，禁止写入用户配置");
    }
    loadConfig();
}

QString Oran7AppConfigManager::configFilePath() { return m_filePath; }

bool Oran7AppConfigManager::loadConfig()
{
    // 同一存储只加载一次，避免丢失尚未落盘的音量/窗口/列表修改。
    if (m_loaded) return m_loadSucceeded;
    m_loaded = true;
    if (!m_writable) return false;
    QFile file(configFilePath());
    if (!file.exists()) {
        m_modified = true;
        m_loadSucceeded = saveConfig();
        return m_loadSucceeded;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_writable = false;
        m_lastError = QStringLiteral("无法读取配置文件，禁止覆盖原件");
        return false;
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close(); // Windows 下先释放原件读句柄，启动迁移才能原子替换该文件。
    m_backupNeeded = true;
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("配置文件损坏，使用默认值；保存前保留原件备份");
        m_modified = true;
        WARNING_LOG << m_lastError;
        return false;
    }
    QJsonObject normalized;
    if (!Oran7AppConfig::normalize(doc.object(), m_defaultConfig, &normalized, &m_lastError)) {
        m_writable = false;
        WARNING_LOG << m_lastError;
        return false;
    }
    m_config = normalized;
    if (!restoreCredentials(&m_config)) return false;
    m_modified = Oran7CredentialStore::publicConfig(m_config, m_credentialRefs) != doc.object();
    m_loadSucceeded = true;
    // 旧版本明文凭据立即迁移；先验证密文，再原子替换普通配置，失败保留原件。
    bool migration = false;
    for (const auto& group : Oran7CredentialStore::groups())
        migration |= !Oran7CredentialStore::extract(doc.object(), group).isEmpty();
    if (migration) {
        m_modified = true;
        const bool saved = saveConfig();
        if (!saved) WARNING_LOG << "[Credentials] 迁移未完成，原配置已保留：" << m_lastError;
        return saved;
    }
    return true;
}

bool Oran7AppConfigManager::restoreCredentials(QJsonObject* config)
{
    const auto failClosed = [&]
    {
        for (const auto& group : Oran7CredentialStore::groups()) Oran7CredentialStore::remove(config, group);
        QJsonObject safe;
        if (Oran7AppConfig::normalize(*config, m_defaultConfig, &safe)) *config = safe;
        m_writable = false;
        return false;
    };
    const auto marker = config->value("credentialStorage");
    if (!marker.isUndefined())
    {
        const auto object = marker.toObject();
        if (!marker.isObject() || object.value("version") != QJsonValue(1) || !object.value("groups").isObject())
        {
            m_lastError = QStringLiteral("凭据存储版本无效或来自较新版本，禁止覆盖原配置");
            return failClosed();
        }
        m_credentialRefs = object.value("groups").toObject();
        for (auto it = m_credentialRefs.begin(); it != m_credentialRefs.end(); ++it)
        {
            if (!Oran7CredentialStore::groups().contains(it.key()) || !it.value().isString() ||
                !Oran7CredentialStore::validRevision(it.value().toString()))
            {
                m_lastError = QStringLiteral("凭据引用无效，禁止覆盖原配置");
                return failClosed();
            }
        }
    }
    for (const auto& group : Oran7CredentialStore::groups())
    {
        if (m_credentialRefs.contains(group))
        {
            QJsonObject secret;
            QString error;
            // 有引用就绝不回退到遗留明文，解密失败也不能复活旧登录态。
            Oran7CredentialStore::remove(config, group);
            if (m_credentials.read(group, m_credentialRefs.value(group).toString(), &secret, &error))
                Oran7CredentialStore::apply(config, group, secret);
            else
            {
                m_credentialErrors[group] = group + QStringLiteral(": ") + error;
                WARNING_LOG << "[Credentials] 无法恢复凭据组=" << group;
            }
        }
    }
    QJsonObject normalized;
    if (!Oran7AppConfig::normalize(*config, m_defaultConfig, &normalized, &m_lastError)) return false;
    *config = normalized;
    for (const auto& group : Oran7CredentialStore::groups())
        m_credentialBaseline[group] = Oran7CredentialStore::extract(*config, group);
    return true;
}

bool Oran7AppConfigManager::clearCredentialGroup(const QString& group)
{
    if (!m_writable || !Oran7CredentialStore::groups().contains(group)) return false;
    Oran7CredentialStore::remove(&m_config, group);
    QJsonObject normalized;
    if (!Oran7AppConfig::normalize(m_config, m_defaultConfig, &normalized, &m_lastError)) return false;
    m_config = normalized;
    m_forceCredentialWrites.insert(group); // 解密失败时也提交空密文代际，明确完成退出登录。
    m_modified = true;
    return saveConfig();
}

bool Oran7AppConfigManager::saveConfig()
{
    // 公共独立实例也必须先读取原件，防止未加载就保存默认值。
    if (!m_loaded) loadConfig();
    if (!m_writable) return false;
    if (!m_modified && m_forceCredentialWrites.isEmpty()) return true;
    if (!QDir().mkpath(QFileInfo(m_filePath).absolutePath())) {
        m_lastError = QStringLiteral("无法创建配置目录"); return false;
    }
    QLockFile transaction(m_filePath + QStringLiteral(".write.lock"));
    if (!transaction.tryLock(0)) {
        m_lastError = QStringLiteral("另一实例正在保存配置，请稍后重试"); return false;
    }
    // 防止另一实例已退出登录后，旧快照因普通设置保存而复活旧凭据引用。
    QFile current(m_filePath);
    if (current.exists() && current.open(QIODevice::ReadOnly)) {
        const auto disk = QJsonDocument::fromJson(current.readAll()).object();
        current.close();
        const auto marker = disk.value("credentialStorage").toObject();
        if (marker.value("groups").toObject() != m_credentialRefs ||
            (!marker.isEmpty() && marker.value("version") != QJsonValue(1))) {
            m_lastError = QStringLiteral("凭据已由另一实例修改，请重新启动后再保存"); return false;
        }
    }
    QJsonObject nextRefs = m_credentialRefs;
    QSet<QString> changedGroups;
    for (const auto& group : Oran7CredentialStore::groups())
    {
        const auto data = Oran7CredentialStore::extract(m_config, group);
        if (data != m_credentialBaseline.value(group).toObject() || m_forceCredentialWrites.contains(group) ||
            (!nextRefs.contains(group) && !data.isEmpty()))
        {
            QString revision;
            if (!m_credentials.writeVerified(group, data, &revision, &m_lastError)) return false;
            nextRefs[group] = revision;
            changedGroups.insert(group);
        }
    }
    if (m_backupNeeded) {
        const auto backup = m_filePath + ".bak-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")
            + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        if (!QFile::copy(m_filePath, backup)) {
            m_lastError = QStringLiteral("无法备份原配置，取消写入"); return false;
        }
        m_backupNeeded = false;
    }
    QSaveFile file(m_filePath);
    file.setDirectWriteFallback(false);
    const auto publicConfig = Oran7CredentialStore::publicConfig(m_config, nextRefs);
    const auto bytes = QJsonDocument(publicConfig).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_lastError = QStringLiteral("配置原子保存失败"); return false;
    }
    m_modified = false;
    m_credentialRefs = nextRefs;
    if (!nextRefs.isEmpty()) m_config["credentialStorage"] = publicConfig.value("credentialStorage");
    for (const auto& group : Oran7CredentialStore::groups())
        m_credentialBaseline[group] = Oran7CredentialStore::extract(m_config, group);
    for (const auto& group : changedGroups) m_credentialErrors.remove(group);
    m_forceCredentialWrites.clear();
    m_loadSucceeded = true;
    m_lastError.clear();
    return true;
}

void Oran7AppConfigManager::updateConfig(const Oran7AppConfig& config)
{
    if (!m_loaded) loadConfig();
    if (!m_writable) return;
    QJsonObject normalized;
    if (!Oran7AppConfig::normalize(config.toJson(), m_defaultConfig, &normalized, &m_lastError)) return;
    if (normalized != m_config) { m_config = normalized; m_modified = true; }
}

QVariant Oran7AppConfigManager::getValueQVariant(const QString& key) const
{
    return getNestedValue(key.split('.'), m_config);
}

QVariantList Oran7AppConfigManager::getValueQVariantList(const QString& key) const
{
    return getValueQVariant(key).toList();
}

/**
 * @brief Oran7AppConfigManager::setValue  往JSON文件中对应的key设置value
 * @param key    传入的QString key列表格式"key1.key2.key3",对应一层二层三层,最终目标键为key3
 *                          在内部已经通过QStringList keys = key.split('.');将QString通过split('.')存储为QStringList
 * @param value  需要绑定的最深层的值
 */
void Oran7AppConfigManager::setValueQVariant(const QString& key, const QVariant& value)
{
    if (!m_loaded) loadConfig();
    if (!m_writable) return;
    QStringList keys = key.split('.');
    QJsonObject newConfig = m_config;

    setNestedValue(keys, value, newConfig);//递归存储调用

    /**
     *  ------>  ：关于QJsonObject operator==() 和QJsonObject operator!=() 判断标准--> 两个 QJsonObject 必须有相同数量的键值对才能相等
     *                    键名必须完全相同（包括大小写），QJsonObject内部的键值对的存储是无序的，键的顺序不影响相等性判断
     */
    QJsonObject normalized;
    if (!Oran7AppConfig::normalize(newConfig, m_defaultConfig, &normalized, &m_lastError)) return;
    newConfig = normalized;
    if (newConfig != m_config)
    {
        m_config = newConfig;
        m_modified = true;
        // emit configChanged(key, value);
    }
}

void Oran7AppConfigManager::setValueQVariantList(const QString &key, const QVariantList &valueList)
{
    if (!m_loaded) loadConfig();
    if (!m_writable) return;
    QStringList keys = key.split(".");
    QJsonObject newConfig = m_config;

    setNestedValueList(keys,valueList,newConfig);

    QJsonObject normalized;
    if (!Oran7AppConfig::normalize(newConfig, m_defaultConfig, &normalized, &m_lastError)) return;
    newConfig = normalized;
    if(newConfig != m_config)
    {
        m_config = newConfig;
        m_modified = true;
    }
}

QVariant Oran7AppConfigManager::getNestedValue(const QStringList& keys, const QJsonObject& obj) const
{
    if (keys.isEmpty()) return QVariant();

    QString currentKey = keys.first();
    if (!obj.contains(currentKey))
    {
        return QVariant();
    }

    QJsonValue value = obj.value(currentKey);

    if (keys.size() == 1) {
        // 最后一级键
        if (value.isBool()) return value.toBool();
        if (value.isDouble()) return value.toDouble();
        if (value.isString()) return value.toString();
        if (value.isArray()) return value.toArray().toVariantList();
        if (value.isObject()) return value.toObject().toVariantMap();
        return QVariant();
    }

    if (value.isObject())
    {
        return getNestedValue(keys.mid(1), value.toObject());
    }

    return QVariant();
}

/**
 * @brief Oran7AppConfigManager::setNestedValue      递归函数 递归搜索赋值Key对应的Value并修改赋值
 * @param keys
 * @param value
 * @param obj
 */
void Oran7AppConfigManager::setNestedValue(const QStringList& keys, const QVariant& value, QJsonObject& obj)
{
    if (keys.isEmpty()) return;

    QString currentKey = keys.first();

    if (keys.size() == 1) {
        // 最后一级键：直接设置值
        obj[currentKey] = QJsonValue::fromVariant(value);
        return;// 递归结束
    }

    if (!obj.contains(currentKey) || !obj[currentKey].isObject())
    {
        //如果obj中不包含currentKey 或 obj中currentKey对应的值不是一个QJsonObject 则新创建
        obj[currentKey] = QJsonObject();
    }

    /**
     *      QJsonObject语法  QJsonObject operator[]返回一个QJsonValueRef代理对象   QJsonValueRef.toObject()方法将其转换为QJsonObject
     *      这里类似于一个键值对通过将currentKey：对应的QJsonObject返回  ，即通过QJsonObject operator[]获取currentKey对应的QJsonObject值
     */
    // 进入下一层递归
    QJsonObject nestedObj = obj[currentKey].toObject();//nested是当前层对应的QJsonObject值，往下一层递归使用

    setNestedValue(keys.mid(1), value, nestedObj);//提取从索引1开始到列表末尾的所有元素，往深层递归

    obj[currentKey] = nestedObj;//拷贝取回
}

void Oran7AppConfigManager::setNestedValueList(const QStringList &keys,
                                          const QVariantList &valueList,
                                          QJsonObject &obj)
{
    if(keys.isEmpty()) {
        return;
    }

    QString currentKey = keys.first();

    if(keys.size() == 1) {
        QJsonArray jsonArray;
        for(const QVariant& tx : std::as_const(valueList)) {
            jsonArray.append(QJsonValue::fromVariant(tx));
        }
        obj[currentKey] = jsonArray;
        return;
    }

    // 确保当前键存在且是对象
    if(!obj.contains(currentKey) || !obj[currentKey].isObject()) {
        obj[currentKey] = QJsonObject();
    }

    QJsonObject nestedObj = obj[currentKey].toObject();// 获取下一层对象

    setNestedValueList(keys.mid(1), valueList, nestedObj);// 递归处理剩余键

    obj[currentKey] = nestedObj;// 将修改后的对象赋值回去
}

bool Oran7AppConfigManager::validateConfig()
{
    QJsonObject normalized;
    if (!Oran7AppConfig::normalize(m_config, m_defaultConfig, &normalized, &m_lastError)) return false;
    if (normalized != m_config) { m_config = normalized; m_modified = true; }
    return true;
}

void Oran7AppConfigManager::resetToDefaults()
{
    if (!m_loaded) loadConfig();
    if (!m_writable) return;
    m_config = m_defaultConfig;
    m_modified = true;
    for (const auto& group : Oran7CredentialStore::groups())
        if (m_credentialRefs.contains(group)) m_forceCredentialWrites.insert(group);
    saveConfig();
    // emit recentFilesChanged();
}

// ==================== 最近文件管理功能实现 ====================

void Oran7AppConfigManager::addRecentFile(const QString& filePath)
{
    if (!m_writable || filePath.isEmpty()) return;

    // 获取当前最近文件列表
    QStringList recentFiles = getRecentFiles();
    int maxCount = getMaxRecentFiles();

    // 移除已存在的相同文件路径
    recentFiles.removeAll(filePath);

    // 将新文件添加到列表开头
    recentFiles.prepend(filePath);

    // 限制列表长度
    while (recentFiles.size() > maxCount) {
        recentFiles.removeLast();
    }

    // 更新配置
    updateRecentFilesArray(recentFiles);

    CONFIG_LOG << "Oran7AppConfigManager::addRecentFile:Added recent file:" << filePath << "Total:" << recentFiles.size();
}

void Oran7AppConfigManager::removeRecentFile(const QString& filePath)
{
    QStringList recentFiles = getRecentFiles();

    if (recentFiles.removeAll(filePath) > 0) {
        updateRecentFilesArray(recentFiles);
        CONFIG_LOG << "Removed recent file:" << filePath;
    }
}

void Oran7AppConfigManager::clearRecentFiles()
{
    updateRecentFilesArray(QStringList());
    CONFIG_LOG << "Cleared all recent files";
}

QStringList Oran7AppConfigManager::getRecentFiles() const
{
    QStringList recentFiles;

    if (m_config.contains(RecentFiles) && m_config[RecentFiles].isArray())
    {
        QJsonArray array = m_config[RecentFiles].toArray();
        for (const auto& value : std::as_const(array))
        {
            if (value.isString())
            {
                recentFiles.append(value.toString());
            }
        }
    }

    return recentFiles;
}

int Oran7AppConfigManager::getMaxRecentFiles() const { return snapshot().recentFilesMaxCount; }

void Oran7AppConfigManager::setMaxRecentFiles(int maxCount)
{
    if (!m_writable) return;
    if (maxCount < 1) maxCount = 1;
    if (maxCount > 50) maxCount = 50; // 设置合理上限

    setValueQVariant(RecentMax, maxCount);

    // 如果当前文件数量超过新限制，需要截断
    QStringList recentFiles = getRecentFiles();
    if (recentFiles.size() > maxCount) {
        recentFiles = recentFiles.mid(0, maxCount);
        updateRecentFilesArray(recentFiles);
    }
}

void Oran7AppConfigManager::updateRecentFilesArray(const QStringList& files)
{
    if (!m_writable) return;
    QJsonArray jsonArray;
    for (const QString& filePath : files) {
        jsonArray.append(filePath);
    }

    m_config[RecentFiles] = jsonArray;
    m_modified = true;

    // emit recentFilesChanged();
    // emit configChanged(RecentFiles, files);

    // 自动保存配置
    saveConfig();
}

// ==================== 最近使用文件管理功能实现 ====================

QString Oran7AppConfigManager::getLastUsedFile() const
{
    if (m_config.contains(LastPath) && m_config[LastPath].isString())
    {
        QString lastFile = m_config[LastPath].toString();
        if (lastFile.isEmpty())
            return "";
        // 网络 URL（网易云播放链接）无本地文件，跳过存在性检查
        if (lastFile.startsWith(QLatin1String("http://"))
            || lastFile.startsWith(QLatin1String("https://")))
            return lastFile;
        // 检查本地文件是否仍然存在
        if (QFile::exists(lastFile))
            return lastFile;
    }
    return "";
}

bool Oran7AppConfigManager::getLastUsedFileIsVip() const
{
    return m_config.value(LastSourceVip).toBool(false);
}

void Oran7AppConfigManager::setLastUsedFile(const QString& filePath, bool isVip)
{
    if (!m_writable) return;
    if (filePath.isEmpty())
    {
        clearLastUsedFile();
        return;
    }

    // 网络 URL（网易云播放链接）无本地文件，跳过存在性检查；本地文件才校验存在
    const bool isUrl = filePath.startsWith(QLatin1String("http://"))
                    || filePath.startsWith(QLatin1String("https://"));
    if (!isUrl && !QFile::exists(filePath))
    {
        WARNING_LOG << "Oran7AppConfigManager::setLastUsedFile:File does not exist:" << filePath;
        return;
    }

    QString currentLastFile = getLastUsedFile();
    const bool vipChanged = m_config.value(LastSourceVip).toBool(false) != isVip;
    if (currentLastFile != filePath || vipChanged)
    {
        m_config[LastPath] = filePath;
        m_config[LastSourceVip] = isVip;
        m_modified = true;

        // 自动保存配置
        saveConfig();

        // emit lastUsedFileChanged();
        // emit configChanged(LastPath, filePath);

        CONFIG_LOG << "Oran7AppConfigManager::setLastUsedFile:Last used file set to:" << filePath;
    }
}

void Oran7AppConfigManager::clearLastUsedFile()
{
    if (!m_writable) return;
    if (m_config.contains(LastPath) && !m_config[LastPath].toString().isEmpty())
    {
        m_config[LastPath] = "";
        m_config[LastSourceVip] = false;
        m_modified = true;

        // 自动保存配置
        saveConfig();

        CONFIG_LOG << "Oran7AppConfigManager::clearLastUsedFile:Last used file cleared";
    }
}

void Oran7AppConfigManager::saveLocalMusicList_playOrder(const QVariantList &playOrderList)
{


    QJsonArray jsonArray;
    for(const QVariant& tx : std::as_const(playOrderList))
    {
        jsonArray.append(QJsonValue::fromVariant(tx));
    }

    setValueQVariantList(PlayOrder,playOrderList);

    saveConfig();
}

QVariantList Oran7AppConfigManager::getLocalMusicList_palyOrder() const
{
    return getValueQVariantList(PlayOrder);
}
