#include "Oran7AppConfigManager.h"
#include "Oran7ConfigKeys.h"
#include "Oran7ConfigBinding.h"
#include "Oran7MediaInfoReader.h"
#include "Oran7NetworkSongResolver.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <cstdio>
#include <cstdlib>
#include <limits>

QMap<QString,QVariant> Oran7MediaInfoReader::analyzeMediaFileInfo(QString) { return {{"success",-1}}; }
bool Oran7NetworkSongResolver::isNetworkUrl(const QString& path) { return path.startsWith("https://") || path.startsWith("http://"); }
static int checks=0;
static void check(bool ok,const char* message) { if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}++checks; }
static QByteArray bytes(const QString& path) { QFile f(path);check(f.open(QIODevice::ReadOnly),"fixture read");return f.readAll(); }
static void write(const QString& path,const QByteArray& data) { QFile f(path);check(f.open(QIODevice::WriteOnly)&&f.write(data)==data.size(),"fixture write"); }
static QJsonObject json(const QString& path) { return QJsonDocument::fromJson(bytes(path)).object(); }
static QStringList backups(const QString& path) { return QDir(QFileInfo(path).absolutePath()).entryList({QFileInfo(path).fileName()+".bak-*"},QDir::Files); }
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);
    const auto defaults=json(QStringLiteral(APP_ROOT)+"/config/default.json");
    QTemporaryDir directory;check(directory.isValid(),"temporary directory");
    const auto path=directory.filePath("config.json");
    QJsonObject normalized;QString error;
    check(Oran7AppConfig::normalize({},defaults,&normalized,&error)&&error.isEmpty(),"unversioned empty document migration");
    check(normalized==defaults,"all defaults come from single default.json");
    check(Oran7AppConfig::fromJson(normalized).toJson()==defaults,"typed roundtrip default schema");
    auto recording=Oran7AppConfig::fromJson(normalized);recording.recording.mode=1;recording.recording.whipUrl="https://example.test/whip";recording.recording.bearerToken="fixture-token";recording.recording.width=1280;recording.recording.height=720;
    check(Oran7AppConfig::normalize(recording.toJson(),defaults,&normalized,&error)&&Oran7AppConfig::fromJson(normalized).recording.bearerToken=="fixture-token"&&Oran7AppConfig::fromJson(normalized).recording.width==1280,"recording schema and credentials roundtrip without changing v2 keys");
    auto legacy=QJsonObject{{"window",QJsonObject{{"width",1440},{"position",QJsonObject{{"x",-1440},{"extra",true}}},{"futureWindow",42}}},
        {"screen_capture",QJsonObject{{"fps",120}}},{"playerVolume",73},{"last_used_file_isvip",true},
        {"netease_auth",QJsonObject{{"MUSIC_U","fixture-secret"}}},{"bilibili_auth",QJsonObject{{"sessdata","fixture-bili"}}},
        {"integratedMusic",QJsonObject{{"list",QJsonArray{QJsonObject{{"music_name","旧行"},{"custom",123}},QJsonObject{{"title","新行"}}}}}},
        {"extension",QJsonObject{{"unknown",QJsonArray{1,2,3}}}},{"user",QJsonObject{{"username","用户"}}}};
    check(Oran7AppConfig::normalize(legacy,defaults,&normalized,&error),"legacy normalize");
    auto typed=Oran7AppConfig::fromJson(normalized);
    check(typed.window.width==1440&&typed.window.height==680&&typed.window.x==-1440&&typed.window.y==200,"partial geometry deep merge and negative monitor position");
    check(typed.capture.fps==120&&typed.capture.drawMouse&&typed.player.volume==73,"preserve valid user settings");
    check(typed.lastMedia.isVip&&typed.auth.netease.musicU=="fixture-secret"&&typed.auth.bilibili.sessdata=="fixture-bili","VIP migration and credential preservation");
    check(typed.toJson()==normalized,"unknown root/nested keys and opaque list rows preserved");
    typed.player.volume=31;typed.capture={3,144,false,false};typed.auth.netease={"new-U","new-csrf","昵称"};
    typed.library.playOrder={"b","a"};typed.lastMedia.songId=12345678901LL;
    auto roundtrip=Oran7AppConfig::fromJson(typed.toJson());
    check(roundtrip.player.volume==31&&roundtrip.capture.fps==144&&!roundtrip.capture.drawMouse,"typed groups serialize");
    check(roundtrip.auth.netease.csrf=="new-csrf"&&roundtrip.lastMedia.songId==12345678901LL&&roundtrip.library.playOrder==QVariantList({"b","a"}),"auth order and 64bit song ID roundtrip");
    check(typed.toJson().value("extension")==legacy.value("extension"),"unknown extension survives updates");
    auto largeId=defaults;largeId["last_media_songid"]=std::numeric_limits<qint64>::max();QJsonObject largeNormalized;
    check(Oran7AppConfig::normalize(largeId,defaults,&largeNormalized,&error)&&Oran7AppConfig::fromJson(largeNormalized).lastMedia.songId==std::numeric_limits<qint64>::max(),"native JSON preserves full signed 64bit song ID");
    QJsonObject second;
    check(Oran7AppConfig::normalize(normalized,defaults,&second,&error)&&second==normalized,"migration is idempotent");
    auto versionOne=legacy;versionOne["schemaVersion"]=1;
    check(Oran7AppConfig::normalize(versionOne,defaults,&second,&error)&&second.value("schemaVersion").toInt()==2,"v1 upgrades to v2");
    for(const QJsonValue bad:{QJsonValue(-1),QJsonValue(2.5),QJsonValue("2"),QJsonValue(QJsonValue::Null),QJsonValue(99)}) {
        check(!Oran7AppConfig::normalize({{"schemaVersion",bad}},defaults,&second,&error)&&!error.isEmpty(),"invalid/future versions refused");
    }
    auto malformed=QJsonObject{{"window",QJsonObject{{"width",0},{"height","900"},{"position",QJsonObject{{"x",-200}}}}},
        {"screen_capture",QJsonObject{{"fps",241},{"output_index",-1},{"draw_mouse","false"}}},
        {"appearance",QJsonObject{{"themeProfile",""}}},{"playerVolume",101},{"recent_files_max_count",0},
        {"recent_files",QJsonArray{"a",42,"b"}},{"netease_auth",QJsonObject{{"MUSIC_U",123}}}};
    check(Oran7AppConfig::normalize(malformed,defaults,&second,&error),"invalid leaves repaired");
    auto repaired=Oran7AppConfig::fromJson(second);
    check(repaired.window.width==1180&&repaired.window.height==680&&repaired.window.x==-200,"geometry repair only invalid fields");
    check(repaired.capture.fps==60&&repaired.capture.outputIndex==0&&repaired.capture.drawMouse,"capture type and range repair");
    check(repaired.player.volume==25&&repaired.recentFilesMaxCount==10&&repaired.recentFiles==QStringList({"a","b"}),"volume and recent files schema");
    check(repaired.appearance.themeProfile=="profile-default"&&repaired.auth.netease.musicU.isEmpty(),"invalid profile/auth types use defaults");
    check(Oran7AppConfig::normalize({{"window",QJsonArray{1}},{"screen_capture",false}},defaults,&second,&error)&&second.value("window")==defaults.value("window"),"invalid group types replaced");
    {
        Oran7AppConfigManager config(path,defaults);
        check(config.loadConfig()&&config.isWritable()&&json(path)==defaults,"first run creates normalized config atomically");
        auto value=config.snapshot();value.player.volume=89;config.updateConfig(value);
        check(config.loadConfig()&&config.snapshot().player.volume==89&&json(path).value("playerVolume")==25,"repeated load preserves unsaved edits");
        Oran7ConfigBinding binding(config);
        check(!binding.roamFilterLiked(),"legacy config defaults to unfiltered roam");
        check(binding.saveRoamFilterLiked(true) && binding.roamFilterLiked()
                  && json(path).value("player").toObject().value("roam_filter_liked").toBool()
                  && json(path).value("playerVolume")==89,"roam preference saves independently and preserves pending settings");
        {
            Oran7AppConfigManager reloaded(path,defaults);
            check(reloaded.snapshot().player.roamFilterLiked,"roam preference survives config reload");
        }
        check(binding.saveRoamFilterLiked(false) && !binding.roamFilterLiked(),"roam filter can be disabled persistently");
        binding.saveIntegratedMusic({QVariantMap{{"music_name","保留格式"}}});
        check(json(path).value("playerVolume")==89&&binding.integratedMusic().size()==1,"integrated save preserves pending volume");
        binding.saveLocalOrder({"b","a"});binding.saveLocalOrder({});
        check(binding.localOrder().isEmpty()&&json(path).value("local_music_sort").toObject().value("play_order").toArray().isEmpty(),"empty local order persisted");
        Oran7Track song{"https://example.test/stable","标题","歌手","专辑","封面","123",12345678901LL,true};
        binding.rememberTrack(song);binding.rememberMusicSource(song.path,true);
        auto restored=binding.restoredTrack();
        check(restored&&restored->songId==song.songId&&restored->title==song.title&&restored->isVip,"real typed binding network track restoration");
        auto update=config.snapshot();update.lastMedia.songId=2;config.updateConfig(update);song.album="旧补全";binding.completePersistedDetails(song);
        check(config.snapshot().lastMedia.album=="专辑","stale metadata cannot overwrite current track");
        config.setMaxRecentFiles(2);config.addRecentFile("a");config.addRecentFile("b");config.addRecentFile("c");
        check(config.getRecentFiles()==QStringList({"c","b"}),"recent file limit maintained");
        config.setValueQVariant(Oran7ConfigKeys::Volume,-20);
        check(config.snapshot().player.volume==25,"compatibility setter uses schema validation");
        check(config.saveConfig()&&backups(path).isEmpty(),"new file needs no backup during initial session");
        QQmlApplicationEngine engine;
        engine.loadData("import QtQml\nQtObject { objectName: \"__Oran7StartupWindow__\"; property int width:460; property int height:300 }", QUrl("qrc:/SplashFixture.qml"));
        engine.loadData("import QtQml\nQtObject { objectName: \"__Oran7Window__\"; property int width:0; property int height:0; property int x:0; property int y:0; property rect normalWindowGeometry: Qt.rect(-1333,-40,1333,777) }",QUrl("qrc:/GeometryFixture.qml"));
        check(engine.rootObjects().size()==2,"startup and named main geometry fixtures");
        auto geometry=config.snapshot();geometry.window={1500,900,-1600,-200,false};config.updateConfig(geometry);
        binding.loadWindowSize(engine);binding.loadWindowPosition(engine);
        check(engine.rootObjects().last()->property("x").toInt()==-1600&&engine.rootObjects().last()->property("height").toInt()==900,"binding restores negative multi-monitor coordinates");
        check(engine.rootObjects().first()->property("width").toInt()==460,"startup window never receives main geometry");
        const auto singleton=directory.filePath("GeometrySettings.qml");
        write(singleton,"pragma Singleton\nimport QtQml\nQtObject { property int savedNormalWidth:1333; property int savedNormalHeight:777; property int savedNormalX:-1333; property int savedNormalY:-40 }");
        qmlRegisterSingletonType(QUrl::fromLocalFile(singleton),"Oran7MainUiSetting",1,0,"Oran7MainUiSetting");
        binding.saveWindowSize(engine);binding.saveWindowPosition(engine);
        check(config.snapshot().window.x==-1333&&config.snapshot().window.y==-40&&config.snapshot().window.width==1333,"binding saves negative coordinates and normal geometry");
        delete engine.rootObjects().last();
        QQmlComponent component(&engine);
        component.setData("import QtQml\nQtObject { objectName: \"__Oran7Window__\"; property int width:0; property int height:0; property int x:0; property int y:0 }", QUrl("qrc:/AdoptedGeometry.qml"));
        auto* adopted = component.create();
        check(adopted, "create engine-adopted geometry fixture");
        adopted->setParent(&engine);
        binding.loadWindowSize(engine); binding.loadWindowPosition(engine);
        check(adopted->property("x").toInt()==-1333 && adopted->property("width").toInt()==1333,
              "binding restores geometry to an incubated root owned by engine parent");
        delete adopted;
        QQmlComponent windowComponent(&engine);
        const auto fixture = QByteArray("import QtQuick\nimport QtQuick.Window\nimport \"")
            + QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/src/qml/Components/Oran7GlobalComponents").toEncoded() + "\"\n"
              "Window { id: win; objectName: \"__Oran7Window__\"; width:1333; height:777; visible:true;"
              "property bool startupReleased:false; readonly property alias normalWindowGeometry: geometry.normalGeometry;"
              "function captureNormalGeometry() { geometry.capture() }"
              "function prepareStartupGeometry() { geometry.initialize() }"
              "Oran7WindowGeometryState { id: geometry; targetWindow: win; active: win.startupReleased } }";
        windowComponent.setData(fixture, QUrl("qrc:/WindowGeometryFixture.qml"));
        auto* window = qobject_cast<QQuickWindow*>(windowComponent.create());
        check(window, "production window geometry state loads in isolated software window");
        window->QObject::setParent(&engine);
        QMetaObject::invokeMethod(window, "prepareStartupGeometry");
        window->resize(1333, 820);
        QTest::qWait(230);
        check(window->property("normalWindowGeometry").toRectF().height()==777, "startup geometry changes cannot overwrite normal snapshot");
        window->setProperty("startupReleased",true);
        window->resize(1333, 812);
        binding.saveWindowSize(engine); binding.saveWindowPosition(engine);
        check(config.snapshot().window.height==812, "save flushes height immediately instead of reading stale registered singleton");
        check(config.saveConfig() && json(path).value("window").toObject().value("height").toInt()==812,
              "latest height reaches temporary persistent config before debounce expires");
        window->resize(1333, 850);
        QTest::qWait(230);
        check(window->property("normalWindowGeometry").toRectF().height()==850, "settled ordinary resize updates normal geometry");
        window->showMaximized(); QTest::qWait(20);
        window->resize(1900, 1100);
        binding.saveWindowSize(engine);
        check(config.snapshot().window.height==850, "maximized dimensions do not overwrite ordinary height");
        window->showNormal(); QTest::qWait(20);
        window->resize(1333, 823);
        window->close();
        binding.saveWindowSize(engine);
        check(config.snapshot().window.height==823, "closing captures the final resize before window becomes hidden");
    }
    write(path,QJsonDocument(legacy).toJson());const auto original=bytes(path);
    {
        Oran7AppConfigManager config(path,defaults);
        check(config.loadConfig()&&!bytes(path).contains("fixture-secret")&&backups(path).size()==1,"P2-3 credential migration commits verified ciphertext and preserves original backup");
        auto saved=config.snapshot();saved.player.volume=92;config.updateConfig(saved);
        check(config.saveConfig()&&backups(path).size()==1,"first overwrite backs up original");
        check(bytes(directory.filePath(backups(path).first()))==original,"backup exact original bytes");
        check(json(path).value("schemaVersion").toInt()==2&&config.snapshot().auth.netease.musicU=="fixture-secret","migration version and login survive save");
        saved=config.snapshot();saved.player.volume=93;config.updateConfig(saved);check(config.saveConfig()&&backups(path).size()==1,"one backup per loaded session");
    }
    {
        const auto corruptPath=directory.filePath("corrupt.json");write(corruptPath,"{broken json");
        Oran7AppConfigManager config(corruptPath,defaults);
        check(!config.loadConfig()&&config.snapshot().window.width==1180&&bytes(corruptPath)=="{broken json","corrupt file defaults without overwrite");
        check(config.saveConfig()&&backups(corruptPath).size()==1&&bytes(directory.filePath(backups(corruptPath).first()))=="{broken json","corrupt bytes backed up before recovery");
        check(config.loadConfig(),"successful recovery updates load status");
    }
    for(const QByteArray raw:{QByteArray("{\"schemaVersion\":99,\"newField\":42}"),QByteArray("{\"schemaVersion\":\"2\"}")}) {
        const auto futurePath=directory.filePath("future.json");write(futurePath,raw);
        Oran7AppConfigManager config(futurePath,defaults);
        check(!config.loadConfig()&&!config.isWritable(),"future/malformed version storage read-only");
        auto saved=config.snapshot();saved.player.volume=8;config.updateConfig(saved);config.resetToDefaults();
        check(!config.saveConfig()&&bytes(futurePath)==raw&&backups(futurePath).isEmpty()&&!config.lastError().isEmpty(),"future config never overwritten by updates or reset and error retained");
    }
    {
        const auto target=directory.filePath("commit-failure.json");Oran7AppConfigManager config(target,defaults);
        QFile::remove(target);QDir().mkdir(target);auto saved=config.snapshot();saved.player.volume=66;config.updateConfig(saved);
        check(!config.saveConfig()&&config.snapshot().player.volume==66,"failed commit retains pending state");
        QDir().rmdir(target);check(config.saveConfig()&&json(target).value("playerVolume")==66,"failed save can retry without lost edits");
    }
    {
        const auto target=directory.filePath("backup-failure.json");write(target,"{}");
        Oran7AppConfigManager config(target,defaults);QFile::remove(target);
        check(!config.saveConfig()&&!QFile::exists(target)&&!config.lastError().isEmpty(),"backup failure prevents replacement");
        write(target,"{}");check(config.saveConfig()&&backups(target).size()==1,"backup failure can retry");
    }
    {
        const auto badPath=directory.filePath("bad-default.json");Oran7AppConfigManager config(badPath,{});
        check(!config.isWritable()&&!config.saveConfig()&&!QFile::exists(badPath),"missing defaults cannot create invalid user file");
        Oran7AppConfigManager partial(badPath,{{"schemaVersion",2}});
        check(!partial.isWritable()&&!partial.saveConfig()&&!QFile::exists(badPath),"partial defaults cannot create zero-sized configuration");
        auto invalidDefaults=defaults;invalidDefaults["recent_files_max_count"]=-3;
        Oran7AppConfigManager invalid(badPath,invalidDefaults);invalid.addRecentFile("ignored");
        check(!invalid.isWritable()&&invalid.getRecentFiles().isEmpty(),"invalid defaults cannot enter recent-file trimming");
    }
    std::printf("PASS: %d config schema/migration/storage/binding checks\n",checks);
}
