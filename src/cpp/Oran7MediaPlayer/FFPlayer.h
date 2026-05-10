#ifndef FFPLAYER_H
#define FFPLAYER_H

#include "FFMsgQueue.h"
#include "FFPlay_Def.h"
#include "Sonic.h"
#include "ImageScaler.h"

#ifdef _WIN32//适配d3dll.lib
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>//ComPtr
#endif

#include <thread>
#include <functional>
#include <atomic>

class Decoder
{
public:
    explicit Decoder();
    Decoder(Decoder &decoder)=delete;
    Decoder& operator=(Decoder& decoder)=delete;

    ~Decoder();
    void decoder_init(AVCodecContext *avctx, PacketQueue *queue);
    // 创建和启动线程
    int decoder_start(enum AVMediaType codec_type, const char *thread_name, void* arg);
    // 停止线程
    void decoder_abort(FrameQueue *fq);
    void decoder_destroy();
    int decode_packet_to_frame(AVFrame *frame);
    int get_video_frame(AVFrame *frame);
    int queue_picture(FrameQueue *fq, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
    int audio_thread(void* arg);
    int video_thread(void* arg);

    PacketQueue	*queue_;         // 数据包队列
    AVCodecContext	*avctx_;     // 解码器上下文
    AVPacket pkt_;
    int		pkt_serial_ = -1;         // 取出的队头数据包序列号,在/*阻塞式读取packet*/时获取到
    int		finished_;           // =0，解码器处于工作状态；=非0，解码器处于空闲状态
    std::thread *decoder_thread_ = nullptr;

    int64_t next_pts;
    AVRational next_pts_tb;
    int64_t start_pts;
    AVRational start_pts_tb;
    int packet_pending_=0;    //用在解码线程缓存包packet的标志

    //**用于硬件加速解码缓存从显存中取出的帧数据内存**//
    AVFrame *software_frame=nullptr;
    ImageScaler *image_scaler_=nullptr;
};

class FFPlayer
{
public:
    explicit FFPlayer();
    ~FFPlayer();
    FFPlayer(FFPlayer&)=delete;
    FFPlayer& operator=(FFPlayer&)=delete;

    int ffp_create();                                               //创建FFPlayer
    void ffp_destroy();                                          //销毁FFPlayer
    int ffp_prepare_async_l(char *file_name);
    int read_thread();                                            //数据读取线程函数
   static int decode_interrupt_cb(void *ctx)//avformat_open_input read中断回调，避免网络堵塞无法退出
    {
        FFPlayer *p = static_cast<FFPlayer*>(ctx);
        return p->abort_request.load() ? 1 : 0;
    }
    /*判断数据包队列是否读满，从而让数据读取线程休眠，性能优化*/
    int stream_has_enough_packets(AVStream * st, int stream_id, PacketQueue * queue);

    // 播放控制
    int       ffp_start_l();
    int       ffp_pause_l();
    int       ffp_stop_l();
    int stream_open( const char *file_name);
    void stream_close();
    // 打开指定stream对应解码器、创建解码线程、以及初始化对应的输出
    int stream_component_open(int stream_index);
    // 关闭指定stream的解码线程，释放解码器资源
    void stream_component_close(int stream_index);

    //**Video Hardware decode  硬件加速解码相关**//
    enum AVHWDeviceType hw_type=AV_HWDEVICE_TYPE_NONE;
    bool Use_Hardware=false;   //是否启用硬件加速
    AVBufferRef *hw_device_ctx_buffer_ref=nullptr;     //硬件设备上下文
    bool initialize_hardware_acceleration(const AVCodec* codec);//初始化硬件加速

    ID3D11Device* m_qtDevice = nullptr;//D3D11设备存储
    void setD3D11Device(ID3D11Device *dev);//外部定义D3D11设备-接口
    //兼容qt6的d3d11设备渲染设备创建方案
    bool create_hw_device_ctx_from_qt_device();

    //**音频输出设备**//
    int audio_open(AVChannelLayout wanted_channel_layout,
                   int wanted_nb_channels, int wanted_sample_rate,
                   struct AudioParams *audio_hw_params);
    void audio_close();

    //**返回给Native层媒体文件总时长**//
    long ffp_get_duration_l();
    //**视频画面输出相关**//
    bool sureHaveVideo=false;
    int video_refresh_thread();
    void video_refresh(double *remaining_time);
    std::thread *video_refresh_thread_ = NULL;
    std::function<int(const Frame * , AVFrame *)> video_refresh_callback_ = NULL;
    void AddVideoRefreshCallback(std::function<int(const Frame *,AVFrame *)> callback);

    //*获取当前播放位置*//
    long ffp_get_current_position_l();

    //**seek相关**//
    int64_t seek_req = 0;
    int64_t seek_rel = 10;
    int64_t seek_flags = 0;
    int64_t seek_pos = 0;  // seek的位置
    //**执行媒体seek处理**//
    int ffp_seek_to_l(long msec);
    void stream_seek(int64_t pos, int64_t rel, int seek_by_bytes);

    //**变速相关**//
    float     pf_playback_rate = 1.0;           // 播放速率
    int        pf_playback_rate_changed = 0;   // 播放速率改变
    float     ffp_get_play_backrate();
    sonicStreamStruct *audio_speed_convert = nullptr;
    //**变速相关函数**//
    int get_target_frequency();
    int     get_target_channels();
    void ffp_set_playback_rate(float rate);
    float ffp_get_playback_rate();
    bool is_normal_playback_rate();
    int ffp_get_playback_rate_change();
    void ffp_set_playback_rate_change(int change);

    //**ABOUT MASTER CLOCK**//
    int get_master_sync_type();
    double get_master_clock();                                        //获取主时钟的当前时间戳
    int av_sync_type = AV_SYNC_AUDIO_MASTER;           // 音视频同步类型, 默认audio master

    Clock audclk;             // 音频时钟
    Clock	vidclk;             // 视频时钟
    double	audio_clock = 0;            // 当前音频帧的PTS+当前帧Duration,单位秒
    int             audio_clock_serial;     // 播放序列，seek可改变此值, 解码后保存
    double last_frame_timer;             // 记录最后一帧视频的播放的时刻
    double max_frame_duration;      // 一帧最大间隔. above this, we consider the jump a timestamp discontinuity
    int64_t         audio_callback_time = 0; //用于sdl_audio_callback时钟同步


    //**帧队列**//
    FrameQueue	pictq;             // 视频Frame队列
    FrameQueue	sampq;          // 采样Frame队列

    // **包队列**//
    PacketQueue audioq;             // 音频packet队列
    PacketQueue videoq;             // 视频packet队列
    std::atomic<int> abort_request{0};            // 各个数据缓存队列退出请求标志

    AVStream		*audio_st = NULL;              // 音频流
    AVStream		*video_st = NULL;              // 视频流

    //*播放结束判断*//
    void check_play_finished();
    int eof = 0;                                            //FFPlayer数据读取线程结束标志
    bool Audio_EOF= false;
    bool Video_EOF= false;
    int All_finished_ = 0;

    //**AVFormatContext**//
    AVFormatContext *ic = NULL;              //输入文件AVFormat上下文
    int audio_stream = -1;     //AVFormatContext *ic中音频流索引
    int video_stream = -1;     //AVFormatContext *ic中视频流索引

    //**Decoder**//
    Decoder auddec;             // 音频解码器
    Decoder viddec;              // 视频解码器

    //**单步运行**//
    int step = 0;
    int framedrop = -1;
    int frame_drops_late = 0;

    //**播放暂停控制相关**//
    int paused = 0;         //播放器暂停状态标志
    int pause_req = 0;      //上一次播放/暂停请求，用于再次启动时标志时钟应以实际数据的->pts更新
    int auto_resume = 0;
    int buffering_on = 0;
    int toggle_pause_(int pause_on);//触发一次播放/暂停请求
    void stream_update_pause_l();
    void stream_toggle_pause_l(int pause_on);

    //**音频输出相关**//
    SDL_AudioDeviceID audio_dev;          //音频设备ID
    struct AudioParams audio_src;           // 音频解码后的frame参数
    struct AudioParams audio_tgt;           // SDL支持的音频参数，重采样转换：audio_src->audio_tgt
    struct SwrContext *swr_ctx  = NULL;  // 音频重采样context
    int			audio_hw_buf_size = 0;       // SDL音频缓冲区的大小(字节为单位)
    int                   audio_write_buf_size;         //向SDL音频缓冲区实际写入的数据大小

     /* 指向需要重采样的数据，指向待播放的一帧音频数据，指向的数据区将被拷入SDL音频缓冲区。
        若经过重采样则指向audio_buf1，否则指向frame中的音频 */
    uint8_t			*audio_buf = NULL;
    uint8_t			*audio_buf1 = NULL;      // 指向重采样后的数据
    unsigned int		audio_buf_size = 0;    // 待播放的一帧音频数据(audio_buf指向)的大小
    unsigned int		audio_buf1_size = 0;  // 申请到的音频缓冲区audio_buf1的实际尺寸
    int			audio_buf_index = 0;         // 更新拷贝位置 当前音频帧中已拷入SDL音频缓冲区
    //*音量相关*//
    std::atomic<int> audio_volume=DEFAULT_VOLUM_SIZE;
    int startup_volume = DEFAULT_VOLUM_SIZE; // 起始音量
    void ffp_update_reqVolum(int value);

    MessageQueue msg_queue_;                        //消息队列实例，由FFPlayer管理
    char *input_filename_;                                   //播放文件资源名字

    std::thread *read_thread_;                             //数据读取线程

    //**计算上一帧需要持续的duration，校正算法，用于音视频同步在video_refresh中**//
    double vp_duration(Frame *vp, Frame *nextvp)
    {
        if (vp->serial == nextvp->serial)   // 同一播放序列，序列连续的情况下
        {
            double duration = nextvp->pts - vp->pts;
            if (isnan(duration) // duration 数值异常
                || duration <= 0    // pts值没有递增时
                || duration > max_frame_duration    // 超过了最大帧范围
                )
            {
                return vp->duration / pf_playback_rate;	 /* 异常时以帧时间为基准(1秒/帧率) */
            }
            else
            {
                return duration / pf_playback_rate; //使用两帧pts差值计算duration，一般情况下也是走的这个分支
            }
        }
        else
        {   // 不同播放序列, 序列不连续则直接返回0
            return 0.0;
        }
    }
    /*计算前一帧应该延迟多少,同步策略*/
    double compute_target_delay(double delay,Frame *last_vp,Frame *vp);
    /*更新视频时钟pts*/
    void update_video_pts(double pts, int64_t pos, int serial);
    //=======
};

//=================FFPlayer操作消息队列方法================
inline static void ffp_notify_msg(FFPlayer *ffp, int what) {
    msg_queue_put_simple(&ffp->msg_queue_, what, 0, 0);
}

inline static void ffp_notify_msg(FFPlayer *ffp, int what, int arg1) {
    msg_queue_put_simple(&ffp->msg_queue_, what, arg1, 0);
}

inline static void ffp_notify_msg(FFPlayer *ffp, int what, int arg1, int arg2) {
    msg_queue_put_simple(&ffp->msg_queue_, what, arg1, arg2);
}

inline static void ffp_notify_msg(FFPlayer *ffp, int what, int arg1, int arg2, void *obj, int obj_len) {
    msg_queue_put_simple(&ffp->msg_queue_, what, arg1, arg2, obj, obj_len);
}

inline static void ffp_remove_msg(FFPlayer *ffp, int what) {
    msg_queue_remove(&ffp->msg_queue_, what);
}



#endif // FFPLAYER_H
