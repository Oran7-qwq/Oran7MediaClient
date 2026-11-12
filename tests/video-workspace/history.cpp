#include "Oran7VideoHistory.h"
#include "Oran7AppConfigManager.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QFile>
#include <cstdio>
#include <cstdlib>

static void check(bool ok,const char* message)
{
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    QTemporaryDir dir;
    QFile file(QStringLiteral(APP_ROOT)+"/config/default.json");
    check(file.open(QIODevice::ReadOnly),"read defaults");
    const auto defaults=QJsonDocument::fromJson(file.readAll()).object();
    const auto path=dir.filePath("history.json");
    {
        Oran7AppConfigManager config(path,defaults);
        Oran7VideoHistory model(config,dir.filePath("thumbnails"));
        model.rememberLocal("C:/Videos/Film.mp4");
        model.rememberLocal("c:\\videos\\film.mp4");
        model.rememberRoom("001234");
        model.rememberRoom("https://fixture.invalid/private?token=secret");
        model.rememberLocal("https://fixture.invalid/video.mp4");
        check(model.localCount()==1 && model.liveCount()==1,"case-insensitive local deduplication and safe room identity");
        check(model.latest().value("target")=="1234","room numbers normalize without storing signed URLs");
        check(model.flush(),"history flushes to temporary config");
        auto snapshot=config.snapshot();
        snapshot.player.volume=37;
        config.updateConfig(snapshot);
        check(config.saveConfig(),"other configuration saves alongside history");
    }
    {
        Oran7AppConfigManager config(path,defaults);
        Oran7VideoHistory model(config,dir.filePath("thumbnails"));
        check(model.localCount()==1 && model.liveCount()==1 && config.snapshot().player.volume==37,
              "a fresh service restores previous process history and preserves unrelated settings");
        model.remove("live","1234");
        check(model.liveCount()==0 && model.flush(),"removing a record persists");
        for (int i=1;i<=70;++i) model.rememberRoom(QString::number(i));
        check(model.liveCount()==50 && model.localCount()==1,"bounded recent history preserves both categories");
        check(model.flush(),"bounded history persists");
    }
    Oran7AppConfigManager config(path,defaults);
    Oran7VideoHistory restored(config,dir.filePath("thumbnails"));
    check(restored.liveCount()==50 && restored.latest().value("target")=="70","latest visit remains first after reload");
    Oran7PlaybackState playback;
    restored.bindPlayback(playback);
    const QString movie="C:/Videos/progress.mp4";
    int restores=0;
    QObject::connect(&restored,&Oran7VideoHistory::resumeRequested,&playback,[&](int seconds) {
        ++restores;
        playback.beginSeek(qint64(seconds)*1000);
        playback.finishSeek();
    });
    playback.beginOpening(movie);
    restored.beginLocalPlayback(movie);
    playback.setDuration(300000);
    playback.setState(Oran7PlaybackState::Playing);
    playback.setPosition(84000);
    playback.setState(Oran7PlaybackState::Paused);
    check(restored.latest().value("positionMs").toLongLong()==84000,"paused playback captures its actual position");
    playback.setState(Oran7PlaybackState::Idle);
    playback.setPosition(0);
    restored.flush();
    check(restored.latest().value("positionMs").toLongLong()==84000,"stopping cannot erase progress with a zero notification");
    playback.beginOpening(movie);
    restored.beginLocalPlayback(movie);
    playback.setState(Oran7PlaybackState::Playing);
    check(restores==0,"resume waits for duration and ready state");
    playback.setDuration(300000);
    check(restores==1 && playback.positionMs()==84000,"saved position restores once when duration arrives late");
    playback.setState(Oran7PlaybackState::Paused);
    playback.setState(Oran7PlaybackState::Playing);
    check(restores==1,"ordinary pause and resume do not repeat the history seek");
    playback.setState(Oran7PlaybackState::Idle);
    playback.beginOpening(movie);
    restored.beginLocalPlayback(movie);
    restored.cancelResume();
    playback.setDuration(300000);
    playback.setState(Oran7PlaybackState::Playing);
    playback.setPosition(42000);
    check(restores==1,"manual seek intent cancels pending automatic resume");
    playback.setState(Oran7PlaybackState::Ended);
    check(restored.latest().value("completed").toBool() && restored.latest().value("positionMs").toLongLong()==300000,
          "finished media records completion");
    playback.beginOpening(movie);
    restored.beginLocalPlayback(movie);
    playback.setDuration(300000);
    playback.setState(Oran7PlaybackState::Playing);
    check(restores==1 && playback.positionMs()==0,"completed videos replay from the beginning");
    playback.setPosition(75000);
    playback.beginOpening("C:/Videos/other.mp4");
    restored.beginLocalPlayback("C:/Videos/other.mp4");
    playback.setDuration(200000);
    playback.setState(Oran7PlaybackState::Playing);
    playback.setPosition(12000);
    check(restored.flush(),"progress flushes across media switches");
    Oran7AppConfigManager freshConfig(path,defaults);
    Oran7VideoHistory fresh(freshConfig,dir.filePath("thumbnails"));
    check(fresh.latest().value("target")=="C:/Videos/other.mp4" &&
              fresh.latest().value("positionMs").toLongLong()==12000,"new service restores latest video progress");
    bool previousPreserved=false;
    for (int i=0;i<fresh.rowCount();++i) {
        if (fresh.data(fresh.index(i,0),Oran7VideoHistory::Target)==movie)
            previousPreserved=fresh.data(fresh.index(i,0),Oran7VideoHistory::PositionMs).toLongLong()==75000;
    }
    check(previousPreserved,"switching media preserves each record independently");
    std::puts("PASS: durable local and live video history");
}
