#ifndef DECODER_H
#define DECODER_H

#include "FFPlay_Def.h"     // PacketQueue/FrameQueue/AVPacket/AVRational/FFmpeg 类型（extern "C" 引入）
#include <thread>

class Oran7FFplayerCore;             // 前向声明（audio_thread/video_thread 的 void* arg 在 Oran7FFmpegDecoderCore.cpp 内再取完整类型）

/**
 * @brief Oran7FFmpegDecoderCore  单个音/视频流的解码器：解码线程生命周期 + packet→frame 解码 + 帧入 FrameQueue
 *                  由 Oran7FFplayerCore 以值成员持有（auddec/viddec），属 Oran7FFplayerCore 内部实现类。
 */
class Oran7FFmpegDecoderCore
{
public:
    explicit Oran7FFmpegDecoderCore();
    Oran7FFmpegDecoderCore(Oran7FFmpegDecoderCore &decoder)=delete;
    Oran7FFmpegDecoderCore& operator=(Oran7FFmpegDecoderCore& decoder)=delete;
    ~Oran7FFmpegDecoderCore();

    void decoder_init(AVCodecContext *avctx, PacketQueue *queue);   // 绑定解码器上下文与包队列
    int  decoder_start(enum AVMediaType codec_type, const char *thread_name, void* arg); // 创建并启动解码线程
    void decoder_abort(FrameQueue *fq);                             // 请求退出并等待解码线程结束
    void decoder_destroy();                                                   // 释放解码器上下文

private:
    friend class Oran7FFplayerCore;   // Oran7FFplayerCore 需直碰 auddec/viddec 的 pkt_serial_/start_pts 等字段

    // —— 解码线程体（仅类内部使用）——
    int decode_packet_to_frame(AVFrame *frame);
    int get_video_frame(AVFrame *frame);
    int queue_picture(FrameQueue *fq, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
    int audio_thread(void* arg);
    int video_thread(void* arg);

    // —— 包队列与解码器句柄 ——
    PacketQueue   *queue_;          // 数据包队列
    AVCodecContext *avctx_;        // 解码器上下文
    AVPacket pkt_;                        // AVERROR(EAGAIN) 缓存包
    int      pkt_serial_ = -1;           // 取出的队头数据包序列号,在/*阻塞式读取packet*/时获取到
    int      finished_;                      // =0，解码器处于工作状态；=非0，解码器处于空闲状态
    std::thread *decoder_thread_ = nullptr;

    int      packet_pending_ = 0;  // 用在解码线程缓存包packet的标志
    int64_t next_pts;
    AVRational next_pts_tb;
    int64_t start_pts;
    AVRational start_pts_tb;

    // —— 硬件加速帧暂存/缩放 ——
    AVFrame *software_frame = nullptr;   // 用于硬件加速解码缓存从显存中取出的帧数据内存

    // 用于硬件加速，记录硬件解码输出的 pixelformat（Oran7FFplayerCore.cpp 与 Oran7FFmpegDecoderCore.cpp 共享，唯一定义在 Oran7FFmpegDecoderCore.cpp）
    static enum AVPixelFormat s_hw_pix_fmt;
};

#endif // DECODER_H
