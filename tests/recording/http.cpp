// 实际传输实现的本机 HTTP 检查；不编入 libdatachannel，不连接公网。
#include "../../src/cpp/Oran7ScreenCapture/Oran7WhipPublisher.cpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstdio>
#include <cstdlib>
bool Oran7VideoEncoder::getPacket(AVPacket*,int){return false;}
bool Oran7AudioEncoder::getPacket(AVPacket*,int){return false;}
bool Oran7Async::isShuttingDown(){return true;}
bool Oran7Async::submit(std::function<void()> work, Group){work();return true;}
static int checks=0;
static void check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}++checks;}
int main(int argc,char**argv){
 QGuiApplication app(argc,argv);QTcpServer server;check(server.listen(QHostAddress::LocalHost),"localhost HTTP fixture");
 QList<QByteArray> requests;
 QObject::connect(&server,&QTcpServer::newConnection,[&]{while(auto* socket=server.nextPendingConnection()){
  auto buffer=std::make_shared<QByteArray>();QObject::connect(socket,&QTcpSocket::readyRead,[&,socket,buffer]{
   buffer->append(socket->readAll());const int header=buffer->indexOf("\r\n\r\n");if(header<0)return;
   int length=0;for(const auto& line:buffer->left(header).split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();
   if(buffer->size()<header+4+length)return;requests<<*buffer;
   socket->write("HTTP/1.1 201 Created\r\nContent-Length: 7\r\nLocation: /resource/one\r\nETag: version\r\nConnection: close\r\n\r\nanswer!");socket->disconnectFromHost();
  });QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
 }});
 const auto url=QString("http://127.0.0.1:%1/custom/whip").arg(server.serverPort()).toStdString();
 const std::string token="per-session-token";
 const auto post=httpPostSync(url,"offer","application/sdp",token);
 check(post.status==201&&post.body=="answer!"&&post.resourcePath=="/resource/one"&&post.etag=="version","real POST response headers preserved");
 check(requests.last().startsWith("POST /custom/whip ")&&requests.last().contains("Authorization: Bearer per-session-token"),"full endpoint and configurable Bearer used");
 check(!requests.last().contains("123456"),"legacy hardcoded credential absent");
 const auto patch=httpPatchSync(url,"candidate","application/trickle-ice-sdpfrag","version",token);
 check(patch.status==201&&requests.last().startsWith("PATCH ")&&requests.last().contains("If-Match: version")&&requests.last().contains("Bearer per-session-token"),"real PATCH forwards credential and ETag");
 check(httpDeleteSync(url,"version",token)==201&&requests.last().startsWith("DELETE ")&&requests.last().contains("Bearer per-session-token"),"real DELETE uses same credential");
 httpPostSync(url,"offer","application/sdp",{});check(!requests.last().contains("Authorization:"),"empty Bearer does not inject a testing token");
 {
  Oran7WhipPublisher publisher;int errors=0;publisher.setErrorCallback([&](const auto&){++errors;});Oran7WhipPublisher::Config config;
  check(!publisher.open(config,nullptr,nullptr)&&errors==1,"partial open reports error");publisher.close();publisher.close();
  check(!publisher.isRunning(),"close cleans failed open and is idempotent");
 }
 std::printf("PASS: %d real WHIP HTTP/credential/failed-open checks (localhost)\n",checks);
}
