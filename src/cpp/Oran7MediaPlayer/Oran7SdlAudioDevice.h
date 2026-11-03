#pragma once

#include <SDL2/SDL_audio.h>
#include <mutex>

// 单个播放器独占设备；句柄只在本类锁内使用，SDL 回调不得调用本类的控制方法。
class Oran7SdlAudioDevice
{
public:
    Oran7SdlAudioDevice() = default;
    ~Oran7SdlAudioDevice();
    Oran7SdlAudioDevice(const Oran7SdlAudioDevice&) = delete;
    Oran7SdlAudioDevice& operator=(const Oran7SdlAudioDevice&) = delete;

    bool open(const SDL_AudioSpec& wanted, SDL_AudioSpec& obtained, int allowedChanges);
    void setPaused(bool paused);
    void close();

private:
    void closeLocked();
    std::mutex m_mutex;
    SDL_AudioDeviceID m_device = 0;
};
