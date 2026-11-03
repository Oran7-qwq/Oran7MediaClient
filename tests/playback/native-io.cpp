#include "../../src/cpp/Oran7MediaPlayer/Oran7MediaPlayer.h"
#include "../../src/cpp/Oran7MediaPlayer/ffmsg.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QtEndian>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <future>

static void check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
template<class Predicate> static void until(Predicate predicate, const char* message) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed()<8000) {
        QCoreApplication::processEvents(); QThread::msleep(1);
    }
    check(predicate(),message);
}
int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    SDL_SetMainReady();
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    QTcpServer server;
    check(server.listen(QHostAddress::LocalHost),"listen on loopback only");
    int responses=0;
    QObject::connect(&server,&QTcpServer::newConnection,&server,[&] {
        while (server.hasPendingConnections()) {
            auto* socket=server.nextPendingConnection();
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket] {
                if (socket->property("sent").toBool()) { socket->readAll(); return; }
                if (!socket->readAll().contains("GET")) return;
                socket->setProperty("sent",true);
                // 大尺寸 WAV 只交付少量数据，故意阻塞 FFmpeg open/probe 的后续网络读取。
                QByteArray wav(44+65536,'\0');
                wav.replace(0,4,"RIFF"); wav.replace(8,8,"WAVEfmt "); wav.replace(36,4,"data");
                auto put16=[&](int at,quint16 n){qToLittleEndian(n,wav.data()+at);};
                auto put32=[&](int at,quint32 n){qToLittleEndian(n,wav.data()+at);};
                put32(4,4000036); put32(16,16); put16(20,1); put16(22,2);
                put32(24,48000); put32(28,192000); put16(32,4); put16(34,16); put32(40,4000000);
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: 4000044\r\n\r\n");
                socket->write(wav); socket->flush(); ++responses;
            });
        }
    });
    for (int iteration=0;iteration<8;++iteration) {
        std::atomic<bool> probing{false};
        auto player=std::make_unique<Oran7MediaPlayer>();
        check(player->oran7mp_create([&probing](void* raw) {
            auto* native=static_cast<Oran7MediaPlayer*>(raw);
            AVMessage message;
            while (native->oran7mp_get_msg(&message,1)>=0) {
                if (message.value==FFP_MSG_OPEN_INPUT) probing=true;
                msg_free_obj_res(&message);
            }
            return 0;
        })==0,"create real native player");
        const auto url=QString("http://127.0.0.1:%1/slow.wav").arg(server.serverPort()).toUtf8();
        player->oran7mp_set_data_source(url.constData());
        check(player->oran7mp_prepare_async()==0,"prepare real FFmpeg reader");
        const auto previous=responses;
        until([&]{return responses>previous;},"serve partial synthetic input");
        until([&]{return probing.load();},"reader enters stream probing before cancellation");
        auto retired=std::async(std::launch::async,[player=std::move(player)]() mutable {player.reset();});
        int guiTurns=0;
        until([&]{++guiTurns; return retired.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;},
            "interrupt and join native reader without freeing its active I/O");
        retired.get();
        check(guiTurns>1,"event loop progresses while native teardown waits for cancellation");
    }
    std::puts("PASS: 8 real FFmpeg slow-input cancellation cycles (dummy SDL, loopback, no account)");
}
