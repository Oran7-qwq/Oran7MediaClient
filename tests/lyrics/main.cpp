#include "Oran7LyricsService.h"
#include "Oran7OnlineLyricsFetcher.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <cstring>
#include <cstdio>
static void check(bool ok,const char* message) { if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);} }
struct Oran7RequestLog { QList<QPointer<QNetworkReply>> replies; int aborted=0, managers=0, destroyed=0; };
class Oran7Reply : public QNetworkReply {
public:
 Oran7Reply(QObject* parent,const QNetworkRequest& request,Oran7RequestLog& log): QNetworkReply(parent),m_log(log){setRequest(request);setUrl(request.url());open(ReadOnly);}
 void abort() override { if(!isFinished()){++m_log.aborted;complete({},OperationCanceledError);} }
 bool isSequential() const override {return true;}
 qint64 bytesAvailable() const override{return m_data.size()-m_offset+QNetworkReply::bytesAvailable();}
 void complete(QByteArray data,NetworkError error=NoError){m_data=std::move(data);if(error!=NoError)setError(error,"mock");setFinished(true);emit readyRead();emit finished();}
protected:
 qint64 readData(char* data,qint64 max) override {const auto size=qMin(max,qint64(m_data.size()-m_offset));if(size<=0)return -1;std::memcpy(data,m_data.constData()+m_offset,size);m_offset+=size;return size;}
private:
 QByteArray m_data; qint64 m_offset=0; Oran7RequestLog& m_log;
};
class Oran7Network : public QNetworkAccessManager {
public:
 explicit Oran7Network(Oran7RequestLog& log):m_log(log){++m_log.managers;}
 ~Oran7Network() override {++m_log.destroyed;}
protected:
 QNetworkReply* createRequest(Operation,const QNetworkRequest& request,QIODevice*) override {auto* reply=new Oran7Reply(this,request,m_log);m_log.replies.append(reply);return reply;}
private: Oran7RequestLog& m_log;
};
static void events(){QCoreApplication::processEvents();}
static Oran7Reply* last(Oran7RequestLog& log){return static_cast<Oran7Reply*>(log.replies.last().data());}
static void write(const QString& path,const QByteArray& bytes){QFile f(path);check(f.open(QIODevice::WriteOnly),"fixture write");f.write(bytes);}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);QTemporaryDir root;check(root.isValid(),"temporary root");
 Oran7RequestLog log;int metadataReads=0,found=0,missing=0,searching=0;QString result;
 const auto factory=[&]{return std::make_unique<Oran7OnlineLyricsFetcher>(std::make_unique<Oran7Network>(log));};
 auto service=std::make_unique<Oran7LyricsService>([&](const QString&){++metadataReads;return QMap<QString,QVariant>{{"success",0},{"artist","artist"},{"title","title"}};},nullptr,factory);
 QObject::connect(service.get(),&Oran7LyricsService::lyricsAvailable,[&](const QString& path){++found;result=path;});
 QObject::connect(service.get(),&Oran7LyricsService::onlineLyricsNotFound,[&]{++missing;});
 QObject::connect(service.get(),&Oran7LyricsService::onlineLyricsSearching,[&]{++searching;});
 const QString cached=root.filePath("cached.lrc");write(cached,"[00:01]cached");
 service->load("C:/music/cached.mp3",root.path(),{});check(result==cached&&metadataReads==0&&log.managers==0,"local cache bypasses metadata and network");
 service->load("file:///C:/music/download.mp3",root.path(),{});check(metadataReads==1&&searching==1,"local cache miss searches metadata");
 last(log)->complete(R"({"syncedLyrics":"[00:01]download"})");events();
 const QString downloaded=result;check(QFileInfo(downloaded).fileName()=="download.lrc"&&QFile::exists(downloaded),"local download uses persistent cache");
 service->stop();check(QFile::exists(downloaded),"stop preserves local cache");
 Oran7Track track;track.songId=123;track.title="song";track.isVip=true;
 service->load("https://m701.music.126.net/no-id.mp3",root.path(),track);
 check(QUrlQuery(last(log)->url()).queryItemValue("id")=="123"&&last(log)->request().rawHeader("Cookie")=="test-cookie","CDN uses injected identity and auth");
 last(log)->complete(R"({"lrc":{"lyric":"[00:01]network"}})");events();
 const QString network=result;check(QFileInfo(network).fileName()=="song_123.lrc"&&QFile::exists(network),"network file named from songId without URL id");
 service->stop();check(!QFile::exists(network)&&QFile::exists(downloaded),"network cache RAII cleanup");
 service->load("https://cdn/a",root.path(),track);last(log)->complete(R"({"lrc":{"lyric":"[00:01]old"}})");
 const int beforeFound=found;service->load("C:/music/cached.mp3",root.path(),{});events();
 check(found==beforeFound+1&&result==cached,"queued old success cannot replace local lyrics");
 service->load("https://cdn/a",root.path(),track);last(log)->complete("{}");
 const int beforeMissing=missing;service->load("C:/music/cached.mp3",root.path(),{});events();check(missing==beforeMissing,"queued old failure suppressed");
 service->load("https://cdn/a",root.path(),track);QPointer<QNetworkReply> retired=last(log);
 const int beforeAbort=log.aborted;service->load("https://cdn/b",root.path(),track);check(!retired&&log.aborted==beforeAbort+1,"in-flight search canceled and destroyed");
 last(log)->complete(R"({"lrc":{"lyric":"[00:01]fresh"}})");events();check(result!=network&&QFile::exists(result),"same songId gets independent request directory");
 service->stop();
 Oran7Track fallback;fallback.title="legacy";fallback.artist="artist";
 service->load("https://cdn/legacy",root.path(),fallback);
 last(log)->complete("{}",QNetworkReply::ContentNotFoundError);check(last(log)->url().path()=="/api/search","LRCLIB exact falls back to search");
 last(log)->complete("[]");check(last(log)->url().path()=="/api/search/get","LRCLIB falls back to NetEase search");
 last(log)->complete(R"({"result":{"songs":[{"id":42}]}})");check(last(log)->url().path()=="/api/song/lyric","NetEase search resolves lyrics");
 last(log)->complete(R"({"lrc":{"lyric":"[00:01]cascade"}})");events();check(QFile::exists(result),"cascade saves and publishes");
 service->load("https://cdn/no-metadata",root.path(),{});check(missing==beforeMissing+1,"unsearchable source produces terminal missing state");
 service->load("https://cdn/fail",root.path(),track);last(log)->complete("{}",QNetworkReply::ConnectionRefusedError);events();check(missing==beforeMissing+2,"current transport error publishes missing");
 service->load("https://cdn/end",root.path(),track);const int beforeDelete=log.aborted;service.reset();check(log.aborted==beforeDelete+1&&log.managers==log.destroyed,"service destruction aborts and releases transport");
 Oran7Async::waitForDone();std::puts("PASS: local LRC/cache, real fetch cascade/auth/CDN id, queued stale success/failure, cancellation, same-id isolation, RAII cleanup");
}
