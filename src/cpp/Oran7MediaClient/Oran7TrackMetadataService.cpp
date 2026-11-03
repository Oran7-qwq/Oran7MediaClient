#include "Oran7TrackMetadataService.h"
#include "Oran7NcmJsonMapper.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

Oran7TrackMetadataService::Oran7TrackMetadataService()
    : Oran7TrackMetadataService(std::make_unique<QNetworkAccessManager>()) {}

Oran7TrackMetadataService::Oran7TrackMetadataService(std::unique_ptr<QNetworkAccessManager> manager)
    : m_manager(manager ? std::move(manager) : std::make_unique<QNetworkAccessManager>()) {}

Oran7TrackMetadataService::~Oran7TrackMetadataService()
{
    cancel();
    m_manager.reset();
}

void Oran7TrackMetadataService::cancel()
{
    ++m_generation;
    if (!m_reply) return;
    auto reply = m_reply;
    m_reply.clear();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    if (reply) reply->deleteLater();
}

void Oran7TrackMetadataService::fetch(qint64 songId, quint64 trackRevision)
{
    cancel();
    if (songId <= 0) return;
    const auto generation = m_generation;
    QUrl url(QStringLiteral("https://music.163.com/api/song/detail"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"), QStringLiteral("[%1]").arg(songId));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Oran7MediaClient/1.0"));
    request.setRawHeader("Referer", "https://music.163.com");
    request.setTransferTimeout(15000);
    auto* reply = m_manager->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, songId, trackRevision, generation] {
        reply->deleteLater();
        if (generation != m_generation || reply->error() != QNetworkReply::NoError) return;
        m_reply.clear();
        const auto songs = QJsonDocument::fromJson(reply->readAll()).object().value("songs").toArray();
        for (const auto& value : songs) {
            const auto track = Oran7NcmJsonMapper::track(value.toObject());
            if (!track || track->songId != songId) continue;
            const auto timesize = track->durationMs > 0 ? QString::number(track->durationMs / 1000) : QString();
            if (!track->album.isEmpty() || !timesize.isEmpty()) emit detailsReady(songId, trackRevision, track->album, timesize);
            return;
        }
    });
}
