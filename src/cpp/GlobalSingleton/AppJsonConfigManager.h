#ifndef APPJSONCONFIGMANAGER_H
#define APPJSONCONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>

class AppConfigManager : public QObject
{
    Q_OBJECT

public:
    static AppConfigManager& ins();

    // 配置项操作
    QVariant getValueQVariant(const QString& key, const QVariant& defaultValue = QVariant())const;
    QVariantList getValueQVariantList(const QString& key, const QVariantList& defaultValueList = QVariantList())const;
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
    void setLastUsedFile(const QString& filePath);
    void clearLastUsedFile();

    //---<本地缓存排序记录(sort_type)>---//
    void saveLocalMusicList_playOrder(const QVariantList& playOrderList);
    QVariantList getLocalMusicList_palyOrder()const;

signals:
    void configChanged(const QString& key, const QVariant& value);
    void recentFilesChanged();  // 新增信号：最近文件列表发生变化
    void lastUsedFileChanged();  // 最近使用的单个文件改变信号

private:
    explicit AppConfigManager(QObject* parent = nullptr);
    AppConfigManager(const AppConfigManager&) = delete;
    AppConfigManager& operator=(const AppConfigManager&) = delete;
    static AppConfigManager* s_instance;

    QString configFilePath();
    QVariant getNestedValue(const QStringList& keys, const QJsonObject& obj)const;
    void setNestedValue(const QStringList& keys, const QVariant& value, QJsonObject& obj);
    void setNestedValueList(const QStringList& keys, const QVariantList& valueList, QJsonObject& obj);

    void updateRecentFilesArray(const QStringList& files);

    QJsonObject m_config;
    QJsonObject m_defaultConfig;
    bool m_modified = false;        //json被修改标志位
};

#endif // APPJSONCONFIGMANAGER_H
