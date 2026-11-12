#ifndef ORAN7VIDEOENCODER_H
#define ORAN7VIDEOENCODER_H

#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

#include "Oran7GlobalHelper.h"

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

class Oran7VideoEncoder
{
public:
    Oran7VideoEncoder();
    ~Oran7VideoEncoder();

    struct EncoderConfig {
        std::string codecName = "libx264"; // "libx264" 或 "h264_mf"
        int width = 1920;
        int height = 1080;
        int fps = 30;
        int bitrateKbps = 5000;
        int gopSize = 30;
        std::string preset = "veryfast";
        std::string tune = "zerolatency";
        std::string profile = "baseline";
        int threadCount = 1;

        // 直接传入 AVBufferRef*（DDAGrab 的 m_hwdev），走硬编路径
        AVBufferRef* hwDevice = nullptr;
        bool liveStreaming = false; // WHIP 保持原低延迟场景；本地录制优先画质与硬编。
    };

    bool configure(const EncoderConfig& cfg,ErrorCallback onError);
    const AVCodecContext* codecContext() const { return _avctx; }
    AVRational encoderTimeBase() const { return _avctx ?_avctx->time_base : AVRational{1, 30}; }

    bool start();
    bool stop();
    
    /**
     * @brief 对外接口，压入编码帧队列
     * 
     * @param frame 接管内存所有权，无论GPU or CPU,编码使用后由编码线程释放内存
     */
    void pushFrame(AVFrame *frame); //接管所有权

    /**
     * 编码线程是否在运行中
     */
    bool isRuning() const {return _running.load();}
    bool isOpened() const {return _opened.load();}

    /** 请求编码器产出一个 IDR 关键帧（响应 PLI/新观众加入） */
    void requestKeyframe() { _forceKeyframe.store(true); }

    /** 内部：阻塞入队编码包（不丢弃） */
    bool enqueuePacket(AVPacket* pkt);

    /**
     *  @brief 消费端拉取接口（阻塞，timeoutMs 超时返回 false）
     *  @note 调用方负责 av_packet_unref / av_packet_free
    */
    bool getPacket(AVPacket* pkt, int timeoutMs = 100);

    /**
     * @brief 调试
     */
    int64_t sentFrameCount() const { return _frameIndex.load(); }
    int64_t droppedFrameCount() const { return _droppedFrames.load(); }
    int queuedPktCount() const {
        std::lock_guard<std::mutex> lock(_pkt_mtx);
        return (int)_pktQueue.size();
    }
private:
    void encodeLoop();
    void processFrame(AVFrame *frame);
    void flushFrames();

    //帧队列
    mutable std::mutex _frame_mtx;
    std::condition_variable _cond;
    std::queue<AVFrame*> _frameQueue; //帧队列
    static constexpr int _kMaxQueued = 30;  // ~0.5s 缓冲，避免 60→48fps 掉帧

    // 包队列（编码线程写，消费线程读）
    mutable std::mutex _pkt_mtx;
    std::condition_variable _pktCond;
    std::condition_variable _pktSpaceCond;  // 编码线程等消费端腾出空间
    std::queue<AVPacket*> _pktQueue;
    static constexpr int _kMaxPktQueued = 15;

    //编码器
    const AVCodec *_codec = nullptr;
    AVCodecContext *_avctx = nullptr;
    EncoderConfig _cfg;               // configure 暂存，start() 在线程内打开
    bool _configured = false;
    bool _softwareMfFallback = false;

    //编码线程
    std::thread _encode_thread;
    std::atomic<bool> _running{false};
    std::atomic<bool> _opened{false};
    std::atomic<bool> _forceKeyframe{false};
    std::atomic<int64_t> _frameIndex{0};
    std::atomic<int64_t> _droppedFrames{0};

    //回调函数
    ErrorCallback _onError;
};

#endif // ORAN7VIDEOENCODER_H
