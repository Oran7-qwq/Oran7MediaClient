#include "CurlHttpClient.h"
#include "BlockingCurl.h"
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QSignalSpy>
#include <QProcess>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QDir>
#include <mutex>
#include <atomic>
#include <future>
#include <chrono>
#include <memory>
#include <cstdio>
#include <cstdlib>

static int checks=0;
static QStringList logs;
static std::mutex logMutex;
static std::atomic<bool> delayStart{true}, workerDelayed{false}, offGui{false};
static void check(bool ok,const char* text){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
template<class F>static void until(F predicate){QElapsedTimer timer;timer.start();while(!predicate()&&timer.elapsed()<6000){QCoreApplication::processEvents();QThread::msleep(1);}check(predicate(),"等待本机 HTTP/进程事件超时");}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& text){
  {std::lock_guard<std::mutex> lock(logMutex);logs.append(text);}
  if(text.contains("Ncm HTTP request=")) {
   if(QThread::currentThread()!=QCoreApplication::instance()->thread()) offGui=true;
   if(delayStart.exchange(false)) {workerDelayed=true;QThread::msleep(400);}
  }
 });
 QTcpServer server;check(server.listen(QHostAddress::LocalHost,0),"本机 fixture server 启动");
 QList<QByteArray> received;
 QObject::connect(&server,&QTcpServer::newConnection,[&]{while(server.hasPendingConnections()){
  auto* socket=server.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();auto replied=std::make_shared<bool>(false);
  QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket,buffer,replied]{
   *buffer+=socket->readAll();if(*replied)return;auto end=buffer->indexOf("\r\n\r\n");if(end<0)return;
   auto match=QRegularExpression("Content-Length: (\\d+)",QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(buffer->left(end)));
   const int length=match.hasMatch()?match.captured(1).toInt():0;if(buffer->size()<end+4+length)return;
   *replied=true;received.append(*buffer);if(buffer->contains("/never"))return;
   const int delay=buffer->contains("/slow")?100:0;
   QTimer::singleShot(delay,socket,[socket]{const QByteArray body="response-secret";socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nSet-Cookie: MUSIC_U=fixture-secret; HttpOnly\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});
  });
  QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
 }});
 const QString base="http://127.0.0.1:"+QString::number(server.serverPort());
 QTemporaryDir requestRoot;check(requestRoot.isValid(),"隔离请求目录");
 CurlHttpClient transport(nullptr,{},requestRoot.path());QSignalSpy done(&transport,&CurlHttpClient::requestFinished),failed(&transport,&CurlHttpClient::requestError);
 int beats=0;QTimer heartbeat;QObject::connect(&heartbeat,&QTimer::timeout,[&]{++beats;});heartbeat.start(5);
 bool resultOnGui=true;
 QObject::connect(&transport,&CurlHttpClient::requestFinished,[&]{resultOnGui&=QThread::currentThread()==app.thread();});
 NcmRequest slow;slow.tag="slow";slow.isPost=false;slow.url=QUrl(base+"/slow?word=中文&token=fixture-secret");slow.headers.insert("Cookie","MUSIC_U=fixture-secret");
 QElapsedTimer submission;submission.start();transport.send(slow);
 check(submission.elapsed()<100,"提交请求不在调用线程等待进程启动");
 until([&]{return workerDelayed.load();});
 const auto beforeBeat=beats;
 QElapsedTimer delayWindow;delayWindow.start();
 until([&]{return delayWindow.elapsed()>150;});
 check(beats>beforeBeat+5 && offGui.load() && done.isEmpty(),"后台启动人为阻塞时 GUI 心跳仍连续推进");
 NcmRequest fast;fast.tag="fast";fast.url=QUrl(base+"/fast");fast.body="csrf_token=fixture-secret";transport.send(fast);
 until([&]{return done.size()==2;});
 check(resultOnGui,"HTTP 完成结果回到前端所属线程");
 check(received.join('\n').contains("%E4%B8%AD")&&received.join('\n').contains("HTTP/1.1"),"中文 URL 编码与 HTTP1.1 保留");
 check(done[0][0].toULongLong()==fast.id&&done[1][0].toULongLong()==slow.id,"真实乱序 HTTP 结果保留 ID/tag");
 check(done[0][1]=="fast"&&done[1][1]=="slow"&&done[0][2].toInt()==200,"操作标签与 HTTP 状态准确");
 check(done[0][3].toByteArray()=="response-secret"&&done[0][4].toString().contains("MUSIC_U"),"响应值仍向消费者交付，日志不依赖正文");
 until([&]{return QDir(requestRoot.path()).entryList(QDir::Dirs|QDir::NoDotAndDotDot).isEmpty();});
 check(true,"正常完成后后台 RAII 清理临时目录");
 CurlHttpClient bad(nullptr,"C:/oran7-fixture-does-not-exist/curl.exe");QSignalSpy startError(&bad,&CurlHttpClient::requestError);
 NcmRequest missing;missing.tag="missing";missing.url=QUrl(base+"/fast");bad.send(missing);
 until([&]{return startError.size()==1;});QCoreApplication::processEvents();
 check(startError[0][0].toULongLong()==missing.id&&startError[0][1]=="missing","进程无法启动时发一次带 ID 的错误");
 NcmRequest unavailable;unavailable.tag="refused";unavailable.isPost=false;unavailable.url=QUrl("http://127.0.0.1:1/?token=fixture-secret");transport.send(unavailable);
 until([&]{return failed.size()==1;});check(failed[0][0].toULongLong()==unavailable.id&&!failed[0][2].toString().contains("fixture-secret"),"curl 失败只返回脱敏错误与 ID");
 struct Oran7BlockingResult {bool ok;int status;QByteArray body;QString error;};
 NcmRequest block;block.tag="blocking";block.url=QUrl(base+"/blocking");block.body="body-secret";
 auto future=std::async(std::launch::async,[block]{Oran7BlockingResult r{};r.ok=ncm::blockingCurl(block,&r.body,&r.status,&r.error,2000);return r;});
 until([&]{return future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;});auto result=future.get();
 check(result.ok&&result.status==200&&result.body=="response-secret","阻塞传输真实本机 HTTP 成功");
 auto timeout=std::async(std::launch::async,[base]{NcmRequest r;r.isPost=false;r.url=QUrl(base+"/never?token=fixture-secret");Oran7BlockingResult out{};out.ok=ncm::blockingCurl(r,&out.body,&out.status,&out.error,150);return out;});
 until([&]{return timeout.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;});const auto timed=timeout.get();check(!timed.ok&&!timed.error.contains("fixture-secret"),"毫秒超时正确转为 curl 秒，失败错误脱敏");
 QTemporaryDir pendingRoot;
 auto retiring=std::make_unique<CurlHttpClient>(nullptr,QString(),pendingRoot.path());NcmRequest pending;pending.tag="pending";pending.isPost=false;pending.url=QUrl(base+"/never");
 const auto beforePending=received.size();retiring->send(pending);
 until([&]{return received.size()>beforePending;});retiring.reset();
 check(QDir(pendingRoot.path()).entryList(QDir::Dirs|QDir::NoDotAndDotDot).isEmpty(),"传输析构在后台终止进程并清理未完成请求目录");
 {std::lock_guard<std::mutex> lock(logMutex);
 check(!logs.join('\n').contains("fixture-secret")&&!logs.join('\n').contains("response-secret"),"所有传输日志不含 Cookie、URL token 或响应正文");}
 std::printf("PASS: %d real curl transport checks (loopback only), correlation, encoding, failure, timeout, RAII and log redaction\n",checks);
}
