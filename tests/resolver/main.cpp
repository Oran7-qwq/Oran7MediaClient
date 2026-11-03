#include "Oran7NetworkSongResolver.h"
#include "NcmSongApi.h"
#include "CurlHttpClient.h"
#include "NcmRequest.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <memory>
#include <cstdlib>
#include <cstdio>

// 真实 Resolver + NcmSongApi，仅替换 HTTP 传输，不读取用户 Cookie 或访问网络。
struct Sent { NcmRequest request; quint64 tag; };
static QList<Sent> sent;
static bool synchronousFailure = false;
CurlHttpClient::CurlHttpClient(QObject* parent, const QString&, const QString&) : QObject(parent) {}
CurlHttpClient::~CurlHttpClient() = default;
void CurlHttpClient::send(const NcmRequest& request)
{
    sent.append({request, request.id});
    if (synchronousFailure) emit requestError(request.id, request.tag, QStringLiteral("模拟同步失败"));
}
static int checks = 0;
static void check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
    ++checks;
}
static void wait(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
static void result(CurlHttpClient& http, quint64 tag, qint64 id, const QString& url)
{
    const QJsonObject entry{{"id", id}, {"url", url}};
    emit http.requestFinished(tag, "songurl", 200, QJsonDocument(QJsonObject{{"data", QJsonArray{entry}}}).toJson(), {});
}
struct Events {
    QStringList urls;
    QList<qint64> ids;
    QList<QString> errors;
    void bind(Oran7NetworkSongResolver& resolver) {
        QObject::connect(&resolver, &Oran7NetworkSongResolver::urlReady, &resolver,
            [this](quint64, const QString& url) { urls.append(url); });
        QObject::connect(&resolver, &Oran7NetworkSongResolver::failed, &resolver,
            [this](quint64, qint64 id, const QString& error) { ids.append(id); errors.append(error); });
    }
};
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    CurlHttpClient http;
    NcmSongApi api(&http);
    api.setCookieProvider([] { return QStringLiteral("MUSIC_U=fixture-only"); });
    qint64 now = 1000000;
    Events events;
    Oran7NetworkSongResolver resolver(&api, [&] { return now; });
    events.bind(resolver);
    resolver.resolve(11, {}, false);
    check(events.urls.size() == 1 && events.urls.last().contains("id=11.mp3"), "普通歌立即使用稳定外链");
    check(sent.last().request.headers.value("Cookie") == "MUSIC_U=fixture-only", "真实 API 使用 Cookie provider");
    result(http, sent.last().tag, 11, "https://cdn.test/11");
    check(events.urls.size() == 1, "后台成功只缓存，不重播");
    resolver.resolve(11, {}, true);
    check(events.urls.last() == "https://cdn.test/11" && sent.size() == 1, "20 分钟内缓存复用");
    resolver.playbackOpenFailed(events.urls.last(), 11);
    check(events.urls.size() == 3, "起播失败缓存补救一次");
    resolver.playbackOpenFailed(events.urls.last(), 11);
    check(events.ids.last() == 11 && events.errors.last().isEmpty(), "补救耗尽保留原播放错误");
    resolver.resolve(11, {}, true);
    resolver.playbackOpenFailed(events.urls.last(), 11);
    check(events.urls.size() == 5, "新请求重置起播补救预算");
    now += 20 * 60 * 1000;
    auto count = events.urls.size();
    resolver.resolve(11, {}, true);
    check(events.urls.size() == count && sent.size() == 2, "缓存边界过期，VIP 等待解析");
    const quint64 stale = sent.last().tag;
    resolver.resolve(22, {}, true);
    const quint64 current = sent.last().tag;
    result(http, stale, 11, "https://cdn.test/stale");
    result(http, current, 22, "https://cdn.test/22");
    result(http, current, 22, "https://cdn.test/duplicate");
    check(events.urls.size() == count + 1 && events.urls.last() == "https://cdn.test/22", "乱序与重复结果不能串歌");
    resolver.resolve(33, {}, true);
    const quint64 cancelled = sent.last().tag;
    resolver.cancel();
    count = events.urls.size();
    result(http, cancelled, 33, "https://cdn.test/cancelled");
    emit api.urlsResolved(NcmRequest::nextId(), {{33, "https://cdn.test/query"}});
    check(events.urls.size() == count, "停止与普通查询不触发播放");
    resolver.resolve(44, "https://m701.music.126.net/expired", false);
    check(events.urls.last().contains("id=44.mp3"), "持久化 CDN 地址替换为稳定外链");
    const auto failures = events.ids.size();
    const auto requests = sent.size();
    result(http, sent.last().tag, 44, {});
    check(events.ids.size() == failures, "外链已启动时解析失败不切歌");
    wait(3200);
    check(sent.size() == requests, "外链后台失败不额外重试");
    resolver.resolve(55, {}, true);
    const quint64 attemptOne = sent.last().tag;
    result(http, attemptOne, 55, {});
    const auto beforeRetry = sent.size();
    wait(3200);
    check(sent.size() == beforeRetry + 1 && sent.last().tag != attemptOne, "3 秒后仅重试一次，尝试编号独立");
    result(http, attemptOne, 55, "https://cdn.test/old-attempt");
    const auto failedBefore = events.ids.size();
    result(http, sent.last().tag, 55, {});
    check(events.ids.size() == failedBefore + 1 && !events.errors.last().isEmpty(), "旧尝试不消费重试，二次失败输出错误");
    resolver.resolve(66, {}, true);
    result(http, sent.last().tag, 66, {});
    const auto stopRetryCount = sent.size();
    resolver.retainCurrentSong(22);
    wait(3200);
    check(sent.size() == stopRetryCount, "点回当前歌取消其他歌重试");
    resolver.resolve(77, {}, true);
    const quint64 same = sent.last().tag;
    resolver.retainCurrentSong(77);
    result(http, same, 77, "https://cdn.test/77");
    check(events.urls.last() == "https://cdn.test/77", "同歌暂停保留正在解析的请求");
    resolver.resolve(0, "https://music.163.com/song/media/outer/url?id=88.mp3", false);
    const auto extractedCount = events.urls.size();
    result(http, sent.last().tag, 88, "https://cdn.test/88");
    check(events.urls.size() == extractedCount + 1 && events.urls.last().endsWith("/88"), "旧 URL 提取 ID 后等待解析");
    resolver.resolve(0, "https://cdn.test/unknown", false);
    check(events.urls.last().endsWith("/unknown"), "无 ID 的网络记录直接交付");
    resolver.playbackOpenFailed("https://cdn.test/unknown", 0);
    check(events.ids.last() == 0, "无 ID 打开失败转交自动切歌");
    const auto localFailures = events.ids.size();
    resolver.resolve(0, "C:/fixture/音乐.mp3", false);
    resolver.playbackOpenFailed("C:/fixture/音乐.mp3", 0);
    check(events.urls.last().startsWith("C:/") && events.ids.size() == localFailures, "本地路径不走网络失败流程");
    {
        Events otherEvents;
        Oran7NetworkSongResolver other(&api);
        otherEvents.bind(other);
        resolver.resolve(99, {}, true);
        const quint64 firstTag = sent.last().tag;
        other.resolve(99, {}, true);
        const quint64 secondTag = sent.last().tag;
        result(http, firstTag, 99, "https://cdn.test/first");
        check(otherEvents.urls.isEmpty(), "共享 API 多个 Resolver 的结果互相隔离");
        result(http, secondTag, 99, "https://cdn.test/second");
        check(otherEvents.urls.size() == 1, "独立实例正常消费本实例结果");
    }
    auto transient = std::make_unique<Oran7NetworkSongResolver>(&api);
    transient->resolve(101, {}, true);
    const quint64 destroyedTag = sent.last().tag;
    transient.reset();
    result(http, destroyedTag, 101, "https://cdn.test/destroyed");
    check(true, "销毁后迟到回调安全断开");
    {
        Oran7NetworkSongResolver reentrant(&api);
        const auto before = sent.size();
        QObject::connect(&reentrant, &Oran7NetworkSongResolver::identityResolved, &reentrant,
                         [&] { reentrant.cancel(); });
        reentrant.resolve(102, {}, true);
        check(sent.size() == before, "元数据同步通知触发取消后不提交 HTTP");
    }
    {
        Oran7NetworkSongResolver reentrant(&api);
        const auto before = sent.size();
        QObject::connect(&reentrant, &Oran7NetworkSongResolver::urlReady, &reentrant,
                         [&] { reentrant.cancel(); });
        reentrant.resolve(103, {}, false);
        check(sent.size() == before, "外链同步起播触发取消后不提交 HTTP");
    }
    synchronousFailure = true;
    resolver.resolve(104, {}, true);
    resolver.cancel();
    synchronousFailure = false;
    check(true, "同步传输失败可取消且不丢关联号");
    resolver.resolve(105, {}, true);
    result(http, sent.last().tag, 105, {});
    const auto newRetry = sent.size();
    wait(3200);
    check(sent.size() == newRetry + 1, "新歌曲恢复独立解析重试预算");
    result(http, sent.last().tag, 105, "https://cdn.test/retried");
    check(events.urls.last().endsWith("/retried"), "重试成功交付地址");
    transient = std::make_unique<Oran7NetworkSongResolver>(&api);
    transient->resolve(106, {}, true);
    result(http, sent.last().tag, 106, {});
    const auto destroyedRetry = sent.size();
    transient.reset();
    wait(3200);
    check(sent.size() == destroyedRetry, "析构取消已排队的延迟重试");
    Oran7Async::beginShutdown();
    Oran7Async::waitForDone();
    std::printf("PASS: %d resolver checks\n", checks);
}
