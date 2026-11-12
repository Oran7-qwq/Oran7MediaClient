#pragma once
#include "ffmsg.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// 只替换 Native I/O；Controller、状态、Qt 定时器和异步释放均使用生产实现。
struct Frame;
struct AVFrame;
struct ID3D11Device;
struct AVMessage { int value = 0; int arg1 = 0; };
class Oran7AudioParameterQueue;
inline std::atomic<int> freedMessages{0};
inline void msg_free_obj_res(AVMessage*) { ++freedMessages; }
inline constexpr int SDL_MIX_MAXVOLUME = 128;
inline int DEFAULT_VOLUM_SIZE = 128;
inline constexpr int MP_STATE_ASYNC_PREPARING = 2;
inline constexpr int MP_STATE_STARTED = 4;
inline constexpr int MP_STATE_PAUSED = 5;
struct Oran7FakeSession {
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<AVMessage> messages;
    bool abort = false;
    bool allowRelease = true;
    std::atomic<bool> releaseStarted{false}, released{false};
    std::atomic<int> state{0}, starts{0}, pauses{0}, durationReads{0};
    std::atomic<long> duration{120000}, seek{0};
    std::atomic<double> position{5.0};
    std::atomic<int> volume{DEFAULT_VOLUM_SIZE}, frameCallbacks{0}, devices{0};
    std::string source;
    std::thread::id releaseThread;
    void push(int value, int arg1 = 0) {
        std::lock_guard<std::mutex> lock(mutex);
        messages.push_back({value, arg1});
        condition.notify_all();
    }
    void unblock() {
        std::lock_guard<std::mutex> lock(mutex);
        allowRelease = true;
        condition.notify_all();
    }
};
inline std::vector<std::shared_ptr<Oran7FakeSession>> sessions;
inline bool failCreate = false, failPrepare = false;
class Oran7MediaPlayer {
public:
    std::shared_ptr<Oran7FakeSession> session = std::make_shared<Oran7FakeSession>();
    std::function<int(void*)> loop;
    std::thread worker;
    Oran7MediaPlayer() { sessions.push_back(session); }
    ~Oran7MediaPlayer() {
        session->releaseThread = std::this_thread::get_id();
        session->releaseStarted = true;
        {
            std::unique_lock<std::mutex> lock(session->mutex);
            session->condition.wait(lock, [&] { return session->allowRelease; });
            session->abort = true;
            session->condition.notify_all();
        }
        if (worker.joinable()) worker.join();
        session->released = true;
    }
    int oran7mp_create(std::function<int(void*)> callback) { loop = std::move(callback); return failCreate ? -1 : 0; }
    int oran7mp_prepare_async() {
        session->state = MP_STATE_ASYNC_PREPARING;
        worker = std::thread([this] { loop(this); });
        return failPrepare ? -1 : 0;
    }
    int oran7mp_get_msg(AVMessage* message, int) {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->condition.wait(lock, [&] { return session->abort || !session->messages.empty(); });
        if (session->abort) return -1;
        *message = session->messages.front();
        session->messages.pop_front();
        return 0;
    }
    int oran7mp_set_data_source(const char* source) { session->source = source; return 0; }
    std::string dataSource() const { return session->source; }
    void AddVideoRefreshCallback(std::function<int(const Frame*, AVFrame*)>) { ++session->frameCallbacks; }
    void oran7mp_setD3D11Device(ID3D11Device*) { ++session->devices; }
    void oran7mp_setAudioParameterQueue(std::shared_ptr<Oran7AudioParameterQueue>) {}
    int oran7mp_get_state() { return session->state; }
    int oran7mp_start() { ++session->starts; session->state = MP_STATE_STARTED; session->push(FFP_MSG_PLAYBACK_STATE_CHANGED, MP_STATE_STARTED); return 0; }
    int oran7mp_pause() { ++session->pauses; session->state = MP_STATE_PAUSED; session->push(FFP_MSG_PLAYBACK_STATE_CHANGED, MP_STATE_PAUSED); return 0; }
    long oran7mp_get_duration() { const auto value = session->duration.load(); ++session->durationReads; return value; }
    double oran7mp_get_current_position_double() { return session->position; }
    int oran7mp_seek_to(long value) { session->seek = value; return 0; }
    void oran7mp_set_playback_volume(int value) { session->volume = value; }
};
