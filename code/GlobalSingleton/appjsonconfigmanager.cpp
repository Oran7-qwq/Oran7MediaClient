#include "appjsonconfigmanager.h"
#include "globalhelper.h"

#include <utility>

#include <QJsonDocument>
#include <QStandardPaths>
#include <QDebug>

AppConfigManager& AppConfigManager::instance()
{
    static AppConfigManager instance;
    return instance;
}

AppConfigManager::AppConfigManager(QObject* parent) : QObject(parent)
{
    // 设置默认配置
    m_defaultConfig = QJsonObject({
        {"window", QJsonObject({
                       {"width", 1180},
                       {"height", 680},
                       {"maximized", false},
                       {"position", QJsonObject({
                                            {"x", 800},
                                            {"y", 400}
                                        })}
                   })},
        {"appearance", QJsonObject({
                               {"theme", "default"},
                               {"font_size", "default"},
                               {"language", "default"}
                           })},
        {"user", QJsonObject({
                     {"username", "default"},
                     {"auto_login", false},
                     {"remember_password", true}
                 })},
        {"recent_files", QJsonArray()},
        {"recent_files_max_count", 10}, // 最大最近文件数量
        {"last_used_file", ""}, // 最近使用的单个文件路径
        {"playerVolume",25},//Global PlayerVolume
        //本地缓存排序模块记录
        {"local_music_sort",QJsonObject({
                                        {"last_modified",""},
                                        {"sort_type","birth_time"},//默认按birth_time排序，custom->用户指定排序
                                        {"play_order",QJsonArray()}})}
    });

    m_config = m_defaultConfig;
}

QString AppConfigManager::configFilePath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("config.json");
}

bool AppConfigManager::loadConfig()
{
    QFile file(configFilePath());
    QFileInfo fileInfo(file);
    //qDebug()<<fileInfo.absoluteFilePath();
    if(!QFile::exists(fileInfo.absoluteFilePath()))//**首次加载--->Config文件不存在，加载默认配置文件
    {
        m_config = m_defaultConfig;
        m_modified = true;
        saveConfig();
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        WARNING_LOG << "AppConfigManager::loadConfig:Cannot open config file, using defaults";
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull() || !doc.isObject()) {
        WARNING_LOG << "Invalid config file, using defaults";
        return false;
    }

    m_config = doc.object();
    m_modified = false;

    // 确保最近文件配置存在
    if (!m_config.contains("recent_files")) {
        m_config["recent_files"] = QJsonArray();
    }
    if (!m_config.contains("recent_files_max_count")) {
        m_config["recent_files_max_count"] = 10;
    }
    //确保最后使用文件配置存在
    if (!m_config.contains("last_used_file")){
        m_config["last_used_file"] = "";
    }

    return true;
}

bool AppConfigManager::saveConfig()
{
    if (!m_modified) return true;

    QFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly))
    {
        WARNING_LOG << "AppConfigManager::saveConfig:Cannot open config file for writing";
        return false;
    }
    QJsonDocument doc(m_config);
    //doc.toJson(QJsonDocument::Indented)：将JSON对象转换为格式化的字符串
    //QJsonDocument::Indented：带缩进的易读格式
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_modified = false;
    return true;
}

QVariant AppConfigManager::getValueQVariant(const QString& key, const QVariant& defaultValue)const
{
    QStringList keys = key.split('.');
    QVariant result = getNestedValue(keys, m_config);

    if (!result.isValid() || result.isNull()) {
        //qDebug()<<"[AppConfigManager::getValueQVariant:]Retrun Default value:key"<<*keys.end()<<"value"<<defaultValue;
        return defaultValue;
    }
    //qDebug()<<"[AppConfigManager::getValueQVariant:]Retrun Config result value"<<result;

    return result;
}

QVariantList AppConfigManager::getValueQVariantList(const QString &key, const QVariantList &defaultValueList) const
{
    QStringList keys = key.split(".");
    QVariant result=getNestedValue(keys,m_config);
    if(! result.isValid() || result.isNull())return defaultValueList;

    QVariantList resultList = result.toList();

    for(const QVariant& qv : std::as_const(resultList))
    {
        if(!qv.isValid() || qv.isNull())return defaultValueList;
    }

    return resultList;
}

/**
 * @brief AppConfigManager::setValue  往JSON文件中对应的key设置value
 * @param key    传入的QString key列表格式"key1.key2.key3",对应一层二层三层,最终目标键为key3
 *                          在内部已经通过QStringList keys = key.split('.');将QString通过split('.')存储为QStringList
 * @param value  需要绑定的最深层的值
 */
void AppConfigManager::setValueQVariant(const QString& key, const QVariant& value)
{
    QStringList keys = key.split('.');
    QJsonObject newConfig = m_config;

    setNestedValue(keys, value, newConfig);//递归存储调用

    /**
     *  ------>  ：关于QJsonObject operator==() 和QJsonObject operator!=() 判断标准--> 两个 QJsonObject 必须有相同数量的键值对才能相等
     *                    键名必须完全相同（包括大小写），QJsonObject内部的键值对的存储是无序的，键的顺序不影响相等性判断
     */
    if (newConfig != m_config)
    {
        m_config = newConfig;
        m_modified = true;
        // emit configChanged(key, value);
    }
}

void AppConfigManager::setValueQVariantList(const QString &key, const QVariantList &valueList)
{
    QStringList keys = key.split(".");
    QJsonObject newConfig = m_config;

    setNestedValueList(keys,valueList,newConfig);

    if(newConfig != m_config)
    {
        m_config = newConfig;
        m_modified = true;
    }
}

QVariant AppConfigManager::getNestedValue(const QStringList& keys, const QJsonObject& obj) const
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
 * @brief AppConfigManager::setNestedValue      递归函数 递归搜索赋值Key对应的Value并修改赋值
 * @param keys
 * @param value
 * @param obj
 */
void AppConfigManager::setNestedValue(const QStringList& keys, const QVariant& value, QJsonObject& obj)
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

void AppConfigManager::setNestedValueList(const QStringList &keys,
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

bool AppConfigManager::validateConfig()
{
    // 简单的配置验证逻辑
    int width = getValueQVariant("window.width").toInt();
    if (width <= 0 || width > 4096) {
        setValueQVariant("window.width", 800);
    }

    QString theme = getValueQVariant("appearance.theme").toString();
    if (theme != "dark" && theme != "light" && theme != "auto") {
        setValueQVariant("appearance.theme", "dark");
    }

    return true;
}

void AppConfigManager::resetToDefaults()
{
    m_config = m_defaultConfig;
    m_modified = true;
    saveConfig();
    // emit recentFilesChanged();
}

// ==================== 最近文件管理功能实现 ====================

void AppConfigManager::addRecentFile(const QString& filePath)
{
    if (filePath.isEmpty()) return;

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

    CONFIG_LOG << "AppConfigManager::addRecentFile:Added recent file:" << filePath << "Total:" << recentFiles.size();
}

void AppConfigManager::removeRecentFile(const QString& filePath)
{
    QStringList recentFiles = getRecentFiles();

    if (recentFiles.removeAll(filePath) > 0) {
        updateRecentFilesArray(recentFiles);
        CONFIG_LOG << "Removed recent file:" << filePath;
    }
}

void AppConfigManager::clearRecentFiles()
{
    updateRecentFilesArray(QStringList());
    CONFIG_LOG << "Cleared all recent files";
}

QStringList AppConfigManager::getRecentFiles() const
{
    QStringList recentFiles;

    if (m_config.contains("recent_files") && m_config["recent_files"].isArray())
    {
        QJsonArray array = m_config["recent_files"].toArray();
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

int AppConfigManager::getMaxRecentFiles() const
{
    if (m_config.contains("recent_files_max_count")) {
        return m_config["recent_files_max_count"].toInt(10);
    }
    return 10;
}

void AppConfigManager::setMaxRecentFiles(int maxCount)
{
    if (maxCount < 1) maxCount = 1;
    if (maxCount > 50) maxCount = 50; // 设置合理上限

    setValueQVariant("recent_files_max_count", maxCount);

    // 如果当前文件数量超过新限制，需要截断
    QStringList recentFiles = getRecentFiles();
    if (recentFiles.size() > maxCount) {
        recentFiles = recentFiles.mid(0, maxCount);
        updateRecentFilesArray(recentFiles);
    }
}

void AppConfigManager::updateRecentFilesArray(const QStringList& files)
{
    QJsonArray jsonArray;
    for (const QString& filePath : files) {
        jsonArray.append(filePath);
    }

    m_config["recent_files"] = jsonArray;
    m_modified = true;

    // emit recentFilesChanged();
    // emit configChanged("recent_files", files);

    // 自动保存配置
    saveConfig();
}

// ==================== 最近使用文件管理功能实现 ====================

QString AppConfigManager::getLastUsedFile() const
{
    if (m_config.contains("last_used_file") && m_config["last_used_file"].isString())
    {
        QString lastFile = m_config["last_used_file"].toString();
        // 检查文件是否仍然存在
        if (!lastFile.isEmpty() && QFile::exists(lastFile))
        {
            return lastFile;
        }
    }
    return "";
}

void AppConfigManager::setLastUsedFile(const QString& filePath)
{
    if (filePath.isEmpty())
    {
        clearLastUsedFile();
        return;
    }

    // 验证文件路径是否有效
    if (!QFile::exists(filePath))
    {
        WARNING_LOG << "AppConfigManager::setLastUsedFile:File does not exist:" << filePath;
        return;
    }

    QString currentLastFile = getLastUsedFile();
    if (currentLastFile != filePath)
    {
        m_config["last_used_file"] = filePath;
        m_modified = true;

        // 自动保存配置
        saveConfig();

        // emit lastUsedFileChanged();
        // emit configChanged("last_used_file", filePath);

        CONFIG_LOG << "AppConfigManager::setLastUsedFile:Last used file set to:" << filePath;
    }
}

void AppConfigManager::clearLastUsedFile()
{
    if (m_config.contains("last_used_file") && !m_config["last_used_file"].toString().isEmpty())
    {
        m_config["last_used_file"] = "";
        m_modified = true;

        // 自动保存配置
        saveConfig();

        CONFIG_LOG << "AppConfigManager::clearLastUsedFile:Last used file cleared";
    }
}

void AppConfigManager::saveLocalMusicList_playOrder(const QVariantList &playOrderList)
{
    if(playOrderList.isEmpty())return;

    QJsonArray jsonArray;
    for(const QVariant& tx : std::as_const(playOrderList))
    {
        jsonArray.append(QJsonValue::fromVariant(tx));
    }

    setValueQVariantList("local_music_sort.play_order",playOrderList);

    saveConfig();
}

QVariantList AppConfigManager::getLocalMusicList_palyOrder() const
{
    return getValueQVariantList("local_music_sort.play_order");
}
