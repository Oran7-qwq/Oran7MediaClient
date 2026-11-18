#include "Oran7SdlAudioDevice.h"
#include <condition_variable>
#include <exception>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <objbase.h>
#include <QElapsedTimer>
#include <QDebug>

namespace {
std::mutex sdlLifecycleMutex;
constexpr Uint32 sdlSubsystems = SDL_INIT_AUDIO | SDL_INIT_TIMER;

// 仅慢操作输出阶段和耗时，便于后续现场核对；不记录媒体或设备身份。
class Oran7AudioControlTimer {
public:
    explicit Oran7AudioControlTimer(const char* stage) : m_stage(stage) { m_timer.start(); }
    ~Oran7AudioControlTimer() {
        const auto elapsed = m_timer.elapsed();
        if (elapsed >= 250) qWarning().nospace() << "[PlaybackAudio] stage=" << m_stage << " elapsed_ms=" << elapsed;
    }
private:
    const char* m_stage;
    QElapsedTimer m_timer;
};
}

// SDL/WASAPI 设备及其 COM 引用只在同一条 MTA 线程创建、控制、释放。
// 调用方的等待仅发生在 Native 解码／消息／退役线程，GUI 不再初始化音频驱动。
struct Oran7SdlAudioDevice::Oran7ControlThread {
    explicit Oran7ControlThread(Oran7Backend backend) : api(backend), worker([this] { run(); }) {}
    ~Oran7ControlThread() {
        {
            std::lock_guard<std::mutex> lock(mailbox);
            stopping = true;
        }
        ready.notify_one();
        worker.join();
    }
    void invoke(std::function<void()> work) {
        std::unique_lock<std::mutex> lock(mailbox);
        finished = false;
        failure = nullptr;
        command = std::move(work);
        ready.notify_one();
        completed.wait(lock, [this] { return finished; });
        if (failure) std::rethrow_exception(failure);
    }
    bool open(const SDL_AudioSpec& wanted, SDL_AudioSpec& obtained, int allowedChanges) {
        close();
        if (FAILED(apartment)) {
            error = "Could not initialize audio control MTA";
            return false;
        }
        if (!initialized) {
            Oran7AudioControlTimer timer("runtime_init");
            std::lock_guard<std::mutex> lock(sdlLifecycleMutex);
            if (api.initialize(sdlSubsystems) != 0) {
                error = SDL_GetError();
                return false;
            }
            initialized = true;
        }
        Oran7AudioControlTimer timer("device_open");
        device = api.open(nullptr, 0, &wanted, &obtained, allowedChanges);
        error = device ? std::string() : SDL_GetError();
        return device != 0;
    }
    void close() {
        const auto old = std::exchange(device, 0);
        // Close 会等待回调结束；绝不额外持有 SDL 的音频回调锁。
        if (old) {
            Oran7AudioControlTimer timer("device_close");
            api.close(old);
        }
    }
    void run() {
        apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        for (;;) {
            std::function<void()> work;
            {
                std::unique_lock<std::mutex> lock(mailbox);
                ready.wait(lock, [this] { return stopping || bool(command); });
                if (stopping) break;
                work = std::move(command);
                command = {};
            }
            std::exception_ptr caught;
            try { work(); } catch (...) { caught = std::current_exception(); }
            {
                std::lock_guard<std::mutex> lock(mailbox);
                failure = caught;
                finished = true;
            }
            completed.notify_one();
        }
        close();
        if (initialized) {
            Oran7AudioControlTimer timer("runtime_quit");
            std::lock_guard<std::mutex> lock(sdlLifecycleMutex);
            api.quit(sdlSubsystems);
        }
        if (SUCCEEDED(apartment)) CoUninitialize();
    }

    Oran7Backend api;
    SDL_AudioDeviceID device = 0;
    bool initialized = false;
    HRESULT apartment = E_FAIL;
    std::string error;
    std::mutex mailbox;
    std::condition_variable ready, completed;
    std::function<void()> command;
    std::exception_ptr failure;
    bool stopping = false, finished = false;
    std::thread worker; // 所有共享字段先构造，线程最后启动、最先回收。
};

Oran7SdlAudioDevice::Oran7SdlAudioDevice() = default;
Oran7SdlAudioDevice::Oran7SdlAudioDevice(Oran7Backend backend) : m_backend(backend) {}
Oran7SdlAudioDevice::~Oran7SdlAudioDevice() = default;

bool Oran7SdlAudioDevice::open(const SDL_AudioSpec& wanted, SDL_AudioSpec& obtained, int allowedChanges)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_control) m_control = std::make_unique<Oran7ControlThread>(m_backend);
    bool opened = false;
    m_control->invoke([&] { opened = m_control->open(wanted, obtained, allowedChanges); });
    // SDL 错误是线程局部值，失败时复制回调用方，保留原 F32→S16 回退诊断。
    if (!opened) SDL_SetError("%s", m_control->error.c_str());
    return opened;
}

void Oran7SdlAudioDevice::setPaused(bool paused)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_control) m_control->invoke([&] {
        if (m_control->device) m_backend.pause(m_control->device, paused ? 1 : 0);
    });
}

void Oran7SdlAudioDevice::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_control) m_control->invoke([this] { m_control->close(); });
}
