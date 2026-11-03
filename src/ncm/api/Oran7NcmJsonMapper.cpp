#include "Oran7NcmJsonMapper.h"
#include <QJsonArray>
#include <QStringList>

QVariantMap Oran7NcmTrackMeta::toVariantMap() const
{
    return {{"songId",songId},{"title",title},{"artist",artist},{"album",album},{"cover",cover},
        {"durationMs",durationMs},{"timesize",QString::number(durationMs / 1000)},
        {"isVip",isVip},{"url",url},{"path",url},{"source",QStringLiteral("netease")}};
}
void Oran7NcmTrackMeta::completeMissing(const Oran7NcmTrackMeta& detail)
{
    if (songId != detail.songId) return;
    if (title.isEmpty()) title = detail.title;
    if (artist.isEmpty()) artist = detail.artist;
    if (album.isEmpty()) album = detail.album;
    if (cover.isEmpty()) cover = detail.cover;
    if (durationMs <= 0) durationMs = detail.durationMs;
    if (detail.hasFee) { hasFee = true; isVip = detail.isVip; }
}
std::optional<Oran7NcmTrackMeta> Oran7NcmJsonMapper::track(const QJsonObject& object)
{
    const auto nested = object.value("songInfo").toObject();
    const auto song = nested.isEmpty() ? object : nested;
    Oran7NcmTrackMeta result;
    result.songId = song.value("id").toVariant().toLongLong();
    if (result.songId <= 0) result.songId = object.value("id").toVariant().toLongLong();
    if (result.songId <= 0) return std::nullopt;
    result.title = song.value("name").toString();
    auto artists = song.value("ar").toArray();
    if (artists.isEmpty()) artists = song.value("artists").toArray();
    QStringList names;
    for (const auto& value : artists) {
        const auto name = value.toObject().value("name").toString();
        if (!name.isEmpty()) names.append(name);
    }
    result.artist = names.join(QStringLiteral(" / "));
    const auto album = song.value("al").toObject();
    const auto fallbackAlbum = song.value("album").toObject();
    result.album = album.value("name").toString();
    if (result.album.isEmpty()) result.album = fallbackAlbum.value("name").toString();
    result.cover = album.value("picUrl").toString();
    if (result.cover.isEmpty()) result.cover = fallbackAlbum.value("picUrl").toString();
    if (result.cover.isEmpty() && !artists.isEmpty()) {
        const auto artist = artists.first().toObject();
        result.cover = artist.value("picUrl").toString();
        if (result.cover.isEmpty()) result.cover = artist.value("img1v1Url").toString();
    }
    result.durationMs = song.value("dt").toVariant().toLongLong();
    if (result.durationMs <= 0) result.durationMs = song.value("duration").toVariant().toLongLong();
    result.durationMs = qMax(qint64(0), result.durationMs);
    result.hasFee = song.contains("fee") && !song.value("fee").isNull();
    result.isVip = song.value("fee").toVariant().toInt() == 1;
    return result;
}
QHash<qint64, QString> Oran7NcmJsonMapper::playbackUrls(const QJsonObject& root)
{
    QHash<qint64, QString> result;
    for (const auto& value : root.value("data").toArray()) {
        const auto item = value.toObject();
        const auto id = item.value("id").toVariant().toLongLong();
        const auto url = item.value("url").toString();
        if (id > 0 && !url.isEmpty()) result.insert(id, url);
    }
    return result;
}
