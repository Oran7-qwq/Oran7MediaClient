#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <optional>

// 当前音乐选择的值快照。path 为列表/恢复入口路径，实际解码地址仍见 playback.source。
struct Oran7Track
{
    Q_GADGET
    Q_PROPERTY(QString path READ getPath)
    Q_PROPERTY(QString title READ getTitle)
    Q_PROPERTY(QString artist READ getArtist)
    Q_PROPERTY(QString album READ getAlbum)
    Q_PROPERTY(QString cover READ getCover)
    Q_PROPERTY(QString timesize READ getTimesize)
    Q_PROPERTY(qint64 songId READ getSongId)
    Q_PROPERTY(bool isVip READ getIsVip)
public:
    QString path, title, artist, album, cover, timesize;
    qint64 songId = 0;
    bool isVip = false;

    QString getPath() const { return path; }
    QString getTitle() const { return title; }
    QString getArtist() const { return artist; }
    QString getAlbum() const { return album; }
    QString getCover() const { return cover; }
    QString getTimesize() const { return timesize; }
    qint64 getSongId() const { return songId; }
    bool getIsVip() const { return isVip; }
    // QML 提交具名字段；ID 允许十进制字符串，避免先转为 JS 浮点数。
    static std::optional<Oran7Track> fromVariantMap(const QVariantMap& fields);

    bool operator==(const Oran7Track& other) const {
        return path == other.path && title == other.title && artist == other.artist
            && album == other.album && cover == other.cover && timesize == other.timesize
            && songId == other.songId && isVip == other.isVip;
    }
};
Q_DECLARE_METATYPE(Oran7Track)

inline std::optional<Oran7Track> Oran7Track::fromVariantMap(const QVariantMap& fields)
{
    bool ok = true;
    const auto id = fields.value(QStringLiteral("songId"));
    const qint64 songId = id.isValid() ? id.toLongLong(&ok) : 0;
    if (!ok || songId < 0) return std::nullopt;
    Oran7Track track;
    track.path = fields.value(QStringLiteral("path")).toString();
    track.title = fields.value(QStringLiteral("title")).toString();
    track.artist = fields.value(QStringLiteral("artist")).toString();
    track.album = fields.value(QStringLiteral("album")).toString();
    track.cover = fields.value(QStringLiteral("cover")).toString();
    track.timesize = fields.value(QStringLiteral("timesize")).toString();
    track.songId = songId;
    track.isVip = fields.value(QStringLiteral("isVip"), false).toBool();
    return track;
}
