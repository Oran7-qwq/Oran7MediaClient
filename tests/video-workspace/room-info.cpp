#include "Oran7VideoHistory.h"
#include "Oran7AppConfigManager.h"
#include "FakeRoomNetwork.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QTest>
#include <cstdio>
#include <cstdlib>

static void check(bool ok,const char* message)
{
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static void waitFor(const std::function<bool()>& ready)
{
    QElapsedTimer timer; timer.start();
    while (!ready() && timer.elapsed()<3000) QTest::qWait(10);
    check(ready(),"asynchronous room metadata finishes");
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    QTemporaryDir dir;
    QFile defaultsFile(QStringLiteral(APP_ROOT)+"/config/default.json");
    check(defaultsFile.open(QIODevice::ReadOnly),"read isolated defaults");
    const auto defaults=QJsonDocument::fromJson(defaultsFile.readAll()).object();
    Oran7AppConfigManager config(dir.filePath("config.json"),defaults);
    auto network=std::make_unique<Oran7FakeRoomNetwork>();
    auto* fake=network.get();
    Oran7VideoHistory model(config,dir.filePath("thumbs"),{},std::move(network));
    model.rememberRoom("6");
    model.requestRoomInfo("6");
    model.requestRoomInfo("6");
    waitFor([&] { return model.data(model.index(0,0),Oran7VideoHistory::RoomInfoState).toInt()==2; });
    check(fake->calls==2,"overlapping room requests share one two-step query");
    check(model.latest().value("anchorName")=="Fixture anchor" &&
              model.latest().value("title")=="Fixture live title","anchor and title enrich the existing history row");
    check(model.data(model.index(0,0),Oran7VideoHistory::LiveStatus)==1,"fresh room metadata exposes live status");
    model.rememberRoom("6");
    check(model.latest().value("title")=="Fixture live title","revisiting a room preserves metadata");
    model.requestRoomInfo("6");
    check(fake->calls==2,"successful metadata is throttled");
    check(model.flush(),"room metadata saves with viewing history");
    {
        Oran7AppConfigManager freshConfig(dir.filePath("config.json"),defaults);
        Oran7VideoHistory restored(freshConfig,dir.filePath("thumbs"));
        check(restored.latest().value("anchorName")=="Fixture anchor","cached room information survives service reconstruction");
        check(restored.data(restored.index(0,0),Oran7VideoHistory::LiveStatus)==-1,"cached status is unknown until refreshed");
    }
    fake->fail=true;
    model.requestRoomInfo("6",true);
    waitFor([&] { return model.data(model.index(0,0),Oran7VideoHistory::RoomInfoState).toInt()==3; });
    check(model.latest().value("title")=="Fixture live title" &&
              model.data(model.index(0,0),Oran7VideoHistory::LiveStatus)==-1,"failed refresh retains cached content and clears live certainty");
    fake->fail=false;
    model.requestRoomInfo("6",true);
    model.remove("live","6");
    QTest::qWait(50);
    check(model.liveCount()==0,"late room metadata cannot resurrect a removed record");
    for (int i=1;i<=6;++i) { model.rememberRoom(QString::number(i)); model.requestRoomInfo(QString::number(i),true); }
    QTest::qWait(100);
    check(fake->maximumActive<=2,"room metadata network concurrency stays bounded");
    model.shutdown();
    std::puts("PASS: room metadata persistence, throttling, cancellation and stale state");
}
