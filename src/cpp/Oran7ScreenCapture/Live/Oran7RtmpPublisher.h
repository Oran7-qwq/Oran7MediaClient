#pragma once
#include <QString>
#include <atomic>
#include <chrono>
#include <memory>
extern "C"
{
#include <libavformat/avformat.h>
}

// 仅由直播工作线程调用；共享取消标记可由 GUI 写入，不创建线程或无界队列。
class Oran7RtmpPublisher
{
  public:
    explicit Oran7RtmpPublisher(std::shared_ptr<std::atomic_bool> cancelled);
    ~Oran7RtmpPublisher();
    Oran7RtmpPublisher(const Oran7RtmpPublisher&) = delete;
    Oran7RtmpPublisher& operator=(const Oran7RtmpPublisher&) = delete;
    static bool available();
    static QString validate(const QString& server, const QString& key);
    bool open(const QString& server, const QString& key, const AVCodecParameters* video, AVRational videoTimeBase,
              const AVCodecParameters* audio = nullptr, AVRational audioTimeBase = {1, 48000}, int timeoutMs = 5000);
    bool write(const AVPacket* packet, bool video, AVRational timeBase);
    void close();
    QString error() const { return m_error; }
    quint64 payloadBytes() const { return m_payloadBytes; }
    quint64 videoPackets() const { return m_videoPackets; }

  private:
    static int interrupt(void* self);
    void arm(int milliseconds);
    std::shared_ptr<std::atomic_bool> m_cancelled;
    AVFormatContext* m_context = nullptr;
    AVStream* m_video = nullptr;
    AVStream* m_audio = nullptr;
    std::chrono::steady_clock::time_point m_deadline;
    QString m_error;
    int m_timeoutMs = 5000;
    bool m_header = false;
    bool m_failed = false;
    quint64 m_payloadBytes = 0, m_videoPackets = 0;
};
