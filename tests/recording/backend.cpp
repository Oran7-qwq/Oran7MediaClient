#include "Oran7RecordingBackend.h"
#include "Oran7WasapiAudioWorker.h"
#include "Oran7Mp4Muxer.h"
#include "Oran7WhipPublisher.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QDir>
#include <cstdio>
#include <cstdlib>

static int checks=0, videos=0, audios=0, captures=0, muxers=0, publishers=0;
static bool videoFailure=false,audioFailure=false,muxerFailure=false,whipFailure=false;
static bool lastLiveStreaming=false;
static QStringList order;
static QString lastFormat;
static void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}++checks;}
Oran7VideoEncoder::Oran7VideoEncoder(){++videos;}
Oran7VideoEncoder::~Oran7VideoEncoder(){stop();--videos;}
bool Oran7VideoEncoder::configure(const EncoderConfig& config,ErrorCallback){lastLiveStreaming=config.liveStreaming;return !videoFailure;}
bool Oran7VideoEncoder::start(){_running=true;_opened=true;return true;}
bool Oran7VideoEncoder::stop(){_running=false;order<<"videoStop";return true;}
Oran7AudioEncoder::Oran7AudioEncoder(){++audios;}
Oran7AudioEncoder::~Oran7AudioEncoder(){stop();--audios;}
bool Oran7AudioEncoder::configure(const EncoderConfig&,ErrorCallback){return !audioFailure;}
bool Oran7AudioEncoder::start(){_running=true;_opened=true;return true;}
bool Oran7AudioEncoder::stop(){_running=false;order<<"audioStop";return true;}
void Oran7AudioEncoder::pushFrame(AVFrame* frame){av_frame_free(&frame);}
bool Oran7WasapiAudioWorker::start(const Oran7WasapiConfig&){_running=true;++captures;return true;}
void Oran7WasapiAudioWorker::stop(){if(_running.exchange(false))--captures;}
bool Oran7WasapiAudioWorker::isRunning()const{return _running.load();}
bool Oran7WasapiAudioWorker::getPcmFrame(AVFrame**,int){QThread::msleep(5);return false;}
Oran7Mp4Muxer::Oran7Mp4Muxer(){++muxers;}
Oran7Mp4Muxer::~Oran7Mp4Muxer(){close();--muxers;}
bool Oran7Mp4Muxer::open(const char*,int,int,AVRational,std::shared_ptr<Oran7VideoEncoder>&,std::shared_ptr<Oran7AudioEncoder>&,const char* format){lastFormat=QString::fromUtf8(format);_opened=!muxerFailure;return _opened;}
void Oran7Mp4Muxer::close(){if(_opened){order<<"muxClose";_opened=false;}}
Oran7WhipPublisher::Oran7WhipPublisher(){++publishers;}
Oran7WhipPublisher::~Oran7WhipPublisher(){--publishers;}
bool Oran7WhipPublisher::open(const Config&,std::shared_ptr<Oran7VideoEncoder>,std::shared_ptr<Oran7AudioEncoder>){return !whipFailure;}
void Oran7WhipPublisher::close(){order<<"whipClose";}
std::string Oran7WhipPublisher::streamId()const{return "stream-fixture";}
std::string Oran7WhipPublisher::connectionState()const{return "connected";}
std::string Oran7WhipPublisher::iceState()const{return "completed";}
static std::shared_ptr<Oran7RecordingFeed> source(){auto feed=std::make_shared<Oran7RecordingFeed>();auto* device=av_buffer_alloc(8);feed->publish(1920,1080,device);av_buffer_unref(&device);return feed;}
static void noLeaks(){check(videos==0&&audios==0&&captures==0&&muxers==0&&publishers==0,"all preparation/runtime resources reclaimed");}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);QTemporaryDir dir;check(dir.isValid(),"temporary outputs");
 Oran7RecordingConfig c;c.format="mp4";c.fps=60;c.bitrateKbps=40000;c.filePrefix="中文录制";c.outputDirectory=dir.path();c.audioEnabled=true;
 check(Oran7RecordingBackend::validate(c,true).isEmpty(),"MP4 options valid");
 for(const QVariantMap bad:{QVariantMap{{"fps",0}},QVariantMap{{"bitrateKbps",99}},QVariantMap{{"width",1279},{"height",720}},QVariantMap{{"width",1280}},QVariantMap{{"filePrefix","../escape"}},QVariantMap{{"fps","60"}},QVariantMap{{"bearerToken","x\r\ny"}},QVariantMap{{"whipUrl","file:///test"}},QVariantMap{{"signalingUrl","https://example.test"}},QVariantMap{{"outputDirectory","relative"}},QVariantMap{{"width",QVariant::fromValue(qint64(1)<<40)}}}){
  auto changed=c;check(!Oran7RecordingBackend::apply(bad,&changed).isEmpty(),"invalid draft rejected");check(changed.fps==c.fps&&changed.width==c.width,"invalid draft cannot partially mutate settings");
 }
 auto changed=c;check(Oran7RecordingBackend::apply({{"width",1280},{"height",720},{"fps",30},{"audioEnabled",false}},&changed).isEmpty()&&changed.width==1280&&!changed.audioEnabled,"valid draft aggregated");
 check(Oran7RecordingBackend::settings(changed).value("width").toInt()==1280,"QML settings map");
 std::atomic_bool cancel{false};int started=0;QString output;
 Oran7RecordingCallbacks cb;
 cb.started=[&](const QVariantMap& result){++started;output=result.value("outputPath").toString();cancel=true;};cb.statistics=[](const auto&){};
 auto feed=source();check(Oran7RecordingBackend::run(c,feed,cancel,cb).isEmpty()&&started==1&&QFile::exists(output),"successful MP4 setup commits unique output");noLeaks();
 check(!lastLiveStreaming,"MP4 requests recording encoder policy");
 check(!feed->snapshot().encoder,"encoder detached before completion");
 check(order.indexOf("videoStop")<order.indexOf("muxClose")&&order.indexOf("audioStop")<order.indexOf("muxClose"),"producers flushed before mux finalization");
 const auto first=output;cancel=false;check(Oran7RecordingBackend::run(c,source(),cancel,cb).isEmpty()&&output!=first&&QFile::exists(first),"repeated recording never overwrites existing output");noLeaks();
 for(auto* failure:{&videoFailure,&audioFailure,&muxerFailure}){
  *failure=true;cancel=false;auto f=source();const auto before=started;
  check(!Oran7RecordingBackend::run(c,f,cancel,cb).isEmpty()&&started==before&&!f->snapshot().encoder,"preparation failure never reports recording");noLeaks();*failure=false;
 }
 check(QDir(dir.path()).entryList(QDir::Files).size()==2,"uncommitted MP4 output removed on failure");
 auto flv=c;check(Oran7RecordingBackend::apply({{"format","flv"}},&flv).isEmpty(),"FLV selected independently from output mode");
 cancel=false;check(Oran7RecordingBackend::run(flv,source(),cancel,cb).isEmpty()&&output.endsWith(".flv")&&lastFormat=="flv", "FLV creates correct extension and passes explicit muxer format");noLeaks();
 check(!lastLiveStreaming && Oran7RecordingBackend::settings(flv).value("format")=="flv", "FLV retains local H264/AAC encoder policy and settings roundtrip");
 auto invalidFormat=flv;check(!Oran7RecordingBackend::apply({{"format","avi"}},&invalidFormat).isEmpty()&&invalidFormat.format=="flv", "unsupported format rejected atomically");
 cancel=true;check(Oran7RecordingBackend::run(c,source(),cancel,cb).isEmpty(),"pre-cancelled setup exits cleanly");noLeaks();
 auto closed=source();closed->close();cancel=false;check(!Oran7RecordingBackend::run(c,closed,cancel,cb).isEmpty(),"stopped capture cannot start recording");
 c.mode=1;check(!Oran7RecordingBackend::validate(c,true).isEmpty(),"WHIP requires endpoint");c.whipUrl="http://127.0.0.1/whip";whipFailure=true;
 check(!Oran7RecordingBackend::run(c,source(),cancel,cb).isEmpty(),"WHIP failure returns error");noLeaks();whipFailure=false;
 cb.started=[&](const QVariantMap& result){check(result.value("streamId")=="stream-fixture","WHIP stream ID delivered");cancel=true;};
 check(Oran7RecordingBackend::run(c,source(),cancel,cb).isEmpty(),"WHIP lifecycle retains implementation");noLeaks();
 check(lastLiveStreaming,"WHIP retains existing streaming encoder policy");
 cancel=false;cb.started=[&](const auto&){feed->close();};feed=source();
 check(!Oran7RecordingBackend::run(c,feed,cancel,cb).isEmpty(),"capture loss ends active recording");noLeaks();
 std::printf("PASS: %d recording settings/orchestration/RAII checks (device/network stubs)\n",checks);
}
