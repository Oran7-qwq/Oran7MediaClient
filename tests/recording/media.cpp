#include "Oran7Mp4Muxer.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>
static int checks=0;
static void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}++checks;}
template<class F>static void until(F f){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<5000)QThread::msleep(5);check(f(),"codec opened within bound");}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);QTemporaryDir dir;check(dir.isValid(),"temporary media files");
 for(bool withAudio:{false,true}){
  std::atomic<int> errors{0};auto error=[&](const std::string& message){++errors;std::fprintf(stderr,"codec: %s\n",message.c_str());};
  auto video=std::make_shared<Oran7VideoEncoder>();Oran7VideoEncoder::EncoderConfig vc;vc.codecName="libx264";vc.width=96;vc.height=64;vc.fps=30;vc.bitrateKbps=300;vc.gopSize=30;
  check(video->configure(vc,error)&&video->start(),"real software video configure");until([&]{return video->isOpened();});
  std::shared_ptr<Oran7AudioEncoder> audio;
  if(withAudio){audio=std::make_shared<Oran7AudioEncoder>();Oran7AudioEncoder::EncoderConfig ac;ac.codecName="aac";check(audio->configure(ac,error)&&audio->start(),"real AAC configure");until([&]{return audio->isOpened();});}
  Oran7Mp4Muxer mux; mux.setErrorCallback(error);const auto path=dir.filePath(withAudio?"音视频.mp4":"视频.mp4");
  check(mux.open(path.toUtf8().constData(),96,64,{1,30},video,audio),"real MP4 header opened");
  for(int i=0;i<45;++i){
   AVFrame* frame=av_frame_alloc();frame->format=AV_PIX_FMT_YUV420P;frame->width=96;frame->height=64;frame->pts=i*3000;
   check(av_frame_get_buffer(frame,32)>=0,"synthetic frame allocation");
   for(int y=0;y<64;++y)memset(frame->data[0]+y*frame->linesize[0],40+i,96);
   for(int y=0;y<32;++y){memset(frame->data[1]+y*frame->linesize[1],128,48);memset(frame->data[2]+y*frame->linesize[2],128,48);}
   video->pushFrame(frame);
   if(audio){AVFrame* pcm=av_frame_alloc();pcm->format=AV_SAMPLE_FMT_S16;pcm->sample_rate=48000;av_channel_layout_default(&pcm->ch_layout,2);pcm->nb_samples=1600;pcm->pts=i*1600;check(av_frame_get_buffer(pcm,0)>=0,"synthetic PCM allocation");memset(pcm->data[0],0,1600*4);audio->pushFrame(pcm);}
   QThread::msleep(8);
  }
  QThread::msleep(200);if(audio)audio->stop();video->stop();mux.close();mux.close();
  check(errors.load()==0,"real encoding and MP4 close have no errors");
  AVFormatContext* format=nullptr;check(avformat_open_input(&format,path.toUtf8().constData(),nullptr,nullptr)>=0,"MP4 moov/readback valid");
  check(avformat_find_stream_info(format,nullptr)>=0&&format->nb_streams==(withAudio?2u:1u),"expected video/audio tracks");
  int packets=0;AVPacket pkt{};while(av_read_frame(format,&pkt)>=0){++packets;av_packet_unref(&pkt);}avformat_close_input(&format);
  check(packets>=30&&QFileInfo(path).size()>1000,"real file contains encoded payload");
 }
 {
  auto video=std::make_shared<Oran7VideoEncoder>();Oran7VideoEncoder::EncoderConfig invalid;invalid.codecName="libx264";invalid.width=0;std::atomic<int> errors{0};
  check(video->configure(invalid,[&](const auto&){++errors;})&&video->start(),"asynchronous open failure fixture");until([&]{return !video->isRuning();});video->stop();video.reset();check(errors>0,"failed codec thread joins without terminate");
 }
 std::printf("PASS: %d real encoder/MP4/readback/failure checks\n",checks);
}
