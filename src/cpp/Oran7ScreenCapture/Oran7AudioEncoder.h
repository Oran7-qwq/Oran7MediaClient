#ifndef ORAN7AUDIOENCODER_H
#define ORAN7AUDIOENCODER_H

#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

#include "Oran7GlobalHelper.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
struct SwrContext;
}

class Oran7AudioEncoder
{
public:
    Oran7AudioEncoder();
    ~Oran7AudioEncoder();

    struct EncoderConfig{
        std::string codecName = "libfdk_aac";
        int sampleRate = 48000;
        int channels = 2; //stero
        int bitrateBps = 128000; // 128 kbps
    };

    bool configure(const EncoderConfig& cfg, ErrorCallback onError);
    const AVCodecContext* codecContext() const;
    AVRational encoderTimeBase() const { return _avctx ? _avctx->time_base : AVRational{1, 48000}; }

    /*! @note 启动编码器线程循环 */
    bool start();
    ///*! @note 停止编码器线程循环 */
    bool stop();

    /**
     * @brief 外部接口 push pcmFrame 到-->
     *            Oran7AudioEncoder 内部维护的 帧队列
     * @param frame 接管内存所有权
     */
    void pushFrame(AVFrame* frame);

    /**
     * @brief getPacket 复用器线程获取编码后的音频包AVPacket
     * @param pkt
     * @param timeoutMs 超时等待阈值
     * @return 获取到AVPacket || 编码循环主动停止  --> 返回true
     *                超时等待 --> 返回false
     */
    bool getPacket(AVPacket* pkt, int timeoutMs = 100);

    bool isRunning() const { return _running.load(); }
    bool isOpened()  const { return _opened.load(); }

    /*! @note 调试接口 */
    int64_t sentFrameCount() const { return _frameIndex.load(); }
    int queuedPktCount() const {
        std::lock_guard<std::mutex> lock(_pkt_mtx);
        return (int)_pktQueue.size();
    }
private:
    void encodeLoop();
    void processFrame(AVFrame* frame);
    void flushFrames();

    // 帧队列
    mutable std::mutex _frame_mtx;
    std::condition_variable _frameCond;
    std::queue<AVFrame*> _frameQueue;
    static constexpr int kMaxFrameQueue = 3;

    // FIFO：WASAPI ~480 samples → libfdk_aac 1024 samples
    std::vector<int16_t> _fifoBuf;       // 交错 S16 缓冲
    int                  _fifoUsed = 0;   // 当前 FIFO 内每声道采样数
    int64_t              _fifoBasePts = 0; // FIFO 首块数据的 PTS（48kHz 时间基）

    // 包队列：编码线程写->复用线程读取消费
    mutable std::mutex _pkt_mtx;
    std::condition_variable _pktCond;
    std::queue<AVPacket*> _pktQueue;
    static constexpr int kMaxPktQueued = 10;

    // 编码器
    const AVCodec*  _codec = nullptr;
    AVCodecContext* _avctx = nullptr;
    EncoderConfig          _cfg;
    bool            _configured = false;
    AVSampleFormat  _codecSampleFmt = AV_SAMPLE_FMT_S16; // 编码器实际需要的格式
    SwrContext*     _swr = nullptr;  // S16 → 目标格式 转换器

    // 线程
    std::thread        _encode_thread;
    std::atomic<bool>  _running{false};  //运行encodeLoop标志
    std::atomic<bool>  _opened{false};  //编码器被avcodec_open2标志
    std::atomic<int64_t> _frameIndex{0};

    // 回调
    ErrorCallback _onError;
};

#endif // ORAN7AUDIOENCODER_H
