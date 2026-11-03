#ifndef ORAN7APPCONFIGMANAGER_H
#define ORAN7APPCONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include "../Oran7Config/Oran7AppConfig.h"
#include "../Oran7Config/Oran7CredentialStore.h"
#include <QMap>
#include <QSet>

class Oran7AppConfigManager : public QObject
{
    Q_OBJECT

public:
    static Oran7AppConfigManager& ins();

    // 独立存储入口，用于隔离配置读写；不会访问进程单例或用户目录。
    Oran7AppConfigManager(const QString& filePath, const QJsonObject& defaults, QObject* parent = nullptr);
    Oran7AppConfig snapshot() const { return Oran7AppConfig::fromJson(m_config); }
    void updateConfig(const Oran7AppConfig& config);
    QString lastError() const { return m_lastError; }
    bool isWritable() const { return m_writable; }
    QString credentialError(const QString& group = {}) const
    { return group.isEmpty() ? m_credentialErrors.values().join(QStringLiteral("；")) : m_credentialErrors.value(group); }
    QString credentialDirectory() const { return m_credentials.directory(); }
    bool clearCredentialGroup(const QString& group);

    // 配置项操作
    QVariant getValueQVariant(const QString& key)const;
    QVariantList getValueQVariantList(const QString& key)const;
    void setValueQVariant(const QString& key, const QVariant& value);
    void setValueQVariantList(const QString& key,const QVariantList& valueList);

    // ---<文件操作>---//
    bool loadConfig();
    bool saveConfig();

    // ---<配置验证和重置>---//
    bool validateConfig();
    void resetToDefaults();

    //---<最近文件管理功能>---//
    void addRecentFile(const QString& filePath);
    void removeRecentFile(const QString& filePath);
    void clearRecentFiles();
    QStringList getRecentFiles() const;
    int getMaxRecentFiles() const;
    void setMaxRecentFiles(int maxCount);

    //---<最近的一次使用的最后一个文件管理功能>---//
    QString getLastUsedFile() const;
    void setLastUsedFile(const QString& filePath, bool isVip = false);
    bool getLastUsedFileIsVip() const;
    void clearLastUsedFile();

    //---<本地缓存排序记录(sort_type)>---//
    void saveLocalMusicList_playOrder(const QVariantList& playOrderList);
    QVariantList getLocalMusicList_palyOrder()const;

signals:
    void configChanged(const QString& key, const QVariant& value);
    void recentFilesChanged();  // 新增信号：最近文件列表发生变化
    void lastUsedFileChanged();  // 最近使用的单个文件改变信号

private:
    explicit Oran7AppConfigManager(QObject* parent = nullptr);
    Oran7AppConfigManager(const Oran7AppConfigManager&) = delete;
    Oran7AppConfigManager& operator=(const Oran7AppConfigManager&) = delete;

    QString configFilePath();
    QVariant getNestedValue(const QStringList& keys, const QJsonObject& obj)const;
    void setNestedValue(const QStringList& keys, const QVariant& value, QJsonObject& obj);
    void setNestedValueList(const QStringList& keys, const QVariantList& valueList, QJsonObject& obj);

    void updateRecentFilesArray(const QStringList& files);
    bool restoreCredentials(QJsonObject* config);

    QJsonObject m_config;
    QJsonObject m_defaultConfig;
    bool m_modified = false;        //json被修改标志位
    QString m_filePath, m_lastError;
    Oran7CredentialStore m_credentials;
    QJsonObject m_credentialRefs, m_credentialBaseline;
    QMap<QString, QString> m_credentialErrors;
    QSet<QString> m_forceCredentialWrites;
    bool m_loaded = false, m_loadSucceeded = false, m_writable = true, m_backupNeeded = false;
};

#endif // ORAN7APPCONFIGMANAGER_H
