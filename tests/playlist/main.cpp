#include "Oran7CloudMusicService.h"
#include "Oran7PlaybackViewModel.h"
#include "Oran7TrackLibrary.h"
#include "Oran7PlaybackState.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QTest>
#include <QSemaphore>
#include <cstdio>
#include <cstdlib>

static int count = 0;
static void check(bool ok, const char* name) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",name); std::fflush(stderr); std::_Exit(1); } ++count; }
template<class F> static void until(F condition) { QElapsedTimer t; t.start(); while(!condition() && t.elapsed()<4000) QTest::qWait(5); check(condition(),"async completion"); }
static QVariantMap song(qint64 id) { return {{"songId",id},{"title",QString::number(id)},{"path",QString()},{"timesize","123"}}; }
static void checkRoamFiltering() {
    {
        Oran7TrackLibrary large; Oran7PlaybackState state;
        QVariantList likes;
        for (int id=1; id<=10000; ++id) likes.append(song(id));
        auto batchGate=std::make_shared<QSemaphore>();
        auto batchMode=std::make_shared<std::atomic_int>(0);
        auto batches=std::make_shared<std::atomic_int>(0);
        Oran7CloudMusicService service(large, {
            [] { return QVariantMap{{"isLoggedIn",true},{"cookie","fixture-large"}}; },
            [likes,batchGate,batchMode,batches](const QString& operation,const QVariantMap&) -> QVariant {
                if (operation=="fetchLikedSongs") return QVariantMap{{"playlistId",88},{"songs",likes}};
                ++*batches; if (!batchGate->tryAcquire(1, 1500)) return {};
                if (*batchMode==1) return QVariantList{song(1),song(10000)};
                if (*batchMode==2) return QVariantList{song(1),song(qint64(1)<<40),song(10000)};
                return QVariantList{song(50001),song(1),song(50002),song(50003)};
            }, [](qint64){}, [](qint64,bool){} });
        Oran7PlaybackViewModel roam(large,state,service);
        QVariantMap current; int emitted=0,ended=0;
        QObject::connect(&roam,&Oran7PlaybackViewModel::trackRequested,[&](const QVariantMap& fields,bool) {
            current=fields; ++emitted; state.selectTrack(*Oran7Track::fromVariantMap(fields));
        });
        QObject::connect(&roam,&Oran7PlaybackViewModel::stopRequested,[&]{++ended;});
        check(service.fetchLiked(),"large liked snapshot requested");
        until([&]{return service.likedLoaded();});
        check(large.liked()->count()==10000 && large.isLiked(10000) && !large.isLiked(qint64(1)<<40),
              "large liked model exposes hashed 64-bit membership without a duplicate cache");
        check(!roam.roamFilterLiked(),"default strategy preserves existing recommendation behavior");
        large.replaceSearchResults({song(1)}); roam.activate(large.search(),0); state.setMode(3);
        roam.advance(1); batchGate->release(); until([&]{return current.value("songId").toLongLong()==50001;});
        const int playing=emitted;
        roam.setRoamFilterLiked(true);
        large.addLikedTrack(song(50001));
        check(emitted==playing,"enabling filter and liking current song never interrupts playback");
        roam.advance(1);
        check(current.value("songId").toLongLong()==50002,"strategy removes liked songs already in pending cache");
        large.addLikedTrack(song(50003));
        *batchMode=1;
        roam.advance(1); batchGate->release(3);
        until([&]{return ended==1;});
        check(*batches==4 && current.value("songId").toLongLong()==50002,
              "liked changes prune pending cache and three fully rejected batches stop bounded refill");
        const int exhausted=batches->load(); roam.advance(1); QTest::qWait(30);
        check(*batches==exhausted,"exhausted filter cannot create an unbounded recommendation retry loop");
        roam.setRoamFilterLiked(false); *batchMode=0;
        roam.advance(1); batchGate->release(); until([&]{return emitted>playing+1;});
        check(current.value("songId").toLongLong()==50001,"pass-through strategy accepts liked songs after disabling");
        roam.cancel(); roam.setRoamFilterLiked(true); *batchMode=2;
        // 取消是协作式；同时释放仍可能运行的旧预取与新请求，旧结果必须被代际丢弃。
        roam.advance(1); batchGate->release(2);
        until([&]{return current.value("songId").toLongLong()==(qint64(1)<<40);});
        check(!large.isLiked(current.value("songId").toLongLong()),"new batches are filtered while preserving 64-bit song IDs");
        roam.shutdown(); service.shutdown(); batchGate->release(10); Oran7Async::waitForDone();
    }
}
static void checkRoamLikedReadiness() {
    for (const bool fail : {false, true}) {
        Oran7TrackLibrary tracks; Oran7PlaybackState state;
        auto gate = std::make_shared<QSemaphore>();
        auto requests = std::make_shared<std::atomic_int>(0);
        Oran7CloudMusicService service(tracks, {
            [] { return QVariantMap{{"isLoggedIn",true},{"cookie","fixture-loading"}}; },
            [gate,requests,fail](const QString& operation,const QVariantMap&) -> QVariant {
                if (operation=="fetchLikedSongs") {
                    if (!gate->tryAcquire(1,1500) || fail) return {};
                    return QVariantMap{{"playlistId",88},{"songs",QVariantList{song(1)}}};
                }
                ++*requests; return QVariantList{song(1),song(2)};
            }, [](qint64){}, [](qint64,bool){} });
        check(service.fetchLiked(),"liked preload accepted before roam");
        Oran7PlaybackViewModel queue(tracks,state,service);
        int stops=0; qint64 played=0;
        QObject::connect(&queue,&Oran7PlaybackViewModel::stopRequested,[&]{++stops;});
        QObject::connect(&queue,&Oran7PlaybackViewModel::trackRequested,[&](const QVariantMap& fields,bool){
            played=fields.value("songId").toLongLong(); state.selectTrack(*Oran7Track::fromVariantMap(fields));
        });
        queue.setRoamFilterLiked(true); tracks.replaceSearchResults({song(3)});
        queue.activate(tracks.search(),0); state.setMode(3); queue.advance(1);
        check(*requests==0 && played==3 && stops==0,"filtered roam awaits liked snapshot without stopping current song");
        gate->release();
        if (fail) {
            until([&]{return stops==1;});
            check(*requests==0 && played==3,"failed liked preload never treats unknown membership as an empty liked list");
        } else {
            until([&]{return played==2;});
            check(*requests==1,"liked completion resumes one filtered recommendation request");
        }
        queue.shutdown(); service.shutdown(); gate->release(); Oran7Async::waitForDone();
    }
}
int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    Oran7TrackLibrary tracks; Oran7PlaybackState playback;
    QVariantMap credentials{{"cookie","account-a"},{"csrf","fixture"},{"isLoggedIn",true}};
    int writes=0; qint64 playlist=0;
    auto gate=std::make_shared<QSemaphore>();
    auto requested=std::make_shared<std::atomic_int>(0);
    Oran7CloudMusicService cloud(tracks, {
        [&]{return credentials;},
        [gate,requested](const QString& operation,const QVariantMap&) -> QVariant {
            ++*requested; gate->acquire();
            if(operation=="fetchLikedSongs") return QVariantMap{{"playlistId",44},{"songs",QVariantList{song(1),song(2),song(3)}}};
            return QVariantList{song(11),song(12),song(13)};
        }, [&](qint64 id){playlist=id;}, [&](qint64,bool){++writes;}});
    Oran7PlaybackViewModel queue(tracks,playback,cloud);
    QVariantMap played; int plays=0,stops=0; bool restart=false;
    QObject::connect(&queue,&Oran7PlaybackViewModel::trackRequested,[&](const QVariantMap& fields,bool force){ played=fields; restart=force; ++plays; playback.selectTrack(*Oran7Track::fromVariantMap(fields)); });
    QObject::connect(&queue,&Oran7PlaybackViewModel::stopRequested,[&]{++stops;});
    check(cloud.fetchLiked(),"initial fetch accepted");
    check(!cloud.fetchLiked() && cloud.likedFetching(),"deduplicate concurrent preload/page fetch");
    gate->release(); until([&]{return cloud.likedLoaded();});
    check(playlist==44 && tracks.liked()->count()==3 && !cloud.likedDirty(),"publish liked snapshot");
    check(!cloud.fetchLiked(),"TTL avoids duplicate first page load");
    check(cloud.toggleLike(song(2)) && !tracks.isLiked(2) && writes==1,"optimistic unlike");
    check(!cloud.toggleLike(song(1)) && writes==1,"shared like cooldown");
    check(cloud.fetchLiked(),"dirty fetch");
    cloud.markLikedDirty(); gate->release(); until([&]{return !cloud.likedFetching();});
    check(!tracks.isLiked(2) && cloud.likedDirty(),"inflight snapshot does not overwrite optimistic edit");
    tracks.replaceSearchResults({song(1),song(2),song(3)});
    check(!queue.activate(tracks.search(),-1),"invalid row ignored");
    check(queue.activate(tracks.search(),1) && !restart && played.value("songId")==2 && queue.owner()=="search","single click path");
    tracks.search()->moveTrack(1,0); queue.advance(1);
    check(played.value("songId")==1 && restart,"next uses stable track identity after reorder");
    playback.setMode(1); queue.advance(1);
    check(played.value("songId")==1 && restart,"repeat forces replay");
    playback.setMode(2); queue.advance(-1);
    check(played.value("songId")!=1,"random avoids current track");
    auto interrupt=QObject::connect(&queue,&Oran7PlaybackViewModel::musicActivated,[&]{queue.cancel(true);});
    const int beforeInterrupt=plays;
    check(!queue.activate(tracks.search(),0) && plays==beforeInterrupt,"synchronous navigation cancels superseded click");
    QObject::disconnect(interrupt);
    playback.setMode(0); queue.activate(tracks.search(),0); const auto first=playback.currentTrack().songId;
    const int before=plays; queue.resolveFailed(9999);
    check(plays==before,"stale failure ignored");
    queue.resolveFailed(first); queue.resolveFailed(playback.currentTrack().songId); queue.resolveFailed(playback.currentTrack().songId);
    check(stops==1,"full round of failures stops");
    queue.activate(tracks.search(),0); playback.setMode(3); queue.advance(1);
    const int waitingPlays=plays; gate->release(); until([&]{return plays>waitingPlays;});
    check(played.value("songId")==11 && restart,"roam exhaustion waits then starts batch");
    queue.advance(-1); check(played.value("songId")==11 && restart,"roam head previous replays without timer");
    queue.advance(1); check(played.value("songId")==12,"roam next");
    queue.cancel(true); const auto canceledPlays=plays; gate->release(); QTest::qWait(80);
    check(plays==canceledPlays && queue.owner().isEmpty(),"late prefetch ignored after video/stop");
    queue.advance(1); check(plays==canceledPlays,"video suppresses automatic music adoption");
    playback.setMode(0); playback.selectTrack(*Oran7Track::fromVariantMap(song(1)));
    queue.toggleCurrent(); check(plays==canceledPlays+1 && !restart,"bottom play resumes music intent after video");
    queue.advance(1); check(played.value("songId")==3,"bottom resume restores next-song ownership");
    {
        Oran7PlaybackViewModel restored(tracks,playback,cloud);
        int restoredId=0;
        playback.setMode(0); playback.selectTrack(*Oran7Track::fromVariantMap(song(1)));
        QObject::connect(&restored,&Oran7PlaybackViewModel::trackRequested,[&](const QVariantMap& fields,bool){restoredId=fields.value("songId").toInt();});
        restored.advance(1);
        check(restoredId==3,"restored music adopts model ownership before next");
    }
    check(cloud.fetchLiked(true),"forced fetch accepted");
    credentials.insert("cookie","account-b"); cloud.syncSession();
    gate->release(4); until([&]{return cloud.likedLoaded();});
    check(tracks.liked()->count()==3 && !cloud.likedFetching(),"account switch invalidates old request");
    credentials.insert("isLoggedIn",false); cloud.syncSession();
    check(tracks.liked()->count()==0 && !cloud.likedLoaded() && playlist==0,"logout clears cache and playlist");
    queue.shutdown(); check(!queue.activate(tracks.search(),0),"shutdown refuses queued UI activation");
    cloud.shutdown(); gate->release(8); Oran7Async::waitForDone();
    checkRoamFiltering();
    checkRoamLikedReadiness();
    std::printf("PASS: %d policy checks\n",count);
}
