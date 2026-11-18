#include "Oran7SdlAudioDevice.h"
#include <SDL2/SDL.h>
#include <QCoreApplication>
#include <QProcess>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <future>
#include <vector>

namespace {
struct Oran7DeviceAudit {
    std::vector<std::thread::id> owners;
    std::promise<void> opening, closing, allowOpen, allowClose;
    std::shared_future<void> openGate = allowOpen.get_future().share();
    std::shared_future<void> closeGate = allowClose.get_future().share();
    bool slowOpen = true, slowClose = true, mta = false;
    int initializations = 0, releases = 0;
};
Oran7DeviceAudit* audit = nullptr;
int SDLCALL auditInitialize(Uint32) {
    audit->owners.push_back(std::this_thread::get_id());
    ++audit->initializations;
    using Query = HRESULT (WINAPI*)(int*, int*);
    auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ole32.dll"), "CoGetApartmentType"));
    int apartment = -1, qualifier = -1;
    audit->mta = query && SUCCEEDED(query(&apartment, &qualifier)) && apartment == 1;
    return 0;
}
void SDLCALL auditQuit(Uint32) {
    audit->owners.push_back(std::this_thread::get_id()); ++audit->releases;
}
SDL_AudioDeviceID SDLCALL auditOpen(const char*, int, const SDL_AudioSpec* wanted, SDL_AudioSpec* obtained, int) {
    audit->owners.push_back(std::this_thread::get_id());
    if (audit->slowOpen) { audit->opening.set_value(); audit->openGate.wait(); }
    if (wanted->format == AUDIO_F32SYS) { SDL_SetError("synthetic format rejection"); return 0; }
    *obtained = *wanted;
    return 17;
}
void SDLCALL auditPause(SDL_AudioDeviceID, int) { audit->owners.push_back(std::this_thread::get_id()); }
void SDLCALL auditClose(SDL_AudioDeviceID) {
    audit->owners.push_back(std::this_thread::get_id());
    if (audit->slowClose) { audit->closing.set_value(); audit->closeGate.wait(); }
}
}

static int checks = 0;
static void check(bool ok, const char* message)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s: %s\n", message, SDL_GetError());
        std::exit(1);
    }
    ++checks;
}

static void silence(void* userdata, Uint8* stream, int length)
{
    SDL_memset(stream, 0, length);
    ++*static_cast<std::atomic<int>*>(userdata);
    SDL_Delay(1);
}

static SDL_AudioSpec wantedSpec(std::atomic<int>& callbacks)
{
    SDL_AudioSpec wanted{};
    wanted.freq = 48000;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 256;
    wanted.callback = silence;
    wanted.userdata = &callbacks;
    return wanted;
}

static void waitForCallbacks(const std::atomic<int>& callbacks, int previous)
{
    const auto start = SDL_GetTicks64();
    while (callbacks.load() <= previous && SDL_GetTicks64() - start < 1000) SDL_Delay(1);
    check(callbacks.load() > previous, "dummy audio callback runs");
}

static void checkControlThread(const SDL_AudioSpec& wanted)
{
    Oran7DeviceAudit state;
    audit = &state;
    auto opening = state.opening.get_future(), closing = state.closing.get_future();
    Oran7SdlAudioDevice::Oran7Backend backend;
    backend.initialize = auditInitialize; backend.quit = auditQuit;
    backend.open = auditOpen; backend.pause = auditPause; backend.close = auditClose;
    auto device = std::make_unique<Oran7SdlAudioDevice>(backend);
    SDL_AudioSpec obtained{};
    std::thread::id caller;
    int beats = 0;
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, &heartbeat, [&] { ++beats; });
    heartbeat.start(5);
    auto pump = [&](auto predicate) {
        QElapsedTimer timer; timer.start();
        while (!predicate() && timer.elapsed() < 1500) { QCoreApplication::processEvents(); QThread::msleep(1); }
        check(predicate(), "bounded synthetic audio control wait");
    };
    auto opened = std::async(std::launch::async, [&] {
        caller = std::this_thread::get_id();
        return device->open(wanted, obtained, 0);
    });
    pump([&] { return opening.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; });
    const int beforeOpen = beats;
    pump([&] { return beats >= beforeOpen + 5; });
    check(opened.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready,
          "GUI event loop continues while synthetic driver open is blocked");
    state.allowOpen.set_value();
    check(opened.get(), "synthetic device opens after releasing driver gate");
    device->setPaused(false); device->setPaused(true);
    auto closed = std::async(std::launch::async, [&] { device->close(); });
    pump([&] { return closing.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; });
    const int beforeClose = beats;
    pump([&] { return beats >= beforeClose + 5; });
    check(closed.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready,
          "GUI event loop continues while synthetic driver close is blocked");
    state.allowClose.set_value(); closed.get();
    state.slowOpen = state.slowClose = false;
    auto f32 = wanted; f32.format = AUDIO_F32SYS;
    check(!device->open(f32, obtained, 0) && std::string(SDL_GetError()) == "synthetic format rejection",
          "worker SDL error reaches caller for F32 fallback");
    check(device->open(wanted, obtained, 0), "S16 retry reuses owned runtime");
    device.reset();
    check(state.mta && state.initializations == 1 && state.releases == 1,
          "owned worker initializes MTA and pairs its SDL runtime reference");
    const auto owner = state.owners.front();
    check(owner != caller && owner != std::this_thread::get_id(), "audio owner differs from caller and GUI threads");
    for (const auto id : state.owners) check(id == owner, "init/open/pause/close/quit share one audio owner thread");
    audit = nullptr;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    // 真实 SDL、静音替身驱动；不打开系统音频设备、不播放文件、不运行主程序。
    SDL_SetMainReady();
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    check(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0, "initialize dummy audio");
    std::atomic<int> callbacks{0};
    auto wanted = wantedSpec(callbacks);
    SDL_AudioSpec obtained{};

    if (app.arguments().contains("--old-locked-close")) {
        // 隔离子进程重现旧关闭顺序，父进程有界终止；不得在测试主进程内锁死。
        const auto device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
        check(device != 0, "open baseline audio");
        SDL_PauseAudioDevice(device, 0);
        waitForCallbacks(callbacks, 0);
        SDL_LockAudioDevice(device);
        SDL_Delay(60); // 让音频线程进入等待设备锁的阶段。
        SDL_PauseAudioDevice(device, 1);
        std::puts("before-locked-close");
        std::fflush(stdout);
        SDL_CloseAudioDevice(device);
        return 2; // 若返回，说明当前 SDL/驱动没有重现旧死锁。
    }

    QProcess baseline;
    baseline.start(QCoreApplication::applicationFilePath(), {"--old-locked-close"});
    check(baseline.waitForStarted(2000), "start isolated old close reproducer");
    QByteArray output;
    for (int i = 0; i < 20 && !output.contains("before-locked-close"); ++i) {
        baseline.waitForReadyRead(100);
        output += baseline.readAllStandardOutput();
    }
    check(output.contains("before-locked-close"), "old path reaches locked close");
    const bool oldHangs = !baseline.waitForFinished(300);
    if (oldHangs) {
        baseline.kill();
        check(baseline.waitForFinished(2000), "terminate only isolated deadlock reproducer");
    }
    check(oldHangs, "old lock-close order reproduces a deadlock with bundled SDL");
    checkControlThread(wanted);

    Oran7SdlAudioDevice device;
    device.setPaused(false);
    device.close();
    for (int i = 0; i < 24; ++i) {
        const auto previous = callbacks.load();
        check(device.open(wanted, obtained, 0), "open managed device");
        device.setPaused(false);
        waitForCallbacks(callbacks, previous);
        device.close();
        const auto stopped = callbacks.load();
        SDL_Delay(8);
        check(callbacks.load() == stopped, "close waits for callback and no callback survives");
        device.close();
    }
    check(device.open(wanted, obtained, 0), "open device before concurrent controls");
    std::atomic<bool> stop{false};
    std::thread controls([&] {
        while (!stop.load()) {
            device.setPaused(false);
            device.setPaused(true);
            SDL_Delay(1);
        }
    });
    SDL_Delay(15);
    device.close();
    // SDL 可复用刚关闭的 ID；旧实例随后提交的暂停不能影响新的实例。
    Oran7SdlAudioDevice replacement;
    check(replacement.open(wanted, obtained, 0), "open replacement after close");
    const auto beforeReplacement = callbacks.load();
    replacement.setPaused(false);
    waitForCallbacks(callbacks, beforeReplacement);
    SDL_Delay(30);
    check(callbacks.load() > beforeReplacement + 2, "stale pause commands do not pause replacement");
    stop = true;
    controls.join();
    replacement.close();
    const auto beforeScope = callbacks.load();
    {
        Oran7SdlAudioDevice scoped;
        check(scoped.open(wanted, obtained, 0), "open scoped device");
        scoped.setPaused(false);
        waitForCallbacks(callbacks, beforeScope);
    }
    const auto afterScope = callbacks.load();
    SDL_Delay(20);
    check(callbacks.load() == afterScope, "RAII destruction joins callbacks");

    {
        // A0-3：F32 设备协商；dummy 驱动只验证打开/回调，不验证实际听感。
        SDL_AudioSpec f32Wanted = wanted;
        f32Wanted.format = AUDIO_F32SYS;
        SDL_AudioSpec f32Obtained{};
        Oran7SdlAudioDevice f32Device;
        check(f32Device.open(f32Wanted, f32Obtained, 0), "open managed F32 device");
        check(f32Obtained.format == AUDIO_F32SYS, "F32 format negotiated");
        const auto beforeF32 = callbacks.load();
        f32Device.setPaused(false);
        waitForCallbacks(callbacks, beforeF32);
        f32Device.close();
    }

    SDL_Quit();
    std::printf("PASS: %d SDL audio lifetime checks\n", checks);
}
