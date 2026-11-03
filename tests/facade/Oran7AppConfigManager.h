#pragma once
#include <QVariantMap>
#include <QStringList>
#include <QJsonObject>
#include "../../src/cpp/Oran7Config/Oran7AppConfig.h"
// 仅替换磁盘配置存储，验证真实 Binding 的键映射；不读取或写入用户 config.json。
class Oran7AppConfigManager {
public:
    QVariantMap values;
    int saves = 0, loads = 0;
    static void put(QJsonObject& object, const QStringList& parts, const QJsonValue& value) {
        if(parts.size()==1){object[parts.first()]=value;return;}
        auto child=object.value(parts.first()).toObject();put(child,parts.mid(1),value);object[parts.first()]=child;
    }
    static void flatten(const QJsonObject& object, const QString& prefix, QVariantMap& result) {
        for(auto i=object.begin();i!=object.end();++i){const auto key=prefix+i.key();if(i->isObject())flatten(i->toObject(),key+".",result);else result[key]=i->toVariant();}
    }
    Oran7AppConfig snapshot() const {
        QJsonObject object;for(auto i=values.begin();i!=values.end();++i)put(object,i.key().split('.'),QJsonValue::fromVariant(i.value()));
        return Oran7AppConfig::fromJson(object);
    }
    void updateConfig(const Oran7AppConfig& config) { flatten(config.toJson(),{},values); }
    QVariant getValueQVariant(const QString& key, const QVariant& fallback = {}) const { return values.value(key, fallback); }
    QVariantList getValueQVariantList(const QString& key) const { return values.value(key).toList(); }
    void setValueQVariant(const QString& key, const QVariant& value) { values[key] = value; }
    void setValueQVariantList(const QString& key, const QVariantList& value) { values[key] = value; }
    bool saveConfig() { ++saves; return true; }
    bool loadConfig() { ++loads; return true; }
    QString getLastUsedFile() const { return values.value("last_used_file").toString(); }
    void setLastUsedFile(const QString& path, bool vip) { values["last_used_file"] = path; values["last_used_file_isvip"] = vip; }
    QVariantList getLocalMusicList_palyOrder() const { return values.value("local_music_sort.custom").toList(); }
    void saveLocalMusicList_playOrder(const QVariantList& order) { values["local_music_sort.custom"] = order; }
};
