#include "Oran7CoverNetworkFactory.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <memory>

static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "isolated cover cache directory");
    QImage image(8, 8, QImage::Format_ARGB32);
    image.fill(Qt::cyan);
    QByteArray png;
    QBuffer output(&png);
    output.open(QIODevice::WriteOnly);
    check(image.save(&output, "PNG"), "synthetic cover bytes");
    // 回环代理截获所有请求；测试使用 CDN 形式的地址，但不解析 DNS 或访问外网。
    QTcpServer proxy;
    check(proxy.listen(QHostAddress::LocalHost), "loopback proxy listen");
    int requests = 0;
    QObject::connect(&proxy, &QTcpServer::newConnection, &app, [&]
    {
        while (auto* socket = proxy.nextPendingConnection())
        {
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket]
            {
                if (socket->property("handled").toBool())
                    return;
                auto request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                if (!request.contains("\r\n\r\n"))
                    return;
                socket->setProperty("handled", true);
                ++requests;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n"
                              "Cache-Control: public, max-age=31536000\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(png.size()) + "\r\n\r\n" + png);
                socket->disconnectFromHost();
            });
        }
    });
    Oran7CoverNetworkFactory factory(directory.path());
    auto manager = std::unique_ptr<QNetworkAccessManager>(factory.create(nullptr));
    const QNetworkProxy localProxy(QNetworkProxy::HttpProxy, "127.0.0.1", proxy.serverPort());
    manager->setProxy(localProxy);
    auto fetch = [&](const QUrl& url, bool cached)
    {
        std::unique_ptr<QNetworkReply> reply(manager->get(QNetworkRequest(url)));
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(3000);
        loop.exec();
        check(reply->isFinished() && reply->error() == QNetworkReply::NoError, "cover request completes");
        check(reply->readAll() == png, "cached cover bytes preserved");
        check(reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool() == cached,
              "expected network or disk cache source");
    };
    const QUrl cover("http://p1.music.126.net/fixture-cover.png?param=80y80");
    fetch(cover, false);
    fetch(cover, true);
    check(requests == 1, "revisiting a cover avoids another HTTP request");
    manager.reset(factory.create(nullptr));
    manager->setProxy(localProxy);
    fetch(cover, true);
    check(requests == 1, "cover survives network manager recreation on disk");
    auto* disk = qobject_cast<QNetworkDiskCache*>(manager->cache());
    check(disk && disk->maximumCacheSize() == 256LL * 1024 * 1024, "cover disk storage is bounded");
    fetch(QUrl("http://api.example.invalid/fixture.json"), false);
    fetch(QUrl("http://api.example.invalid/fixture.json"), false);
    fetch(QUrl("http://music.126.net.example.invalid/fixture.png"), false);
    fetch(QUrl("http://music.126.net.example.invalid/fixture.png"), false);
    check(requests == 5, "API and suffix spoof hosts are not persisted");
    check(disk->remove(cover), "remove cached fixture");
    fetch(cover, false);
    check(requests == 6, "cache miss falls back to normal download");
    std::puts("PASS: bounded persistent cover cache, offline proxy only");
}
