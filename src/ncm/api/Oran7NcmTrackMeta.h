#pragma once
#include <QString>
#include <QVariantMap>

// 网易云曲目唯一值类型；毫秒为内部单位，旧 timesize 仅在界面边界生成。
struct Oran7NcmTrackMeta
{
    qint64 songId = 0;
    QString title, artist, album, cover, url;
    qint64 durationMs = 0;
    bool isVip = false;
    bool hasFee = false;
    QVariantMap toVariantMap() const;
    void completeMissing(const Oran7NcmTrackMeta& detail);
};
