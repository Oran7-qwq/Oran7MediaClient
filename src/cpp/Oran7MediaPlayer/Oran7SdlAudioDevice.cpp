#include "Oran7SdlAudioDevice.h"
#include <utility>

Oran7SdlAudioDevice::~Oran7SdlAudioDevice()
{
    close();
}

bool Oran7SdlAudioDevice::open(const SDL_AudioSpec& wanted, SDL_AudioSpec& obtained, int allowedChanges)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeLocked();
    m_device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, allowedChanges);
    return m_device != 0;
}

void Oran7SdlAudioDevice::setPaused(bool paused)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_device) SDL_PauseAudioDevice(m_device, paused ? 1 : 0);
}

void Oran7SdlAudioDevice::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeLocked();
}

void Oran7SdlAudioDevice::closeLocked()
{
    const auto device = std::exchange(m_device, 0);
    if (!device) return;
    // Close 自行停止回调并等待音频线程；持有 SDL_LockAudioDevice 再 Close 会死锁。
    // 回调式音频无需 ClearQueuedAudio；关闭返回后不能再操作可能被 SDL 复用的旧 ID。
    SDL_CloseAudioDevice(device);
}
