#include "Oran7RecordingSession.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QSignalSpy>
#include <QPointer>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
using State=Oran7RecordingSession::Oran7RecordingState;
static int checks=0;
static void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}++checks;}
template<class F> static void until(F f){QElapsedTimer timer;timer.start();while(!f()&&timer.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(1);}check(f(),"bounded async completion");}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 Oran7RecordingSession session;QSignalSpy started(&session,&Oran7RecordingSession::started),finished(&session,&Oran7RecordingSession::finished);
 check(session.state()==State::Idle&&!session.busy(),"initial idle");
 std::atomic<bool> entered{false},cleaned{false};int ticks=0;QTimer timer;timer.setInterval(5);QObject::connect(&timer,&QTimer::timeout,[&]{++ticks;});timer.start();
 Oran7RecordingCallbacks oldCallbacks;
 QElapsedTimer elapsed;elapsed.start();
 check(session.start([&](const auto& cancel,const auto& cb)->QString{entered=true;oldCallbacks=cb;QThread::msleep(140);if(cancel.load())return {};cb.started({{"outputPath","fixture.mp4"}});cb.statistics({{"framesSent",42}});while(!cancel.load())QThread::msleep(2);QThread::msleep(80);cleaned=true;return {};}),"start request accepted");
 check(elapsed.elapsed()<100&&session.state()==State::Preparing&&session.busy(),"start returns without waiting for preparation");
 check(!session.start([](const auto&,const auto&){return QString();}),"duplicate start rejected");
 until([&]{return started.size()==1;});check(ticks>8,"GUI timer advances while preparing");
 until([&]{return session.statistics().value("framesSent").toInt()==42;});
 check(session.state()==State::Recording,"worker readiness owns recording transition");
 elapsed.restart();session.stop();check(elapsed.elapsed()<100&&session.state()==State::Stopping&&session.busy(),"stop request nonblocking");
 check(!session.start([](const auto&,const auto&){return QString();}),"restart rejected until resources released");
 session.stop();until([&]{return finished.size()==1;});check(cleaned&&session.state()==State::Idle&&!session.busy(),"finished only after cleanup");
 oldCallbacks.started({});oldCallbacks.statistics({{"framesSent",999}});QCoreApplication::processEvents();
 check(started.size()==1&&session.statistics().value("framesSent").toInt()==42,"late callbacks cannot resurrect idle recording");
 check(session.start([](const auto& cancel,const auto& cb){QThread::msleep(70);cb.started({});while(!cancel.load())QThread::msleep(2);return QString();}),"next generation accepted");
 oldCallbacks.statistics({{"framesSent",1234}});session.stop();until([&]{return finished.size()==2;});
 check(started.size()==1&&session.statistics().isEmpty(),"cancel during preparation rejects old and delayed started callbacks");
 check(session.start([](const auto&,const auto&)->QString{throw std::runtime_error("private details");}),"exception job accepted");until([&]{return session.state()==State::Error;});
 check(!session.error().isEmpty()&&!session.error().contains("private"),"exceptions become sanitized error state");
 check(session.start([](const auto&,const auto&){return QStringLiteral("输出失败");}),"restart after error");until([&]{return finished.size()==4;});check(session.error()==QStringLiteral("输出失败"),"backend failure retained");
 {
  Oran7RecordingSession reentrant;int notifications=0;
  QObject::connect(&reentrant,&Oran7RecordingSession::changed,[&]{if(reentrant.state()==State::Preparing||reentrant.state()==State::Stopping){++notifications;reentrant.stop();}});
  check(reentrant.start([](const auto& cancel,const auto&){while(!cancel.load())QThread::msleep(1);return QString();}),"reentrant start/stop accepted");until([&]{return !reentrant.busy();});check(notifications==2,"repeated stop is idempotent during synchronous notifications");
 }
 {
  auto dying=std::make_unique<Oran7RecordingSession>();std::atomic<bool> released{false};
  check(dying->start([&](const auto& cancel,const auto& cb){while(!cancel.load())QThread::msleep(1);cb.started({});released=true;return QString();}),"destruction fixture accepted");
  dying.reset();QCoreApplication::processEvents();check(released,"destructor cancels joins and invalidates queued callbacks");
 }
 session.shutdown();check(!session.start([](const auto&,const auto&){return QString();}),"shutdown rejects new work");
 std::printf("PASS: %d recording lifecycle/cancellation/reentrancy checks\n",checks);
}
