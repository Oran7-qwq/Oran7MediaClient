#include "NcmSongApi.h"
#include "CurlHttpClient.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QVector>
#include <QDebug>
#include <cstdlib>
#include <cstdio>
static QList<NcmRequest> sent;
static bool failImmediately = false;
CurlHttpClient::CurlHttpClient(QObject* parent, const QString&, const QString&) : QObject(parent) {}
CurlHttpClient::~CurlHttpClient() = default;
void CurlHttpClient::send(const NcmRequest& request) {
    sent.append(request);
    if (failImmediately) emit requestError(request.id, request.tag, QStringLiteral("模拟同步失败"));
}
static void check(bool ok, const char* text) { if (!ok) { qCritical()<<text;std::exit(1); } }
int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    CurlHttpClient transport;
    NcmSongApi api(&transport);
    QVector<quint64> ids;QVector<QHash<qint64,QString>> results;int errors=0;
    QObject::connect(&api,&NcmSongApi::urlsResolved,[&](quint64 id,const QHash<qint64,QString>& map){ids.append(id);results.append(map);});
    QObject::connect(&api,&NcmSongApi::apiError,[&](quint64,const QString&){++errors;});
    auto reply=[&](const NcmRequest& request,int status,const QByteArray& body){emit transport.requestFinished(request.id,request.tag,status,body,{});};
    api.resolveUrls({11},101);api.resolveUrls({22},102);api.resolveUrls({33});
    check(sent.size()==3&&sent[0].id!=sent[1].id&&sent[1].id!=sent[2].id,"请求 ID 独立");
    reply(sent[1],200,R"({"data":[{"id":22,"url":"https://example.test/22"}]})");
    reply(sent[2],200,R"({"data":[]})");
    reply(sent[0],200,R"({"data":[{"id":11,"url":"https://example.test/11"}]})");
    check(ids==QVector<quint64>({102,sent[2].id,101}),"乱序保留原 ID，普通查询也有唯一 ID");
    check(results[0].contains(22)&&results[1].isEmpty()&&results[2].contains(11),"结果不混合");
    api.resolveUrls({44},103);emit transport.requestError(sent.last().id,sent.last().tag,"模拟错误");
    check(ids.last()==103&&results.last().isEmpty()&&errors==1,"网络失败保留 ID");
    api.resolveUrls({55},104);reply(sent.last(),503,{});check(ids.last()==104&&results.last().isEmpty(),"HTTP 失败保留 ID");
    failImmediately=true;api.resolveUrls({66},105);check(ids.last()==105&&errors==2,"同步失败不丢 ID");
    const auto count=ids.size();reply(sent[0],200,"{}");emit transport.requestFinished(NcmRequest::nextId(),"other",200,"{}",{});
    check(ids.size()==count,"重复及其他 API 结果被拒绝");
    Oran7Async::beginShutdown();Oran7Async::waitForDone();std::puts("PASS: 7 network correlation checks");
}
