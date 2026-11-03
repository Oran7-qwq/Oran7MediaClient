#pragma once
#include "Oran7TrackListModel.h"
#include <memory>

// 四类列表的所有权、去重及成员关系；不接管网络请求或播放队列策略。
class Oran7TrackLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Oran7TrackListModel* local READ local CONSTANT)
    Q_PROPERTY(Oran7TrackListModel* integrated READ integrated CONSTANT)
    Q_PROPERTY(Oran7TrackListModel* liked READ liked CONSTANT)
    Q_PROPERTY(Oran7TrackListModel* search READ search CONSTANT)
public:
    explicit Oran7TrackLibrary(QObject* parent = nullptr);
    Oran7TrackListModel* local() const { return m_local.get(); }
    Oran7TrackListModel* integrated() const { return m_integrated.get(); }
    Oran7TrackListModel* liked() const { return m_liked.get(); }
    Oran7TrackListModel* search() const { return m_search.get(); }
    void replaceLocalTracks(const QVariantList& tracks);
    void restoreIntegratedTracks(const QVariantList& tracks);
    QVariantList integratedTracks() const;
    Q_INVOKABLE bool isIntegrated(qint64 songId, const QString& url, const QString& path) const;
    Q_INVOKABLE bool isLiked(qint64 songId) const;
    Q_INVOKABLE void addIntegratedTrack(const QVariantMap& track);
    Q_INVOKABLE void addIntegratedTracks(const QVariantList& tracks);
    Q_INVOKABLE bool removeIntegratedTrack(qint64 songId, const QString& url, const QString& path);
    Q_INVOKABLE void addLikedTrack(const QVariantMap& track);
    Q_INVOKABLE bool removeLikedTrack(qint64 songId);
    Q_INVOKABLE void replaceLikedTracks(const QVariantList& tracks);
    Q_INVOKABLE void clearLikedTracks();
    Q_INVOKABLE void replaceSearchResults(const QVariantList& tracks);
    Q_INVOKABLE void clearSearchResults();
signals:
    void integratedMusicChanged(const QVariantList& tracks);
private:
    void refreshMembership();
    std::unique_ptr<Oran7TrackListModel> m_local;
    std::unique_ptr<Oran7TrackListModel> m_integrated;
    std::unique_ptr<Oran7TrackListModel> m_liked;
    std::unique_ptr<Oran7TrackListModel> m_search;
    bool m_restoring = false;
};
