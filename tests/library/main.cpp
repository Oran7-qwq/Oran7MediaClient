#include "Oran7LocalLibraryService.h"
#include "Oran7TrackLibrary.h"
#include "Oran7MediaInfoReader.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QSemaphore>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <cstdio>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
template<class F>static void until(F test){QElapsedTimer t;t.start();while(!test()&&t.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(1);}check(test(),"wait timed out");}
static void write(const QString& path,const QByteArray& data="dummy"){QFile f(path);check(f.open(QIODevice::WriteOnly),"fixture file");f.write(data);}
static QByteArray read(const QString& path){QFile f(path);f.open(QIODevice::ReadOnly);return f.readAll();}
struct Oran7Fixture {
 QSemaphore entered,gate;
 std::atomic_bool block{false};std::atomic_int active{0},maxActive{0},calls{0};
 QString throwing;
};
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);using Service=Oran7LocalLibraryService;
 QTemporaryDir root,other,source;check(root.isValid()&&other.isValid()&&source.isValid(),"temporary roots");
 auto state=std::make_shared<Oran7Fixture>();
 const auto reader=[state](const QString& path)->QMap<QString,QVariant>{
  check(QThread::currentThread()!=QCoreApplication::instance()->thread(),"metadata runs outside GUI");
  const int active=++state->active;state->maxActive.store(qMax(state->maxActive.load(),active));++state->calls;
  if(state->block.exchange(false)){state->entered.release();state->gate.acquire();}
  --state->active;
  if(QFileInfo(path).fileName()==state->throwing)throw std::runtime_error("mock metadata failure");
  if(path.endsWith(".txt"))return {{"success",1}};
  return {{"success",0},{"title",QFileInfo(path).completeBaseName()},{"artist","artist"},{"album","album"},{"duration","12"},{"cover",QFileInfo(path).absolutePath()+"/audioCover/"+QFileInfo(path).completeBaseName()+".jpg"}};
 };
 QStringList trashed;
 const auto mover=[&trashed](const QString& path){trashed.append(path);return !path.endsWith("fail.mp3")&&QFile::remove(path);};
 Oran7TrackLibrary models;
 Service service(reader,mover);
 QObject::connect(&service,&Service::tracksReady,&models,&Oran7TrackLibrary::replaceLocalTracks);
 QObject::connect(models.local(),&Oran7TrackListModel::contentsChanged,[&]{service.setPlayOrder(models.local()->paths());});int clears=0,errors=0;QStringList published;

 QObject::connect(&service,&Service::operationFailed,[&](const QString&){++errors;});
 QObject::connect(&service,&Service::tracksReady,[&](const QVariantList& tracks){
  check(QThread::currentThread()==app.thread(),"publication on GUI"); ++clears; published.clear();
  for(const auto& track:tracks) published.append(track.toMap().value("path").toString());
 });
 const auto a=root.filePath("a.mp3"),b=root.filePath("b.flac"),c=root.filePath("c.ogg");write(a);write(b);write(c);write(root.filePath("skip.txt"));
 service.setPlayOrder({b,a});service.scan(root.path());until([&]{return !service.isBusy();});
 check(published==QStringList({c,b,a})&&service.files().size()==3,"new first and saved order applied to published rows");
 check(service.playOrder()==QVariantList({c,b,a}),"state order matches UI order");
 check(models.local()->count()==3 && models.local()->get(0).value("path")==c,"real model receives library batch snapshot");
 models.local()->moveTrack(2,0);
 check(service.playOrder()==QVariantList({a,c,b}),"native model reorder updates library order without QML roundtrip");
 QFile::remove(c);service.scan(root.path());until([&]{return !service.isBusy();});check(service.files().size()==2&&!published.contains(c),"refresh removes stale files and duplicates");
 state->block=true;service.scan(root.path());check(state->entered.tryAcquire(1,5000),"blocked scan entered");models.local()->moveTrack(0,1);state->gate.release();until([&]{return !service.isBusy();});check(published==QStringList({b,a}),"model reorder during scan preserved");
 const auto latest=other.filePath("latest.mp3");write(latest);
 const int beforeClears=clears;state->block=true;service.scan(root.path());check(state->entered.tryAcquire(1,5000),"old scan entered");
 service.scan(source.path());service.scan(other.path());state->gate.release();until([&]{return !service.isBusy();});
 check(clears==beforeClears+1&&published==QStringList({latest}),"obsolete scans ignored and queued scans coalesced");
 const auto imported=source.filePath(QString::fromUtf8("new #中文.mp3"));write(imported,"original");
 service.importFiles(root.path(),{QUrl::fromLocalFile(imported).toString()});service.importFiles(root.path(),{imported});until([&]{return !service.isBusy();});
 const auto destination=root.filePath(QFileInfo(imported).fileName());check(read(destination)=="original"&&service.files().size()==3,"serialized import handles file URL and duplicates");
 write(imported,"replacement");service.importFiles(root.path(),{imported});until([&]{return !service.isBusy();});check(read(destination)=="original","duplicate import never overwrites");
 QDir().mkpath(root.filePath("audioCover"));write(root.filePath("audioCover/a.jpg"));write(root.filePath("fail.mp3"));write(root.filePath("audioCover/fail.jpg"));
 service.removeFiles(root.path(),{a,root.filePath("fail.mp3"),latest,root.path()});until([&]{return !service.isBusy();});
 check(!QFile::exists(a)&&!QFile::exists(root.filePath("audioCover/a.jpg")),"successful removal cleans cover");
 check(QFile::exists(root.filePath("fail.mp3"))&&QFile::exists(root.filePath("audioCover/fail.jpg")),"failed trash keeps media and cover");
 check(QFile::exists(latest)&&!trashed.contains(latest)&&errors>=3,"outside root and directory deletion rejected");
 check(published.contains(root.filePath("fail.mp3"))&&!published.contains(a),"failed deletion remains visible after refresh");
 state->throwing="bad.mp3";write(root.filePath("bad.mp3"));service.scan(root.path());until([&]{return !service.isBusy();});check(!published.contains(root.filePath("bad.mp3")),"bad metadata does not block queue");
 service.scan(other.path());until([&]{return !service.isBusy();});check(published==QStringList({latest})&&state->maxActive==1,"serial work progresses after exceptions");
 auto retiring=std::make_unique<Service>(reader,mover);int callbacks=0;QObject::connect(retiring.get(),&Service::tracksReady,[&]{++callbacks;});
 state->block=true;retiring->scan(root.path());check(state->entered.tryAcquire(1,5000),"lifecycle scan entered");retiring.reset();state->gate.release();Oran7Async::waitForDone();QCoreApplication::processEvents();check(callbacks==0,"destroyed service has no callbacks or worker object access");
 state->block=true;service.scan(root.path());check(state->entered.tryAcquire(1,5000),"shutdown scan entered");const auto prior=published;service.shutdown();state->gate.release();Oran7Async::waitForDone();QCoreApplication::processEvents();check(!service.isBusy()&&published==prior,"shutdown invalidates pending publication");
 const auto parsed=Oran7MediaInfoReader::extractMediaInfo(QJsonDocument::fromJson(R"({"format":{"duration":"12.9","tags":{"TITLE":"T","ARTIST":"A","ALBUM":"B"}}})").object());
 check(parsed.value("title")=="T"&&parsed.value("artist")=="A"&&parsed.value("album")=="B"&&parsed.value("duration")=="12","real ffprobe parser retains tag casing and duration behavior");
 {
  Oran7TrackLibrary trackModels;
  QQmlEngine engine;
  engine.rootContext()->setContextProperty("trackModels", &trackModels);
  engine.rootContext()->setContextProperty("BasicConfig",QVariantMap{{"globalPlayingFocus",0},{"globalPlayer_MusicPlayerIndex",0}});
  engine.rootContext()->setContextProperty("Client",QVariantMap{{"playback",QVariantMap{{"active",true},{"currentTrack",QVariantMap{{"path","b.mp3"},{"songId",0}}}}}});
  QQmlComponent component(&engine,QUrl::fromLocalFile(QDir::current().absoluteFilePath("tests/library/Focus.qml")));
  std::unique_ptr<QObject> focus(component.create());
  if(!focus){std::fprintf(stderr,"%s\n",qPrintable(component.errorString()));return 1;}
  trackModels.replaceLocalTracks({QVariantMap{{"path","a.mp3"}},QVariantMap{{"path","b.mp3"}}});QCoreApplication::processEvents();
  check(focus->property("playing")==1&&focus->property("selecting")==1,"initial model locates current song");
  trackModels.replaceLocalTracks({QVariantMap{{"path","new.mp3"}},QVariantMap{{"path","a.mp3"}},QVariantMap{{"path","b.mp3"}}});QCoreApplication::processEvents();
  check(focus->property("playing")==2&&focus->property("selecting")==2,"batch refresh restores focus after inserting rows before current song");
 }
 std::puts("PASS: library snapshot/ordering, latest scan, serialized import/delete, path boundary/failure retention, GUI publication, exceptions, shutdown and destruction; real metadata parser");
}
