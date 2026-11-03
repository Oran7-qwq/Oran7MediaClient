#pragma once
#include "Oran7NcmTrackMeta.h"
#include <QJsonObject>
#include <QHash>
#include <optional>

// 搜索、song/detail、漫游内嵌/平铺曲目和播放地址共用解析入口。
class Oran7NcmJsonMapper
{
public:
    static std::optional<Oran7NcmTrackMeta> track(const QJsonObject& object);
    static QHash<qint64, QString> playbackUrls(const QJsonObject& root);
};
