#pragma once
#include <QJsonObject>
#include <QMap>
#include <QVariant>

// 无对象状态，不访问 Client；可从后台扫描或 GUI 的恢复/歌词入口调用。
class Oran7MediaInfoReader
{
public:
    static QMap<QString, QVariant> analyzeMediaFileInfo(QString filepath);
    static QMap<QString, QVariant> extractMediaInfo(const QJsonObject& root);
private:
    static bool isNetworkUrl(const QString& path);
    static QVariant getNestedValue(const QStringList& keys, const QJsonObject& obj);
    static QString extractMusicInfoTag(const QJsonObject& root, const QString& tagName,
                                      const QString& defaultValue = "");
};
