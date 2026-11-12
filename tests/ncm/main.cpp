#include "Oran7NcmJsonMapper.h"
#include "NcmSongApi.h"
#include "NcmPlaylistApi.h"
#include "NcmUserApi.h"
#include "CurlHttpClient.h"
#include "NcmProtocol.h"
#include "BlockingCurl.h"
#include "NcmAsyncHandlers.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QSet>
#include <thread>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static int checks=0;
static void check(bool ok,const char* text) { ++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);} }
static QList<NcmRequest> sent;
static bool immediateFailure=false;
CurlHttpClient::CurlHttpClient(QObject* parent,const QString&, const QString&) : QObject(parent) {}
CurlHttpClient::~CurlHttpClient() = default;
void CurlHttpClient::send(const NcmRequest& request) {
    sent.append(request);
    if(immediateFailure) emit requestError(request.id,request.tag,"fixture failure");
}
// 加密实现保持生产代码不变；这里只替代协议加密，以检查业务请求的 payload。
NcmRequest NcmProtocol::prepareWeapi(const QString& path,const QJsonObject& payload) const {
    NcmRequest r;r.url=QUrl("https://music.163.com"+path);r.tag=r.url.path();r.body=QJsonDocument(payload).toJson();return r;
}
struct Oran7Fixture { QString path;QByteArray body;int status=200;bool ok=true; };
static QList<Oran7Fixture> fixtures;
static QList<NcmRequest> blockingSent;
namespace ncm {
bool blockingCurl(const NcmRequest& request,QByteArray* body,int* status,QString* error,int) {
    check(!fixtures.isEmpty(),"没有意外 HTTP 请求");const auto f=fixtures.takeFirst();
    check(request.url.path()==f.path,"HTTP 阶段与路径正确");
    check(request.id>0&&!request.tag.isEmpty(),"阻塞请求同样有 ID/tag");blockingSent.append(request);
    if(body)*body=f.body;if(status)*status=f.status;if(error)*error=f.ok?QString():"fixture failure";return f.ok;
}
}
static void reply(CurlHttpClient& transport,NcmRequest request,QByteArray body,int status=200) {
    emit transport.requestFinished(request.id,request.tag,status,body,{});
}
static QJsonObject parse(const char* text){return QJsonDocument::fromJson(text).object();}
static void push(const char* path,const char* body,int status=200,bool ok=true){fixtures.append({path,body,status,ok});}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const auto legacy=Oran7NcmJsonMapper::track(parse(R"({"id":11,"name":"曲目","artists":[{"name":"甲","img1v1Url":"artist-cover"},{"name":"乙"}],"album":{"name":"专辑"},"duration":123456,"fee":1})"));
    check(legacy&&legacy->songId==11&&legacy->artist=="甲 / 乙"&&legacy->isVip,"旧字段解析与 fee==1");
    check(legacy->cover=="artist-cover"&&legacy->durationMs==123456,"搜索封面兜底与毫秒精度");
    const auto modern=Oran7NcmJsonMapper::track(parse(R"({"id":12,"name":"新字段","ar":[{"name":"甲"}],"al":{"picUrl":"album-cover"},"album":{"name":"兜底专辑"},"dt":60001,"fee":4})"));
    check(modern&&modern->album=="兜底专辑"&&modern->cover=="album-cover"&&!modern->isVip,"新旧专辑字段分别兜底，fee 规则一致");
    const auto nested=Oran7NcmJsonMapper::track(parse(R"({"id":13,"songInfo":{"name":"嵌套","duration":-1}})"));
    check(nested&&nested->songId==13&&nested->durationMs==0,"漫游嵌套 ID 与负时长");
    check(!Oran7NcmJsonMapper::track({}),"无身份曲目拒绝");
    auto filled=*nested;auto detail=*legacy;detail.songId=13;filled.completeMissing(detail);
    check(filled.title=="嵌套"&&filled.artist==legacy->artist&&filled.isVip,"补空不覆盖已有标题，详情 fee 生效");
    auto other=*modern;filled.completeMissing(other);check(filled.songId==13&&filled.title=="嵌套","不同身份不补全");
    auto dto=filled.toVariantMap();check(dto.value("title")=="嵌套"&&dto.value("timesize")=="123"&&dto.contains("durationMs")&&!dto.contains("music_name"),"统一边界字段，不再分长短三种 DTO");
    check(Oran7NcmJsonMapper::playbackUrls(parse(R"({"data":[{"id":1,"url":"u"},{"id":0,"url":"bad"},{"id":2,"url":null}]})")).size()==1,"播放 URL 无效条目过滤");
    std::mutex mutex;QSet<quint64> allocated;
    auto allocate=[&]{for(int i=0;i<500;++i){auto id=NcmRequest::nextId();std::lock_guard<std::mutex> lock(mutex);allocated.insert(id);}};
    std::thread a(allocate),b(allocate);a.join();b.join();check(allocated.size()==1000,"多线程请求 ID 唯一");

    CurlHttpClient transport;NcmPlaylistApi playlist(&transport);NcmUserApi user(&transport);NcmSongApi song(&transport);
    QString cookie="fixture-A";playlist.setCookieProvider([&]{return cookie;});user.setCookieProvider([&]{return cookie;});
    QSignalSpy likes(&playlist,&NcmPlaylistApi::likeResult),orders(&playlist,&NcmPlaylistApi::reorderResult);
    QSignalSpy accounts(&user,&NcmUserApi::accountFetched),verifies(&user,&NcmUserApi::verifyResult),errors(&user,&NcmUserApi::apiError);
    QSignalSpy urls(&song,&NcmSongApi::urlsResolved);
    const auto likeA=playlist.setLike(11,true);const auto reqA=sent.last();
    const auto likeB=playlist.setLike(12,false);const auto reqB=sent.last();
    reply(transport,reqB,R"({"code":200})");reply(transport,reqA,R"({"code":500})");
    check(likes.size()==2&&likes[0][0].toULongLong()==likeB&&likes[0][1].toLongLong()==12&&!likes[0][2].toBool()&&likes[0][3].toBool(),"乱序取消喜欢保留原 songId/value/ID");
    check(likes[1][0].toULongLong()==likeA&&likes[1][1].toLongLong()==11&&likes[1][2].toBool()&&!likes[1][3].toBool(),"旧点赞结果不借用最后一条操作身份");
    reply(transport,reqA,R"({"code":200})");check(likes.size()==2,"写操作重复回调忽略");
    check(QJsonDocument::fromJson(reqB.body).object().value("trackId").toVariant().toLongLong()==12,"请求 payload 与身份匹配");
    playlist.setLikedPlaylistId(99);const auto oldOrder=playlist.reorderLikedSongs({11,12});const auto firstOrder=sent.last();
    const auto latestOrder=playlist.reorderLikedSongs({12,11});const auto lastOrder=sent.last();
    reply(transport,firstOrder,R"({"code":500})");check(orders.isEmpty()&&oldOrder!=latestOrder,"旧排序响应不会刷新新顺序");
    reply(transport,lastOrder,R"({"code":200})");check(orders.size()==1&&orders[0][0].toULongLong()==latestOrder&&orders[0][1].toBool(),"排序结果返回最新请求 ID");
    playlist.setLike(99,true);const auto previousAccount=sent.last();cookie="fixture-B";reply(transport,previousAccount,R"({"code":200})");check(likes.size()==2,"换账号后写操作旧回调拒绝");
    const auto accountId=NcmRequest::nextId();user.fetchAccount(accountId);const auto accountReq=sent.last();
    emit transport.requestError(accountReq.id,"songurl","wrong consumer");check(errors.isEmpty(),"跨 API 错误不广播到账号消费者");
    reply(transport,accountReq,R"({"code":200,"profile":{"nickname":"昵称"}})");check(accounts.size()==1&&accounts[0][0].toULongLong()==accountId&&accounts[0][1]=="昵称","账号结果携带原请求 ID");
    const auto verifyId=NcmRequest::nextId();user.verifyLogin(verifyId);reply(transport,sent.last(),R"({"code":200,"profile":null})");check(verifies.size()==1&&verifies[0][0].toULongLong()==verifyId&&verifies[0][1].toInt()==0,"无效登录与网络失败区分");
    immediateFailure=true;playlist.setLike(13,true);user.verifyLogin();song.resolveUrls({11});immediateFailure=false;
    check(likes.size()==3&&!likes.last()[3].toBool()&&verifies.last()[1].toInt()==-1&&urls.size()==1,"同步传输失败结束各自请求");
    NcmSongApi another(&transport);QSignalSpy foreign(&another,&NcmSongApi::urlsResolved);
    const auto songId=NcmRequest::nextId();song.resolveUrls({42},songId);const auto songReq=sent.last();reply(transport,songReq,R"({"data":[{"id":42,"url":"https://cdn/42"}]})");
    check(urls.last()[0].toULongLong()==songId&&foreign.isEmpty(),"共享传输的多个歌曲 API 实例隔离");

    push("/api/search/get",R"({"code":200,"result":{"songs":[{"id":1,"name":"搜索","artists":[{"name":"歌手","img1v1Url":"fallback"}],"duration":123456,"fee":1}]}})");
    push("/api/song/detail",R"({"songs":[{"id":1,"al":{"name":"专辑","picUrl":"detail-cover"}}]})");
    push("/api/song/enhance/player/url",R"({"data":[{"id":1,"url":"https://cdn/1"}]})");
    const auto search=ncm::musicSearchHandler({{"keywords","中文"},{"cookie","fixture"},{"isLoggedIn",true}}).toList();
    check(search.size()==1&&search[0].toMap().value("title")=="搜索"&&search[0].toMap().value("cover")=="detail-cover"&&search[0].toMap().value("path")=="https://cdn/1","真实搜索 handler 输出统一 DTO 与播放地址");
    check(!blockingSent[0].headers.contains("Cookie")&&!blockingSent[1].headers.contains("Cookie")&&blockingSent[2].headers.value("Cookie").contains("os=pc"),"搜索/详情不带 Cookie，VIP 地址保留 pc 指纹");
    push("/api/search/get",R"({"code":200,"result":{"songs":[{"id":2,"name":"兜底","artists":[{"name":"歌手","img1v1Url":"fallback"}],"fee":0}]}})");push("/api/song/detail","",503);
    const auto fallback=ncm::musicSearchHandler({{"keywords","词"}}).toList()[0].toMap();check(fallback.value("cover")=="fallback"&&fallback.value("path").toString().contains("id=2.mp3"),"封面失败与未登录时沿用外链兜底");
    auto likedPrefix=[] {
        push("/api/nuser/account/get",R"({"profile":{"userId":7}})");
        push("/api/user/playlist",R"({"playlist":[{"id":99,"specialType":5}]})");
    };
    likedPrefix();push("/api/v6/playlist/detail",R"({"playlist":{"trackIds":[{"id":2},{"id":1}]}})");
    push("/api/song/detail",R"({"songs":[{"id":1,"name":"一","artists":[{"name":"甲"}],"album":{"name":"专辑"},"fee":1},{"id":2,"name":"二","ar":[{"name":"乙"}],"al":{"picUrl":"cover"},"dt":99999,"fee":0}]})");
    const auto liked=ncm::fetchLikedSongsHandler({{"cookie","fixture"}}).toMap();const auto songs=liked.value("songs").toList();
    check(liked.value("playlistId")==99&&songs[0].toMap().value("songId")==2&&songs[1].toMap().value("isVip").toBool(),"收藏按 trackIds 顺序输出相同 DTO");
    likedPrefix();push("/api/v6/playlist/detail",R"({"playlist":{"trackIds":[]}})");
    check(ncm::fetchLikedSongsHandler({}).toMap().value("songs").toList().isEmpty(),"空歌单是成功结果，不造成反复失败重试");
    push("/weapi/playmode/intelligence/list",R"({"code":200,"data":[{"id":1,"songInfo":{"name":"保留标题","ar":[{"name":"歌手"}],"al":{"picUrl":"原封面"},"fee":0}},{"id":2},{"id":3}]})");
    push("/api/song/detail",R"({"songs":[{"id":2,"name":"补全","artists":[{"name":"甲"}],"album":{"picUrl":"补图"},"duration":60000,"fee":1}]})");
    const auto roam=ncm::roamSongsHandler({{"songId",1},{"playlistId",99},{"cookie","fixture"}}).toList();
    check(roam.size()==2&&roam[0].toMap().value("title")=="保留标题"&&roam[1].toMap().value("title")=="补全"&&roam[1].toMap().value("isVip").toBool(),"漫游嵌套解析、补空、丢弃无标题，统一字段");
    check(!blockingSent[blockingSent.size()-2].headers.value("Cookie").contains("os=pc"),"漫游保持基础 Cookie，不注入 pc 指纹");
    likedPrefix();push("/api/v6/playlist/detail",R"({"code":301})");
    bool invalidPlaylist=false;try {ncm::fetchLikedSongsHandler({});}catch(const std::exception&){invalidPlaylist=true;}
    check(invalidPlaylist,"错误响应不能误判为空歌单成功");
    likedPrefix();push("/api/v6/playlist/detail",R"({"playlist":{"trackIds":[{"id":1}]}})");push("/api/song/detail","",503);
    bool noDetails=false;try {ncm::fetchLikedSongsHandler({});}catch(const std::exception&){noDetails=true;}
    check(noDetails,"全部详情失败不能用空结果覆盖缓存");
    for (int mode=4;mode<=6;++mode) {
        push("/weapi/v1/radio/get",R"({"code":200,"data":[{"id":1099511627776,"name":"FM","ar":[{"name":"artist"}],"al":{"picUrl":"cover"},"dt":60000}]})");
        const auto fm=ncm::roamSongsHandler({{"recommendationMode",mode},{"cookie","fixture"}}).toList();
        const auto payload=QJsonDocument::fromJson(blockingSent.last().body).object();
        check(fm.size()==1 && fm[0].toMap().value("songId").toLongLong()==(qint64(1)<<40),"FM 无种子请求保留 64 位歌曲 ID");
        check(payload.value("mode").toString()==(mode==4?"DEFAULT":mode==5?"FAMILIAR":"EXPLORE") && !payload.contains("songId"),"FM 取向独立路由，未借用心动参数");
    }
    check(fixtures.isEmpty(),"全部 HTTP fixture 被准确消费");
    std::printf("PASS: %d NCM mapper/request/API/handler checks\n",checks);
}
