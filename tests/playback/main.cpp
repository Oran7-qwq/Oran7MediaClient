#include "Oran7PlaybackController.h"
#include "Oran7PlaybackState.h"
#include "Oran7MediaPlayer.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>
#include <limits>

static int checks = 0;
static void check(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
    ++checks;
}
static void wait(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
template<class Predicate> static void until(Predicate predicate, const char* description, bool process = true) {
    QElapsedTimer elapsed; elapsed.start();
    while (!predicate() && elapsed.elapsed() < 3000) {
        if (process) QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    check(predicate(), description);
}
struct Oran7Fixture {
    Oran7PlaybackState state;
    int renderStarts = 0, renderStops = 0, accepted = 0, stopLyrics = 0, next = 0, errors = 0, progress = 0;
    bool lastMusic = true;
    Oran7PlaybackController controller{state, {
        [this] { ++renderStarts; return Oran7PlaybackController::FrameCallback{}; },
        [this] { ++renderStops; }, [] { return static_cast<ID3D11Device*>(nullptr); }}};
    Oran7Fixture() {
        QObject::connect(&controller, &Oran7PlaybackController::playOrPauseRequested, &controller, &Oran7PlaybackController::toggle);
        QObject::connect(&controller, &Oran7PlaybackController::stopRequested, &controller, &Oran7PlaybackController::stopCurrent);
        QObject::connect(&controller, &Oran7PlaybackController::mediaAccepted, &controller, [this](const QString&, bool music) { ++accepted; lastMusic = music; });
        QObject::connect(&controller, &Oran7PlaybackController::stopped, &controller, [this](bool switching) { if (!switching) ++stopLyrics; });
        QObject::connect(&controller, &Oran7PlaybackController::playNextTriggered, &controller, [this] { ++next; });
        QObject::connect(&controller, &Oran7PlaybackController::openFailed, &controller, [this] { ++errors; });
        QObject::connect(&controller, &Oran7PlaybackController::playProgressUpdated, &controller, [this] { ++progress; });
    }
};
int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    const auto guiThread = std::this_thread::get_id();
    {
        Oran7Fixture fixture;
        auto& c = fixture.controller;
        check(c.seekTo(1) == -1, "无播放器 seek 失败");
        c.setFocusedSource("restore.mp3");
        check(!c.hasPlayer() && c.source() == "restore.mp3", "恢复聚焦不启动播放器");
        c.setVolume(35);
        c.prepare("a.mp3");
        auto first = sessions.back();
        check(c.hasPlayer() && c.playingSource() == "a.mp3" && fixture.state.state() == Oran7PlaybackState::Opening, "准备本地媒体并创建实例");
        check(first->frameCallbacks == 1 && first->devices == 1 && first->volume == 44, "初始化帧回调、设备和持久默认音量");
        first->push(FFP_MSG_FIND_STREAM_INFO);
        until([&] { return first->durationReads > 0; }, "消息线程读取所属实例时长", false);
        first->duration = 240000;
        until([&] { return fixture.state.durationMs() == 120000; }, "GUI 使用发生时的时长快照");
        first->push(FFP_MSG_PREPARED);
        until([&] { return fixture.state.state() == Oran7PlaybackState::Playing; }, "prepared 后自动开始");
        c.toggle();
        until([&] { return fixture.state.state() == Oran7PlaybackState::Paused; }, "暂停状态由 native 消息同步");
        c.toggle();
        until([&] { return fixture.state.state() == Oran7PlaybackState::Playing; }, "继续状态由 native 消息同步");
        check(c.seekTo(999) == 0 && first->seek == 120000 && fixture.state.state() == Oran7PlaybackState::Seeking, "seek 限制在已知时长内");
        const auto progressBeforeSeek = fixture.progress;
        wait(150);
        check(fixture.progress == progressBeforeSeek, "seek 中不被进度定时器覆盖");
        first->push(FFP_MSG_SEEK_COMPLETE);
        until([&] { return fixture.state.state() == Oran7PlaybackState::Playing; }, "seek 完成恢复播放态");
        until([&] { return fixture.progress > progressBeforeSeek; }, "100ms 进度正常更新");
        first->position = std::numeric_limits<double>::quiet_NaN();
        const auto invalidPosition = fixture.state.positionMs();
        wait(140);
        check(fixture.state.positionMs() == invalidPosition, "NaN 进度被过滤");
        c.setVolume(200);
        check(fixture.state.volume() == 100 && first->volume == 100, "音量范围归一且传给当前实例");
        const auto created = sessions.size();
        c.prepare("b.mp3");
        check(!c.hasPlayer() && fixture.stopLyrics == 0, "内部切歌释放旧实例，保留新歌词");
        c.prepare("c.mp4", false);
        wait(370);
        check(sessions.size() == created + 1 && c.playingSource() == "c.mp4" && !fixture.lastMusic, "连续请求只创建最后目标，保留媒体类型");
        until([&] { return first->released.load(); }, "旧实例后台释放完成");
        check(first->releaseThread != guiThread, "退役释放不阻塞 GUI");
        auto second = sessions.back();
        c.toggle();
        check(fixture.state.state() == Oran7PlaybackState::Paused, "准备期间暂停意图被记录");
        second->push(FFP_MSG_PREPARED);
        until([&] { return second->pauses > 0; }, "prepared 执行暂停意图");
        check(second->starts == 0, "准备期间暂停不短暂起播");
        wait(110);
        c.prepare("d.mp3");
        const auto beforeStop = sessions.size();
        c.stop();
        wait(370);
        check(!c.hasPlayer() && sessions.size() == beforeStop && fixture.state.state() == Oran7PlaybackState::Idle, "主动停止取消延迟起播");
        c.prepare("error.mp3");
        auto errorPlayer = sessions.back();
        errorPlayer->push(FFP_MSG_ERROR, -403);
        until([&] { return fixture.errors == 1; }, "打开失败通知 Resolver 门面");
        check(fixture.state.state() == Oran7PlaybackState::Error, "打开失败写入 Error 状态");
        errorPlayer->push(FFP_MSG_PLAY_FNISH);
        until([&] { return fixture.next == 1; }, "EOF 停止实例并通知下一首一次");
        check(fixture.state.state() == Oran7PlaybackState::Ended && !c.hasPlayer(), "EOF 保留 Ended 状态");
        c.prepare("stale.mp3");
        until([&] { return c.hasPlayer(); }, "EOF 旧实例释放后创建新实例");
        auto stale = sessions.back();
        const int consumed = freedMessages;
        stale->push(FFP_MSG_PLAY_FNISH);
        stale->push(FFP_MSG_ERROR, -1);
        until([&] { return freedMessages >= consumed + 2; }, "旧消息已进入 GUI 队列", false);
        c.stop();
        wait(60);
        check(fixture.next == 1 && fixture.errors == 1 && fixture.state.state() == Oran7PlaybackState::Idle, "停止后丢弃旧 EOF/错误");
        c.prepare("shutdown.mp3");
        wait(110);
        c.prepare("never.mp3");
        const auto beforeShutdown = sessions.size();
        c.shutdown();
        c.prepare("ignored.mp3");
        c.toggle();
        wait(370);
        check(c.isShuttingDown() && !c.hasPlayer() && sessions.size() == beforeShutdown, "退出阻止定时器与后续命令起播");
    }
    {
        Oran7Fixture fixture;
        auto& c = fixture.controller;
        c.prepare("first.mp3");
        auto first = sessions.back();
        first->allowRelease = false;
        c.stop();
        c.prepare("second.mp3");
        const auto count = sessions.size();
        int beats = 0;
        QTimer heartbeat;
        QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++beats; });
        heartbeat.start(5);
        for (int i = 0; i < 30; ++i) {
            c.stop(); // 与门面上一曲的强制重播顺序一致。
            c.prepare(QString("rapid-%1.mp3").arg(i));
            wait(5);
        }
        wait(360); // 超过旧固定 300ms 切歌等待，旧实例仍被测试闸门挡住。
        check(sessions.size() == count && !c.hasPlayer() && beats > 20,
            "快速上一曲在旧实例未释放时不堆积播放器且 GUI 事件循环持续推进");
        int removed = 0;
        c.whenPlayersReleased([&] { ++removed; });
        check(removed == 0, "旧文件句柄未释放时不执行文件任务");
        first->unblock();
        until([&] { return removed == 1; }, "所有句柄释放后再调用文件任务");
        until([&] { return c.hasPlayer(); }, "后台退役完成后自动消费最后目标");
        check(sessions.size() == count + 1 && c.playingSource() == "rapid-29.mp3",
            "连续上一曲仅创建最后一首的播放器");
        auto second = sessions.back();
        second->allowRelease = false;
        c.stop();
        c.prepare("cancel-while-retiring.mp3");
        c.stop();
        second->unblock();
        until([&] { return second->released.load(); }, "第二实例后台释放");
        wait(360);
        check(!c.hasPlayer() && sessions.size() == count + 1, "等待退役期间主动停止不被迟到回调重新起播");
    }
    {
        auto fixture = std::make_unique<Oran7Fixture>();
        fixture->controller.prepare("destroy.mp3");
        auto retiring = sessions.back();
        retiring->allowRelease = false;
        fixture->controller.stop();
        until([&] { return retiring->releaseStarted.load(); }, "后台销毁已开始", false);
        fixture.reset();
        const auto consumed = freedMessages.load();
        retiring->push(FFP_MSG_ERROR, -9);
        until([&] { return freedMessages > consumed; }, "服务销毁后消息线程仍可安全投递和释放消息", false);
        retiring->unblock();
        until([&] { return retiring->released.load(); }, "服务销毁后后台播放器独立释放");
    }
    {
        Oran7Fixture fixture;
        failCreate = true;
        fixture.controller.prepare("fail-create.mp3");
        auto failed = sessions.back();
        failCreate = false;
        check(!fixture.controller.hasPlayer() && fixture.state.state() == Oran7PlaybackState::Error, "create 失败不保留半初始化实例");
        until([&] { return failed->released.load(); }, "create 失败资源释放");
        check(failed->pauses == 0, "未初始化成功的实例不投递 pause");
        failPrepare = true;
        fixture.controller.prepare("fail-prepare.mp3");
        failed = sessions.back();
        failPrepare = false;
        until([&] { return failed->released.load(); }, "prepare 失败也 join 消息线程并释放");
    }
    {
        Oran7Fixture fixture;
        fixture.controller.prepare("reentrant.mp3");
        wait(110);
        bool nested = false;
        QObject::connect(&fixture.controller, &Oran7PlaybackController::stopped, &fixture.controller, [&](bool switching) {
            if (switching && !nested) { nested = true; fixture.controller.prepare("nested.mp3"); }
        });
        fixture.controller.prepare("superseded.mp3");
        wait(370);
        check(nested && fixture.controller.playingSource() == "nested.mp3", "停止旧实例的同步通知重入不丢切歌定时器");
        fixture.controller.stop();
    }
    Oran7Async::waitForDone();
    QCoreApplication::processEvents();
    for (const auto& session : sessions) check(session->released.load(), "所有测试实例均已释放");
    Oran7Async::beginShutdown();
    std::printf("PASS: %d playback checks, %zu native instances released\n", checks, sessions.size());
}
