#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <cstring>

class Oran7FakeRoomReply : public QNetworkReply
{
public:
    Oran7FakeRoomReply(const QNetworkRequest& request,QByteArray body,QObject* parent)
        : QNetworkReply(parent),m_body(std::move(body))
    {
        setRequest(request); setUrl(request.url()); open(QIODevice::ReadOnly);
        QTimer::singleShot(0,this,[this] {
            if (isFinished()) return;
            setFinished(true); emit readyRead(); emit finished();
        });
    }
    void abort() override {
        if (isFinished()) return;
        setError(OperationCanceledError,"fixture canceled"); setFinished(true); emit finished();
    }
    qint64 bytesAvailable() const override { return m_body.size()-m_offset+QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* data,qint64 length) override {
        const auto count=qMin(length,qint64(m_body.size())-m_offset);
        if (count<=0) return -1;
        std::memcpy(data,m_body.constData()+m_offset,size_t(count)); m_offset+=count; return count;
    }
private:
    QByteArray m_body;
    qint64 m_offset=0;
};
class Oran7FakeRoomNetwork : public QNetworkAccessManager
{
public:
    bool fail=false;
    int calls=0,active=0,maximumActive=0;
protected:
    QNetworkReply* createRequest(Operation,const QNetworkRequest& request,QIODevice*) override {
        ++calls; ++active; maximumActive=qMax(maximumActive,active);
        QByteArray body;
        if (fail) body=R"({"code":-352})";
        else if (request.url().path().contains("get_info"))
            body=R"({"code":0,"data":{"room_id":12345,"title":"Fixture live title","live_status":1,"user_cover":""}})";
        else body=R"({"code":0,"data":{"info":{"uname":"Fixture anchor","face":""}}})";
        auto* reply=new Oran7FakeRoomReply(request,body,this);
        connect(reply,&QNetworkReply::finished,this,[this] { --active; });
        return reply;
    }
};
