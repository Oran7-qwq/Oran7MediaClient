#ifndef FFPLAYER_H
#define FFPLAYER_H

#include "FFMsgQueue.h"
#include "FFPlay_Def.h"
#include "Oran7FFmpegDecoderCore.h"        // Oran7FFmpegDecoderCore 已独立成文件

#include <thread>
#include <functional>
#include <atomic>

// 前向声明：仅指针成员引用，避免把平台头/第三方头泄漏进公共头
struct ID3D11Device;
struct sonicStreamStruct;

/**
 * @brief Oran7FFplayerCore  播放器核心（ffplay 系移植）：解封装读取线程、播放控制、音视频同步、
 *                   音频输出、seek、变速、消息队列与视频帧刷新编排。
 *                   内部实现细节收进 private 并按领域分组；
 *                   对外只暴露 Oran7MediaPlayer 门面真正使用的 14 个方法 + 消息队列访问器。
 */
class Oran7FFplayerCore
{
public:
    explicit Oran7FFplayerCore();
    ~Oran7FFplayerCore();

    //================= PUBLIC 接口 =================
    int ffp_create();                                               /*! @brief 创建FFPlayer*/
    void ffp_destroy();                                          //销毁FFPlayer
    int ffp_prepare_async_l(char *file_name);      // 获取播放链接，启动总入口stream_open

    int       ffp_start_l();                                          // 开始播放
    int       ffp_pause_l();                                        // 暂停播放
    int       ffp_stop_l();                                           // 终止all stream

    long   ffp_get_duration_l();                              //媒体文件总时长
    long   ffp_get_current_position_l();                 //当前播放位置（整数秒，向下取整）
    double ffp_get_current_position_d();              //当前播放位置（浮点秒，保留小数精度）

    int  ffp_seek_to_l(long msec);                           // seek 接口 - ms
    void ffp_set_playback_rate(float rate);            // 设置倍数rate
    void ffp_update_reqVolum(int value);             // 设置音量

    // =========渲染/设备注入 =========//
    void AddVideoRefreshCallback(std::function<int(const Frame *,AVFrame *)> callback);
    void setD3D11Device(ID3D11Device *dev);                       //外部定义D3D11设备-接口

    //================= 消息队列访问器 =================//
    // msg_queue_ 的唯一外部出口（Oran7MediaPlayer 经它收发播放状态消息）
    MessageQueue* ffp_msg_queue() { return &msg_queue_; }

    //================= 变速内部辅助（暂未调用） =================//
    float ffp_get_play_backrate();
    float ffp_get_playback_rate();
    bool  is_normal_playback_rate();
    int   ffp_get_playback_rate_change();
    void  ffp_set_playback_rate_change(int change);

private:
    friend class Oran7FFmpegDecoderCore;   // Oran7FFmpegDecoderCore::audio_thread/video_thread 需访问内部队列与流状态

    // ================== PRIVATE 接口==================//

    int  read_thread();                                    //数据读取线程函数
    static int decode_interrupt_cb(void *ctx)//avformat_open_input read中断回调，避免网络堵塞无法退出
    {
        Oran7FFplayerCore *p = static_cast<Oran7FFplayerCore*>(ctx);
        return p->abort_request.load() ? 1 : 0;
    }
    int  stream_open(const char *file_name);                                // 启动ffplayer总核心入口
    void stream_close();                                                                  // 控制ffplayer All stream close
    int  stream_component_open(int stream_index);                   //打开解码器、创建解码线程、初始化输出
    void stream_component_close(int stream_index);                 //关闭解码线程，释放解码器资源
    int  stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue); //缓冲溢满等待判断
    int  video_refresh_thread();                                                      // video frame刷新线程启动入口
    void video_refresh(double *remaining_time);                         // video frame 刷新线程函数对象,由入口调用
    void check_play_finished();                                                       // 判断播放结束
    void stream_seek(int64_t pos, int64_t rel, int seek_by_bytes); // seek 请求 由公共接口 ffp_seek_to_l处理调用

    //==== 音视频同步 ====
    int    get_master_sync_type();
    double get_master_clock();                                  //获取主时钟的当前时间戳

    /*计算上一帧需要持续的duration,校正算法，用于音视频同步，在video_refresh中*/
    double vp_duration(Frame *vp, Frame *nextvp)
    {
        if (vp->serial == nextvp->serial)                       // 同一播放序列，序列连续的情况下
        {
            double duration = nextvp->pts - vp->pts;
            if (isnan(duration)                                         // duration 数值异常
                || duration <= 0                                         // pts值没有递增时
                || duration > max_frame_duration             // 超过了最大帧范围
                )
                return vp->duration / pf_playback_rate;	 /* 异常时以帧时间为基准(1秒/帧率) */
            else
                return duration / pf_playback_rate;          //使用两帧pts差值计算duration，一般情况下也是走的这个分支
        }
        else
            return 0.0;                                                      // 不同播放序列, 序列不连续则直接返回0
    }
    /*计算前一帧应该延迟多少,同步策略*/
    double compute_target_delay(double delay,Frame *last_vp,Frame *vp);
    /*更新视频时钟pts*/
    void update_video_pts(double pts, int64_t pos, int serial);
    int  toggle_pause_(int pause_on);                         //触发一次播放/暂停请求
    void stream_update_pause_l();
    void stream_toggle_pause_l(int pause_on);

    //==== 音频输出 ====
    int  audio_open(AVChannelLayout wanted_channel_layout,
                    int wanted_nb_channels, int wanted_sample_rate,
                    struct AudioParams *audio_hw_params);
    void audio_close();
    // 音频重采样/变速处理
    static int  audio_resample_frame(Oran7FFplayerCore *is);
    // SDL 音频回调
    static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);

    //==== 硬件加速（D3D11/Qt 设备注入） ====
    bool initialize_hardware_acceleration(const AVCodec* codec);//初始化硬件加速
    bool create_hw_device_ctx_from_qt_device();//兼容qt6的d3d11设备渲染设备创建方案
    // 视频硬件协商回调：供 avctx->get_format 使用（FFmpeg 固定签名回调，故为 static）
    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

    //==== 变速内部 ====
    int get_target_frequency();
    int get_target_channels();

    // ================== PRIVATE Parameters ==================//

    int paused = 0;              //播放器暂停状态标志
    int pause_req = 0;         //上一次播放/暂停请求，用于再次启动时标志时钟应以实际数据的->pts更新
    int auto_resume = 0;
    int buffering_on = 0;

    int step = 0;                       //逐帧单步运行，未启用
    int framedrop = -1;
    int frame_drops_late = 0;

    int eof = 0;                        //FFPlayer数据读取read_thread线程结束标志
    bool Audio_EOF= false;    //音频流结束标志
    bool Video_EOF= false;    // 视频流结束标志
    int All_finished_ = 0;        // All stream END


    int av_sync_type = AV_SYNC_AUDIO_MASTER;// 音视频同步类型, 默认audio master
    Clock audclk;                               // 音频时钟
    Clock vidclk;                                // 视频时钟
    double audio_clock = 0;             // 当前音频帧的PTS+当前帧Duration,单位秒
    int    audio_clock_serial;             // 播放序列，seek可改变此值, 解码后保存
    double last_frame_timer;           // 记录最后一帧视频的播放的时刻
    double max_frame_duration;    // 一帧最大间隔. above this, we consider the jump a timestamp discontinuity
    int64_t audio_callback_time = 0; //用于sdl_audio_callback时钟同步


    FrameQueue pictq;             // 视频Frame队列
    FrameQueue sampq;          // 采样Frame队列
    PacketQueue audioq;         // 音频packet队列
    PacketQueue videoq;         // 视频packet队列
    std::atomic<int> abort_request{0};// 各个数据缓存队列退出请求标志


    AVFormatContext *ic = NULL;              //输入文件AVFormat上下文
    AVStream *audio_st = NULL;              // 音频流
    AVStream *video_st = NULL;              // 视频流
    int audio_stream = -1;                       //AVFormatContext *ic中音频流索引
    int video_stream = -1;                       //AVFormatContext *ic中视频流索引


    Oran7FFmpegDecoderCore auddec;             // 音频解码器
    Oran7FFmpegDecoderCore viddec;              // 视频解码器

    std::thread *read_thread_;                             //数据读取线程,函数对象
    std::thread *video_refresh_thread_ = NULL; //视频刷新线程，函数对象

    char *input_filename_;                                  //播放文件资源，总链接(Local Path / Network Url)


    bool sureHaveVideo=false;                          // 是否启用video stream（music player for false）
    std::function<int(const Frame * , AVFrame *)> video_refresh_callback_ = NULL; // 外部视频刷新线程回调函数对象，由外部约定


    int64_t seek_req = 0;
    int64_t seek_rel = 10;
    int64_t seek_flags = 0;
    int64_t seek_pos = 0;  // seek的位置


    float pf_playback_rate = 1.0;               // 播放速率
    int   pf_playback_rate_changed = 0;   // 播放速率改变
    sonicStreamStruct *audio_speed_convert = nullptr;


    SDL_AudioDeviceID audio_dev;          //音频设备ID
    struct AudioParams audio_src;           // 音频解码后的frame参数
    struct AudioParams audio_tgt;           // SDL支持的音频参数，重采样转换：audio_src->audio_tgt
    struct SwrContext *swr_ctx  = NULL;  // 音频重采样context
    int   audio_hw_buf_size = 0;              // SDL音频缓冲区的大小(字节为单位)
    int   audio_write_buf_size;                //向SDL音频缓冲区实际写入的数据大小
    uint8_t *audio_buf = NULL;               // 指向待播放的一帧音频数据，指向的数据区将被拷入SDL音频缓冲区
    uint8_t *audio_buf1 = NULL;             // 指向重采样后的数据
    unsigned int audio_buf_size = 0;      // 待播放的一帧音频数据(audio_buf指向)的大小
    unsigned int audio_buf1_size = 0;    // 申请到的音频缓冲区audio_buf1的实际尺寸
    int audio_buf_index = 0;                  // 更新拷贝位置 当前音频帧中已拷入SDL音频缓冲区

    std::atomic<int> audio_volume=DEFAULT_VOLUM_SIZE; //音量
    int startup_volume = DEFAULT_VOLUM_SIZE; // 起始音量


    MessageQueue msg_queue_;                           //消息队列实例，由FFPlayer管理


    enum AVHWDeviceType hw_type=AV_HWDEVICE_TYPE_NONE;
    bool Use_Hardware=false;                                       //是否启用硬件加速
    AVBufferRef *hw_device_ctx_buffer_ref=nullptr;     //硬件设备上下文
    ID3D11Device* m_qtDevice = nullptr;                      //D3D11设备存储


    Oran7FFplayerCore(Oran7FFplayerCore&)=delete;
    Oran7FFplayerCore& operator=(Oran7FFplayerCore&)=delete;
};

//=================FFPlayer操作消息队列方法================//
inline static void ffp_notify_msg(Oran7FFplayerCore *ffp, int what) {
    msg_queue_put_simple(ffp->ffp_msg_queue(), what, 0, 0);
}

inline static void ffp_notify_msg(Oran7FFplayerCore *ffp, int what, int arg1) {
    msg_queue_put_simple(ffp->ffp_msg_queue(), what, arg1, 0);
}

inline static void ffp_notify_msg(Oran7FFplayerCore *ffp, int what, int arg1, int arg2) {
    msg_queue_put_simple(ffp->ffp_msg_queue(), what, arg1, arg2);
}

inline static void ffp_notify_msg(Oran7FFplayerCore *ffp, int what, int arg1, int arg2, void *obj, int obj_len) {
    msg_queue_put_simple(ffp->ffp_msg_queue(), what, arg1, arg2, obj, obj_len);
}

inline static void ffp_remove_msg(Oran7FFplayerCore *ffp, int what) {
    msg_queue_remove(ffp->ffp_msg_queue(), what);
}

#endif // FFPLAYER_H
