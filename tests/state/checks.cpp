#include "Oran7PlaybackState.h"
#include "Oran7Async.h"
#include "Oran7AsyncManager.h"
#include "NcmAsyncHandlers.h"
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QThread>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

// 替换网络处理器，测试不访问真实网络、曲库或登录凭据。
namespace ncm {
QVariant musicSearchHandler(const QVariantMap&) { return {}; }
QVariant fetchLikedSongsHandler(const QVariantMap&) { return {}; }
QVariant roamSongsHandler(const QVariantMap&) { return {}; }
}
static void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
template<class Predicate> static void until(Predicate predicate) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(); QThread::msleep(1);
    }
    check(predicate(), "等待条件超时");
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QObject context;
    Oran7PlaybackState state;
    int volumeSignals = 0;
    QObject::connect(&state, &Oran7PlaybackState::volumeChanged, [&] { ++volumeSignals; });
    check(state.state() == Oran7PlaybackState::Idle && !state.playing(), "初始为空闲");
    state.beginOpening("test.mp3");
    check(state.active() && !state.playing() && state.source() == "test.mp3", "打开状态与媒体源");
    state.setDuration(10000); state.setState(Oran7PlaybackState::Playing);
    state.beginSeek(15000);
    check(state.state() == Oran7PlaybackState::Seeking && state.playing() && state.positionMs() == 10000, "播放中 Seek 与越界裁剪");
    state.finishSeek(); check(state.state() == Oran7PlaybackState::Playing, "Seek 后恢复播放");
    state.setState(Oran7PlaybackState::Paused); state.beginSeek(500);
    check(!state.playing(), "暂停 Seek 不改变播放语义");
    state.finishSeek(); check(state.state() == Oran7PlaybackState::Paused, "Seek 后仍暂停");
    state.setVolume(200); state.setVolume(100); state.setVolume(-10);
    check(state.volume() == 0 && volumeSignals == 2, "音量校验和通知去重");
    state.setMode(3); state.setMode(99); check(state.mode() == 3, "漫游模式及非法模式");
    state.setError(QStringLiteral("模拟错误"));
    check(state.state() == Oran7PlaybackState::Error && !state.playing(), "错误使播放态结束");
    state.beginOpening("next.mp3");
    check(state.errorString().isEmpty() && state.durationMs() == 0 && state.positionMs() == 0, "新媒体清除错误和旧进度");

    int trackSignals = 0;
    QObject::connect(&state, &Oran7PlaybackState::currentTrackChanged, [&] { ++trackSignals; });
    Oran7Track track{"", QStringLiteral("会员歌曲"), QStringLiteral("歌手"), "", "cover", "", 123, true};
    state.setCurrentTrack(track);
    const auto firstRevision = state.trackRevision();
    state.setCurrentTrack(track);
    check(trackSignals == 1 && state.trackRevision() == firstRevision, "完整快照通知去重，空 URL 的 VIP 信息有效");
    check(state.completeTrackDetails(firstRevision, "album", "120"), "补全当前版本");
    check(state.currentTrack().album == "album" && state.currentTrack().timesize == "120", "缺失元数据补全");
    const auto oldRevision = state.trackRevision();
    state.setCurrentTrack({"local.mp3", "local", "artist", "", "", "5", -1, false});
    check(state.currentTrack().songId == 0 && !state.currentTrack().isVip, "本地歌清除网络身份");
    check(!state.completeTrackDetails(oldRevision, "stale", "999") && state.currentTrack().title == "local", "旧补全结果不覆盖新歌");
    state.setCurrentTrack(track);
    check(!state.completeTrackDetails(firstRevision, "stale", "999"), "切走再切回相同 songId 也拒绝过期结果");
    state.completeTrackDetails(state.trackRevision(), "album", "120");
    state.completeTrackDetails(state.trackRevision(), "overwrite", "999");
    check(state.currentTrack().album == "album" && state.currentTrack().timesize == "120", "补全不覆盖已有元数据");

    state.selectTrack(track);
    check(state.currentTrack().album == "album" && state.currentTrack().timesize == "120", "再次选择同一首保留已经补全的详情");
    state.selectTrack({"b.mp3", "other", "", "", "", "", 0, false});
    check(state.currentTrack().album.isEmpty() && state.currentTrack().timesize.isEmpty(), "不同歌曲不继承旧详情");
    state.setCurrentTrack(track);
    Oran7AsyncManager* manager = nullptr;
    qmlRegisterUncreatableType<Oran7PlaybackState>("Client", 1, 0, "Oran7PlaybackState", "只读状态");
    qmlRegisterSingletonType<Oran7AsyncManager>("Oran7Async", 1, 0, "Oran7AsyncManager",
        [&manager](QQmlEngine*, QJSEngine*) -> QObject* {
            manager = new Oran7AsyncManager;
            manager->registerHandler("p1Test", [](const QVariantMap&) -> QVariant { return 42; });
            return manager;
        });
    {
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("testPlayback", &state);
        QQmlComponent component(&engine);
        component.setData(R"(import QtQml
import Client 1.0
import Oran7Async 1.0
QtObject {
    readonly property bool active: testPlayback.active
    readonly property int volume: testPlayback.volume
    readonly property string title: testPlayback.currentTrack.title
    readonly property string cover: testPlayback.currentTrack.cover
    readonly property double songId: testPlayback.currentTrack.songId
    readonly property bool vip: testPlayback.currentTrack.isVip
    function attemptTrackWrite() { try { testPlayback.currentTrack.title = "bad" } catch (error) { return false }; return testPlayback.currentTrack.title === "bad" }
    function attemptStateWrite() { try { testPlayback.currentTrack = ({}) } catch (error) { return false }; return testPlayback.currentTrack.title === undefined }
    readonly property var async: Oran7AsyncManager
})", QUrl());
        auto* root = component.create();
        if (!root) { std::fprintf(stderr, "%s\n", qPrintable(component.errorString())); return 1; }
        check(root->property("active").toBool(), "QML 读取状态对象");
        check(root->property("title").toString() == QStringLiteral("会员歌曲") && root->property("songId").toLongLong() == 123
            && root->property("vip").toBool(), "QML 读取 gadget 完整快照");
        state.setCurrentTrack({"a.mp3", "same-cover-new-title", "new-artist", "", "cover", "9", 0, false});
        check(root->property("title").toString() == "same-cover-new-title" && !root->property("vip").toBool(), "同封面更新标题和身份");
        QVariant writable;
        check(QMetaObject::invokeMethod(root, "attemptTrackWrite", Q_RETURN_ARG(QVariant, writable))
            && !writable.toBool() && state.currentTrack().title == "same-cover-new-title", "QML 无法写入快照字段");
        check(QMetaObject::invokeMethod(root, "attemptStateWrite", Q_RETURN_ARG(QVariant, writable))
            && !writable.toBool() && state.currentTrack().title == "same-cover-new-title", "QML 无法整体替换只读快照");
        auto savedTrack = state.currentTrack(); state.beginOpening("resolved-cdn-url");
        check(state.currentTrack() == savedTrack && state.source() == "resolved-cdn-url", "实际解码地址与音乐入口路径独立");
        state.setState(Oran7PlaybackState::Paused); state.setVolume(37);
        check(!root->property("active").toBool() && root->property("volume").toInt() == 37, "QML 绑定随状态变化");
        engine.globalObject().setProperty("p1Reply", -1);
        auto completed = engine.evaluate("(function(value) { p1Reply = value; })");
        int taskId = manager->run("p1Test", {}, completed);
        check(taskId > 0, "QML 任务返回编号");
        until([&] { return engine.globalObject().property("p1Reply").toInt() == 42; });
        check(!manager->cancel(taskId), "已结束任务移除句柄");
        manager->registerHandler("p1Cancel", [](const QVariantMap&) -> QVariant { QThread::msleep(30); return 99; });
        taskId = manager->run("p1Cancel", {}, completed);
        check(manager->cancel(taskId), "QML 任务可取消");
        Oran7Async::waitForDone(); QCoreApplication::processEvents();
        check(engine.globalObject().property("p1Reply").toInt() == 42, "取消后不调用 QML 完成函数");
        manager->run("p1Cancel", {}, completed); manager->run("p1Cancel", {}, completed);
        manager->cancelAll(); Oran7Async::waitForDone(); QCoreApplication::processEvents();
        check(engine.globalObject().property("p1Reply").toInt() == 42, "批量取消抑制所有 QML 回调");
        manager->registerHandler("p1Failure", [](const QVariantMap&) -> QVariant { throw std::runtime_error("模拟失败"); });
        engine.globalObject().setProperty("p1Failed", false);
        auto failed = engine.evaluate("(function(error) { p1Failed = true; })");
        manager->run("p1Failure", {}, completed, failed);
        until([&] { return engine.globalObject().property("p1Failed").toBool(); });
        engine.globalObject().setProperty("p1Failed", false);
        check(manager->run("unknown", {}, completed, failed) == -1
            && engine.globalObject().property("p1Failed").toBool(), "未知操作立即失败");
        delete root;
    }

    bool valueDone = false, voidDone = false, moveDone = false;
    Oran7Async::run(&context, [] { return std::make_unique<int>(9); }, [&](std::unique_ptr<int> value) { moveDone = value && *value == 9; });
    Oran7Async::run(&context, [] { return 7; }, [&](int value) {
        check(QThread::currentThread() == context.thread() && value == 7, "完成回调回到所属线程"); valueDone = true;
    });
    Oran7Async::run(&context, [] {}, [&] { voidDone = true; });
    until([&] { return valueDone && voidDone && moveDone; });

    QSemaphore blocked, release;
    std::atomic<int> active{0}, peak{0};
    for (int i = 0; i < 2; ++i) Oran7Async::run(&context, [&] {
        int now = ++active; int old = peak.load(); while (old < now && !peak.compare_exchange_weak(old, now)) {}
        blocked.release(); release.acquire(); --active;
    }, [] {}, Oran7Async::Group::Network);
    check(blocked.tryAcquire(2, 3000), "两个网络任务可以并发");
    std::atomic_bool canceledWork{false}; bool canceledCallback = false;
    auto canceled = Oran7Async::run(&context, [&] { canceledWork = true; }, [&] { canceledCallback = true; }, Oran7Async::Group::Network);
    canceled.cancel();
    bool generalDone = false;
    Oran7Async::run(&context, [] { return true; }, [&](bool value) { generalDone = value; });
    until([&] { return generalDone; });
    check(peak == 2 && !canceledWork.load(), "网络并发上限与通用任务独立推进");
    release.release(2); Oran7Async::waitForDone(); QCoreApplication::processEvents();
    check(!canceledWork && !canceledCallback, "取消排队任务跳过执行和回调");

    QSemaphore running, finish;
    bool activeCallback = false;
    auto activeTask = Oran7Async::run(&context, [&] { running.release(); finish.acquire(); return 1; }, [&](int) { activeCallback = true; });
    check(running.tryAcquire(1, 3000), "运行中的任务已开始");
    activeTask.cancel(); finish.release(); Oran7Async::waitForDone(); QCoreApplication::processEvents();
    check(!activeCallback, "取消运行中任务不调用界面");

    auto* transient = new QObject;
    bool destroyedCallback = false;
    Oran7Async::run(transient, [&] { running.release(); finish.acquire(); }, [&] { destroyedCallback = true; });
    check(running.tryAcquire(1, 3000), "生命周期测试任务已开始");
    delete transient; finish.release(); Oran7Async::waitForDone(); QCoreApplication::processEvents();
    check(!destroyedCallback, "上下文销毁后无回调");

    bool exceptionCallback = false;
    Oran7Async::run(&context, []() -> int { throw std::runtime_error("模拟异常"); }, [&](int) { exceptionCallback = true; });
    Oran7Async::waitForDone(); QCoreApplication::processEvents();
    check(!exceptionCallback, "工作异常不调用成功回调");
    std::atomic_bool shutdownWork{false}; bool shutdownCallback = false;
    Oran7Async::run(&context, [&] { shutdownWork = true; }, [&] { shutdownCallback = true; });
    Oran7Async::beginShutdown(); Oran7Async::waitForDone(); QCoreApplication::processEvents();
    check(shutdownWork && !shutdownCallback, "退出排空已接受工作并抑制回调");
    check(!Oran7Async::run(&context, [] {}, [] {}).isValid(), "退出后拒绝新任务");
    check(!Oran7Async::runDetached([] {}).isValid(), "退出后拒绝新分离任务");
    std::puts("PASS: playback state, QML bindings, shared scheduler, cancellation, lifecycle, shutdown");
}
