#include "Oran7VideoHistory.h"
#include "Oran7AppConfigManager.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QUrl>
#include <utility>

Oran7VideoHistory::Oran7VideoHistory(Oran7AppConfigManager& config, QString directory, QString executable,
    std::unique_ptr<QNetworkAccessManager> roomNetwork)
    : m_config(config), m_thumbnails(std::move(directory),std::move(executable)),m_roomInfo(std::move(roomNetwork))
{
    connect(&m_roomInfo,&Oran7LiveRoomInfoService::started,this,[this](const QString& room) {
        m_roomInfoState.insert(room,1);
        for (int i=0;i<m_records.size();++i)
            if (m_records.at(i).toMap().value("target")==room)
                emit dataChanged(index(i,0),index(i,0),{RoomInfoState,LiveStatus});
    });
    connect(&m_roomInfo,&Oran7LiveRoomInfoService::finished,this,[this](const QString& room,const QVariantMap& info) {
        m_roomInfoState.insert(room,info.isEmpty()?3:2);
        for (int i=0;i<m_records.size();++i) {
            auto row=m_records.at(i).toMap();
            if (row.value("mediaKind")!="live" || row.value("target")!=room) continue;
            for (auto it=info.begin();it!=info.end();++it)
                if (it.value().typeId()!=QMetaType::QString || !it.value().toString().isEmpty()) row[it.key()]=it.value();
            if (!row.value("roomTitle").toString().isEmpty()) row["title"]=row.value("roomTitle");
            m_records[i]=row;
            emit dataChanged(index(i,0),index(i,0),{Title,Thumbnail,AnchorName,AvatarUrl,LiveStatus,RoomInfoState});
            if (i==0) emit recordsChanged();
            if (!info.isEmpty()) { m_dirty=true; m_saveTimer.start(); }
            return;
        }
    });
    connect(&m_thumbnails,&Oran7VideoThumbnailCache::ready,this,[this](const QString& path,const QString& url) {
        for (int i=0;i<m_records.size();++i) {
            if (m_records.at(i).toMap().value("target").toString() != path) continue;
            if (m_thumbnailUrls.value(path) == url) return;
            m_thumbnailUrls.insert(path,url);
            emit dataChanged(index(i,0),index(i,0),{Thumbnail});
            if (i == 0) emit recordsChanged();
            return;
        }
    });
    QSet<QString> seen;
    int local = 0, live = 0;
    for (const auto& value : config.snapshot().videoHistory) {
        const auto record = normalized(value.toMap());
        if (record.isEmpty() || seen.contains(identity(record))) continue;
        auto& count = record.value("mediaKind") == "local" ? local : live;
        if (count >= 50) continue;
        ++count;
        seen.insert(identity(record));
        m_records.append(record);
    }
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] { flush(); });
    m_progressSaveTimer.setSingleShot(true);
    m_progressSaveTimer.setInterval(5000);
    connect(&m_progressSaveTimer,&QTimer::timeout,this,[this] { flush(); });
}

QVariantMap Oran7VideoHistory::normalized(const QVariantMap& record)
{
    const auto kind = record.value("mediaKind").toString();
    auto target = record.value("target").toString().trimmed();
    QString title;
    if (kind == "local") {
        if (target.startsWith("file:", Qt::CaseInsensitive)) target = QUrl(target).toLocalFile();
        target = QDir::cleanPath(QDir::fromNativeSeparators(target));
        if (!QFileInfo(target).isAbsolute() || QUrl(target).scheme().size() > 1) return {};
        title = QFileInfo(target).fileName();
    } else if (kind == "live") {
        bool ok = false;
        const auto number = target.toULongLong(&ok);
        if (!ok || !number || number > 999999999999ULL) return {};
        target = QString::number(number);
        title = record.value("roomTitle").toString().isEmpty() ? target : record.value("roomTitle").toString();
    } else return {};
    if (target.isEmpty() || title.isEmpty()) return {};
    const auto duration = kind == "local" ? qMax(qint64(0),record.value("durationMs").toLongLong()) : 0;
    const auto position = qBound(qint64(0),record.value("positionMs").toLongLong(),duration);
    QVariantMap result{{"mediaKind", kind}, {"target", target}, {"title", title},
            {"lastOpenedAt", record.value("lastOpenedAt").toString()},
            {"positionMs",position},{"durationMs",duration},{"completed",kind == "local" && record.value("completed").toBool()}};
    if (kind=="live") {
        for (const auto& key : {"roomTitle","anchorName","coverUrl","avatarUrl","realRoomId","infoUpdatedAt"})
            result.insert(key,record.value(key).toString());
        result["liveStatus"]=record.value("liveStatus",-1).toInt();
    }
    return result;
}

QString Oran7VideoHistory::identity(const QVariantMap& record)
{
    return record.value("mediaKind").toString() + "|" + record.value("target").toString().toCaseFolded();
}

int Oran7VideoHistory::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : m_records.size(); }
QVariant Oran7VideoHistory::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size()) return {};
    const auto row=m_records.at(index.row()).toMap();
    const auto room=row.value("target").toString();
    if (role == Thumbnail) return row.value("mediaKind")=="live" ? row.value("coverUrl",QString{}) : m_thumbnailUrls.value(room);
    if (role == RoomInfoState) return m_roomInfoState.value(room);
    if (role == LiveStatus) return m_roomInfoState.value(room)==2 ? row.value("liveStatus",-1) : -1;
    if (role == AnchorName || role == AvatarUrl) return row.value(QString::fromLatin1(roleNames().value(role)),QString{});
    return m_records.at(index.row()).toMap().value(QString::fromLatin1(roleNames().value(role)));
}
QHash<int, QByteArray> Oran7VideoHistory::roleNames() const
{
    return {{MediaKind,"mediaKind"},{Target,"target"},{Title,"title"},{LastOpenedAt,"lastOpenedAt"},{Thumbnail,"thumbnail"},
            {PositionMs,"positionMs"},{DurationMs,"durationMs"},{Completed,"completed"},
            {AnchorName,"anchorName"},{AvatarUrl,"avatarUrl"},{LiveStatus,"liveStatus"},{RoomInfoState,"roomInfoState"}};
}
QVariantMap Oran7VideoHistory::latest() const
{
    if (m_records.isEmpty()) return {};
    auto record = m_records.first().toMap();
    record.insert("thumbnail",record.value("mediaKind")=="live" ? record.value("coverUrl",QString{})
        : m_thumbnailUrls.value(record.value("target").toString()));
    return record;
}
void Oran7VideoHistory::requestRoomInfo(const QString& room,bool force)
{
    for (const auto& row : std::as_const(m_records)) {
        const auto record=row.toMap();
        if (record.value("mediaKind")=="live" && record.value("target")==room) {
            m_roomInfo.request(room,force);
            return;
        }
    }
}
void Oran7VideoHistory::requestThumbnail(const QString& path)
{
    for (const auto& row : std::as_const(m_records)) {
        const auto record = row.toMap();
        if (record.value("mediaKind") == "local" && record.value("target") == path) {
            m_thumbnails.request(path);
            return;
        }
    }
}
int Oran7VideoHistory::localCount() const
{
    int count = 0;
    for (const auto& record : m_records) if (record.toMap().value("mediaKind") == "local") ++count;
    return count;
}
int Oran7VideoHistory::liveCount() const { return m_records.size() - localCount(); }
void Oran7VideoHistory::rememberLocal(const QString& path) { remember({{"mediaKind","local"},{"target",path}}); }
void Oran7VideoHistory::rememberRoom(const QString& room) { remember({{"mediaKind","live"},{"target",room}}); }

void Oran7VideoHistory::remember(QVariantMap record)
{
    record = normalized(record);
    if (record.isEmpty()) return;
    record["lastOpenedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QVariantList records{record};
    int sameKindCount = 1;
    for (const auto& previous : std::as_const(m_records)) {
        const auto row = previous.toMap();
        if (identity(row) == identity(record)) {
            auto updated=row;
            updated["target"]=record.value("target");
            updated["lastOpenedAt"]=record.value("lastOpenedAt");
            if (record.value("mediaKind")=="local") updated["title"]=record.value("title");
            records[0]=updated;
            continue;
        }
        if (row.value("mediaKind") == record.value("mediaKind") && ++sameKindCount > 50) continue;
        records.append(row);
    }
    publish(records);
}

void Oran7VideoHistory::remove(const QString& kind, const QString& target)
{
    const auto record = normalized({{"mediaKind",kind},{"target",target}});
    if (record.isEmpty()) return;
    auto records = m_records;
    for (int i = records.size() - 1; i >= 0; --i)
        if (identity(records.at(i).toMap()) == identity(record)) records.removeAt(i);
    if (records != m_records) publish(records);
}

void Oran7VideoHistory::publish(QVariantList records)
{
    beginResetModel();
    m_records = std::move(records);
    QSet<QString> retained;
    for (const auto& record : std::as_const(m_records)) retained.insert(record.toMap().value("target").toString());
    for (auto it=m_thumbnailUrls.begin();it!=m_thumbnailUrls.end();) {
        if (!retained.contains(it.key())) it=m_thumbnailUrls.erase(it);
        else ++it;
    }
    for (auto it=m_roomInfoState.begin();it!=m_roomInfoState.end();) {
        if (!retained.contains(it.key())) it=m_roomInfoState.erase(it);
        else ++it;
    }
    endResetModel();
    m_dirty = true;
    emit recordsChanged();
    m_saveTimer.start();
}

bool Oran7VideoHistory::flush()
{
    m_saveTimer.stop();
    m_progressSaveTimer.stop();
    if (!m_dirty) return true;
    auto config = m_config.snapshot();
    config.videoHistory = m_records;
    m_config.updateConfig(config);
    const bool saved = m_config.saveConfig();
    m_dirty = !saved;
    const auto error = saved ? QString{} : m_config.lastError();
    if (m_error != error) { m_error = error; emit recordsChanged(); }
    return saved;
}

void Oran7VideoHistory::bindPlayback(Oran7PlaybackState& playback)
{
    m_playback = &playback;
    connect(&playback,&Oran7PlaybackState::sourceChanged,this,[this] {
        m_activeSource.clear();
        m_resumePending = false;
        flush();
    });
    connect(&playback,&Oran7PlaybackState::stateChanged,this,[this] {
        if (!m_playback || m_playback->source() != m_activeSource) return;
        if (m_playback->playing() && !m_started) {
            m_started = true;
            rememberLocal(m_activeSource);
        }
        restoreProgress();
        captureProgress();
        if (m_playback->state() == Oran7PlaybackState::Paused || m_playback->state() == Oran7PlaybackState::Ended)
            flush();
    });
    connect(&playback,&Oran7PlaybackState::durationChanged,this,[this] { restoreProgress(); captureProgress(); });
    connect(&playback,&Oran7PlaybackState::positionChanged,this,[this] { captureProgress(); });
}

void Oran7VideoHistory::beginLocalPlayback(const QString& path)
{
    m_activeSource.clear();
    m_resumePending = false;
    m_started = false;
    const auto record = normalized({{"mediaKind","local"},{"target",path}});
    if (record.isEmpty()) return;
    m_activeSource = path;
    m_resumeMs = 0;
    for (const auto& value : std::as_const(m_records)) {
        const auto row = value.toMap();
        if (identity(row) == identity(record) && !row.value("completed").toBool())
            m_resumeMs = row.value("positionMs").toLongLong();
    }
    m_resumePending = m_resumeMs > 0;
}

void Oran7VideoHistory::restoreProgress()
{
    if (!m_resumePending || !m_playback || m_playback->source() != m_activeSource
        || m_playback->durationMs() <= 0
        || m_playback->state() != Oran7PlaybackState::Playing) return;
    m_resumePending = false;
    if (m_resumeMs < m_playback->durationMs() - 1000)
        emit resumeRequested(int(m_resumeMs / 1000));
}

void Oran7VideoHistory::captureProgress()
{
    if (!m_playback || m_resumePending || m_activeSource.isEmpty() || m_playback->source() != m_activeSource) return;
    const auto state = m_playback->state();
    if (state != Oran7PlaybackState::Playing && state != Oran7PlaybackState::Paused
        && state != Oran7PlaybackState::Seeking && state != Oran7PlaybackState::Ended) return;
    const auto duration = m_playback->durationMs();
    if (duration <= 0) return;
    const bool finished = state == Oran7PlaybackState::Ended;
    const auto position = finished ? duration : qBound(qint64(0),m_playback->positionMs(),duration);
    const auto key = identity(normalized({{"mediaKind","local"},{"target",m_activeSource}}));
    for (int i=0;i<m_records.size();++i) {
        auto row = m_records.at(i).toMap();
        if (identity(row) != key) continue;
        if (row.value("positionMs").toLongLong() == position && row.value("durationMs").toLongLong() == duration
            && row.value("completed").toBool() == finished) return;
        row["positionMs"] = position;
        row["durationMs"] = duration;
        row["completed"] = finished;
        m_records[i] = row;
        m_dirty = true;
        emit dataChanged(index(i,0),index(i,0),{PositionMs,DurationMs,Completed});
        if (i == 0) emit recordsChanged();
        if (!m_progressSaveTimer.isActive()) m_progressSaveTimer.start();
        return;
    }
}
