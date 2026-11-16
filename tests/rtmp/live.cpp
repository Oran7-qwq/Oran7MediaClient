#include "Oran7LiveSession.h"
#include "Oran7RecordingFeed.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 Oran7LiveConfig c{"custom","rtmp://127.0.0.1:1/live","FICTIONAL",1920,1080,60,6000,true};
 check(Oran7LiveSession::validate(c,true).isEmpty(),"valid independent live config");
 auto invalid=c;invalid.fps=240;check(!Oran7LiveSession::validate(invalid,true).isEmpty(),"local 240fps cannot leak into live");
 invalid=c;invalid.width=3840;check(!Oran7LiveSession::validate(invalid,true).isEmpty(),"live dimensions bounded");
 invalid=c;invalid.server.clear();invalid.streamKey.clear();check(Oran7LiveSession::validate(invalid,false).isEmpty(),"credentials can be cleared together");
 Oran7LiveSession session;
 auto feed=std::make_shared<Oran7RecordingFeed>();feed->close();
 check(session.start(c,feed) && session.busy() && session.state()=="Connecting","live starts its own asynchronous preparation");
 check(!session.start(c,feed),"rapid second start rejected");
 QElapsedTimer t;t.start();while(session.busy() && t.elapsed()<3000){QCoreApplication::processEvents();QThread::msleep(2);}
 check(!session.busy() && session.state()=="Error" && !session.error().isEmpty(),"capture unavailable yields Error without hardware or platform connection");
 check(!session.error().contains("FICTIONAL"),"live error does not expose credentials");
 check(session.start(c,feed),"explicit retry creates a new generation");session.stop();
 t.restart();while(session.busy() && t.elapsed()<3000){QCoreApplication::processEvents();QThread::msleep(2);}
 check(!session.busy(),"cancelled preparation retires asynchronously");session.shutdown();
 std::puts("PASS: independent live lifecycle, validation, cancellation and retry");
}
