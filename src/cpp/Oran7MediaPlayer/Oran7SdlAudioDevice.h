#pragma once

#include <SDL2/SDL.h>
#include <mutex>
#include <memory>

// 单播放器独占后台 MTA 控制线程；所有 SDL 生命周期操作在该线程执行。
// public 命令仍同步返回，只由 Native 的解码／消息／退役线程调用，SDL 回调不得调用。
class Oran7SdlAudioDevice
{
public:
    // 默认使用真实 SDL；隔离检查可注入慢设备，不接触用户音频硬件。
    struct Oran7Backend {
        decltype(&SDL_InitSubSystem) initialize = SDL_InitSubSystem;
        decltype(&SDL_QuitSubSystem) quit = SDL_QuitSubSystem;
        decltype(&SDL_OpenAudioDevice) open = SDL_OpenAudioDevice;
        decltype(&SDL_PauseAudioDevice) pause = SDL_PauseAudioDevice;
        decltype(&SDL_CloseAudioDevice) close = SDL_CloseAudioDevice;
    };
    Oran7SdlAudioDevice();
    explicit Oran7SdlAudioDevice(Oran7Backend backend);
    ~Oran7SdlAudioDevice();
    Oran7SdlAudioDevice(const Oran7SdlAudioDevice&) = delete;
    Oran7SdlAudioDevice& operator=(const Oran7SdlAudioDevice&) = delete;

    bool open(const SDL_AudioSpec& wanted, SDL_AudioSpec& obtained, int allowedChanges);
    void setPaused(bool paused);
    void close();

private:
    struct Oran7ControlThread;
    std::mutex m_mutex;
    Oran7Backend m_backend;
    std::unique_ptr<Oran7ControlThread> m_control;
};
