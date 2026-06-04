#include "FFPlayer.h"

#include <string.h>
#include <atomic>
#include <d3d10_1.h> // for ID3D10Multithread

#include "FFPlay_Def.h"
#include "ffmsg.h"
#include "GlobalHelper.h"
#include "BilibiliAuthManager.h"
#include "SDL2/SDL_audio.h"
/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30
/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01  // 每帧休眠10ms
/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04  // 40ms
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.07   // 70ms
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.040

/**
 * @brief print_error     自定义调试输出工具
 * @param filename
 * @param err
 */
static void RET_ERROR_INFO(int err,const char *filename="")
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    if(strcmp(filename,""))
        av_log(NULL, AV_LOG_ERROR, "[AV_RET_ERROR_INFO:]%s\n", errbuf_ptr);
    else
        av_log(NULL, AV_LOG_ERROR, "[AV_RET_ERROR_INFO:]%s: %s\n", filename, errbuf_ptr);
}

/**
 * @brief hw_pix_fmt    用于video采用硬件加速时，记录硬件解码输出的pixelformat
 */
static enum AVPixelFormat hw_pix_fmt=AV_PIX_FMT_NONE;

/**
 * @brief get_hw_format --->for avctx->get_format 函数指针 用于协商输出格式-->让应用程序参与决定解码器输出的像素格式（hw_pix_fmt）
 * @param ctx
 * @param pix_fmts
 * @return
 */
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,const enum AVPixelFormat *pix_fmts)
{
    (void)ctx;
    const enum AVPixelFormat *p;
    for(p = pix_fmts;*p!=AV_PIX_FMT_NONE;p++){
        if(*p==hw_pix_fmt)
            return *p;
    }
    av_log(NULL,AV_LOG_ERROR,"[AV_LOG_ERROR:]Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

/**
 * @brief decoder_reorder_pts       =-1 ：记录解码帧的实际pts使用frame->best_effort_timestamp
 */
static const int decoder_reorder_pts=-1;

/**
 * @brief infinite_buffer   是否无限buffer空间
 */
static int infinite_buffer = 0;

static void safe_join(std::thread* t, const char* name)
{
    if (!t) return;
    if (!t->joinable()) return;

    if (t->get_id() == std::this_thread::get_id()) {
        INFO_LOG << "[safe_join] refuse to join self: " << name;
        return; // must can't join itself
    }

    INFO_LOG << "[safe_join] joining: " << name;
    t->join();
    INFO_LOG << "[safe_join] joined: " << name;
}

/**
 * @brief FFPlayer::FFPlayer
 */
FFPlayer::FFPlayer()
{

}

/**
 * @brief FFPlayer::~FFPlayer
 */
FFPlayer::~FFPlayer()
{
    this->abort_request = 1;

    if (ic && ic->pb)
        avio_closep(&ic->pb);
    if (read_thread_ && read_thread_->joinable())
        read_thread_->join();
    delete read_thread_;      // 释放thread内存
    read_thread_ = nullptr;

    if (m_qtDevice) m_qtDevice->Release();
    m_qtDevice=nullptr;
}

/**
 * @brief FFPlayer::ffp_create
 * @return
 */
int FFPlayer::ffp_create()
{
    INFO_LOG<<"FFplayer object create.";
    msg_queue_init(&msg_queue_);
    return 0;
}

/**
 * @brief FFPlayer::ffp_destroy
 */
void FFPlayer::ffp_destroy()
{
    stream_close();

    // 销毁消息队列
    msg_queue_destory(&msg_queue_);
}

/**
 * @brief FFPlayer::ffp_prepare_async_l
 * @param file_name
 * @return
 */
int FFPlayer::ffp_prepare_async_l(char *file_name)
{
    //保存文件名
    input_filename_ =  strdup(file_name);

    //启动ffplay总入口stream_open
    int reval = stream_open(file_name);

    return reval;
}

/**
 * @brief FFPlayer::read_thread
 * @return
 */
int FFPlayer::read_thread()
{
    int err, ret;
    int st_index[AVMEDIA_TYPE_NB];

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) return AVERROR(ENOMEM);

    AVDictionary *opts = nullptr;

    // ---出口清理 ---
    auto cleanup = [&]() {
        av_dict_free(&opts);
        if (pkt) av_packet_free(&pkt);
    };

    try
    {
        //将各个数据流索引初始化为-1,如果一直为-1说明没相应stream
        memset(st_index, -1, sizeof(st_index));
        video_stream = -1;
        audio_stream = -1;
        eof = 0;

        // 网络库初始化
        avformat_network_init();

        //创建上下文结构体，这个结构体是最上层的结构体，表示输入上下文
        ic = avformat_alloc_context();
        if (!ic)
        {
            ret = AVERROR(ENOMEM);
            throw std::runtime_error("Failed to avformat_alloc_context.");
        }
        // 中断回调：避免网络阻塞时无法退出
        ic->interrupt_callback.callback = decode_interrupt_cb;
        ic->interrupt_callback.opaque   = this;

        // --------- 网络流参数  BiliBili---------
        //b站网络连接头部构造
        av_dict_set(&opts, "user_agent",
           "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120 Safari/537.36",
           0);
        av_dict_set(&opts, "referer", "https://live.bilibili.com/", 0);
        //断流重连-直播
        av_dict_set(&opts, "reconnect", "1", 0);
        av_dict_set(&opts, "reconnect_streamed", "1", 0);
        av_dict_set(&opts, "reconnect_delay_max", "2", 0);

        //超时检测-避免卡死
        av_dict_set(&opts, "stimeout", "5000000", 0);
        av_dict_set(&opts, "rw_timeout", "5000000", 0);

        // --------- B站认证Cookie注入 ---------
        // 检测到B站直播流URL时，将Cookie注入FFmpeg HTTP请求头
        // 注意：使用headers选项时会覆盖user_agent/referer单独设置，所以需要包含完整头
        QString bilibiliCookies = BilibiliAuthManager::instance().ffmpegCookieString();
        if (!bilibiliCookies.isEmpty() && QString(input_filename_).contains("bilivideo.com")) {
            QString headers = QString(
                "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120 Safari/537.36\r\n"
                "Referer: https://live.bilibili.com/\r\n"
                "Cookie: %1\r\n"
            ).arg(bilibiliCookies);
            av_dict_set(&opts, "headers", headers.toUtf8().constData(), 0);
            NETWORK_LOG << "FFPlayer: Injected Bilibili auth cookies for stream playback";
        }
        // ---------------------------------------

        /*打开文件，主要是探测协议类型，如果是网络文件则创建网络链接等 */
        err = avformat_open_input(&ic, input_filename_ , NULL, &opts);
        if (err < 0)
        {
            RET_ERROR_INFO(err,input_filename_);
            ret = -1;
            throw std::runtime_error("Failed to avformat_open_input.");
        }

        ffp_notify_msg(this, FFP_MSG_OPEN_INPUT);
        INFO_LOG<<"Notify ffplayer message of ( FFP_MSG_OPEN_INPUT )";

        /*
     *  探测媒体类型，可得到当前文件的封装格式，音视频编码参数等信息
     * 调用该函数后得多的参数信息会比只调用avformat_open_input更为详细，
     * 其本质上是去做了decdoe packet获取信息的工作
     * codecpar, filled by libavformat on stream creation or
     * in avformat_find_stream_info()
     */
        err = avformat_find_stream_info(ic, NULL);
        if (err < 0)
        {
            av_log(NULL, AV_LOG_WARNING,
                   "%s: could not find codec parameters\n", input_filename_);
            ret = -1;
            throw std::runtime_error(std::string(input_filename_) + ": could not find codec parameters\n");
        }
        ffp_notify_msg(this, FFP_MSG_FIND_STREAM_INFO);
        INFO_LOG<<("Notify ffplayer message of ( FFP_MSG_FIND_STREAM_INFO )");


        av_dump_format(ic, 0, input_filename_, 0);

        // 降低FFmpeg内部日志级别，避免HLS直播拉流时大量INFO日志刷屏
        // av_dump_format之后设置，确保流信息dump正常输出
        av_log_set_level(AV_LOG_WARNING);

        /*利用av_find_best_stream选择流*/
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

        st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,st_index[AVMEDIA_TYPE_AUDIO],st_index[AVMEDIA_TYPE_VIDEO],NULL, 0);

        /*探测是否使用硬件解码video*/
        AVCodecParameters *codecpar = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codecpar;
        AVRational avg_frame_rate = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->avg_frame_rate;
        double fps = av_q2d(avg_frame_rate);
        this->Use_Hardware =
            // Condition-1: 4K resolution
            (codecpar->width >= 3840 && codecpar->height >= 2160) ||
            // Condition-2: 1080p + Height frame size(≥60fps)
            (codecpar->width >= 1920 && codecpar->height >= 1080 && fps >= 59.0) ||
            // Condition-3: 1080p + Specific codec format(HEVC/AV1/VP9)
            (codecpar->width >= 1920 && codecpar->height >= 1080 &&
             (codecpar->codec_id == AV_CODEC_ID_HEVC ||codecpar->codec_id == AV_CODEC_ID_AV1||codecpar->codec_id == AV_CODEC_ID_VP9))||
            // Condition-4 height of bit_rate 1080p video(>14Mbps)
            (codecpar->width >= 1920 && codecpar->height >= 1080 &&codecpar->bit_rate > 14000000);
        Use_Hardware = true;//<---此处强制启动硬件解码

        //* 直播流判定：不可 seek 基本就是 live *//
        bool is_live = false;
        if (ic->pb)
        {
            is_live = !(ic->pb->seekable & AVIO_SEEKABLE_NORMAL);
        }
        //* open the streams *//
        //* 打开视频、音频解码器。在此会打开相应解码器，并创建相应的解码线程。 *//
        if (st_index[AVMEDIA_TYPE_AUDIO] >= 0)  // 如果有音频流则打开音频流（音频解码线程在此内部创建）
        {
            stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
        }
        ret = -1;
        //**如果有视频流，并且该视频流disposition标准非媒体封面，并且视频流的duration也是有效的，则打开视频流（视频解码线程在此内部创建）
        if (st_index[AVMEDIA_TYPE_VIDEO] >= 0 &&
            !(ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->disposition & AV_DISPOSITION_ATTACHED_PIC) &&
            // ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->duration != AV_NOPTS_VALUE && //<---直播文件必为AV_NOPTS_VALUE
            // ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->nb_frames >= 5 &&//<---直播文件必为false
            ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codecpar->codec_id != AV_CODEC_ID_MJPEG &&
            ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codecpar->codec_id != AV_CODEC_ID_PNG &&
            ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codecpar->codec_id != AV_CODEC_ID_BMP)
        {
            /*同时要标志启动视频渲染刷新线程*/
            sureHaveVideo=true;
            last_frame_timer = av_gettime_relative() / 1000000.0;//渲染初始帧pts

            ret = stream_component_open( st_index[AVMEDIA_TYPE_VIDEO]);
        }
        else
        {
            INFO_LOG<<"Video Stream Componet not Open--->:";
            INFO_LOG<<"Video Stream of codec_id:"<<avcodec_get_name(ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codecpar->codec_id);
            INFO_LOG<<"Video Stream of isPicture:"<<((ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->disposition & AV_DISPOSITION_ATTACHED_PIC) ?"true" : "false");
            INFO_LOG<<"Video Stream of nb_frames:"<<ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->nb_frames;
            INFO_LOG<<"Video Stream of duration:"<<ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->duration;
        }
        ffp_notify_msg(this, FFP_MSG_COMPONENT_OPEN);
        INFO_LOG<<("Notify ffplayer message of ( FFP_MSG_COMPONENT_OPEN )");


        //*验证文件音频流视频流都存在*//
        if (video_stream < 0 && audio_stream < 0)
        {
            ret = -1;
            throw std::runtime_error("Failed to open file '"+std::string(input_filename_)+"' that video stream or audio stream not exist.");
        }
        //*根据推测是否要启动视频刷新线程*//
        if(video_stream>=0)
        {
            if (sureHaveVideo==true)
            {
                //--->非封面,并且视频流的duration!=AV_NOPTS_VALUE
                //--->创建视频刷新线程
                video_refresh_thread_ = new std::thread([this]() {
                    this->video_refresh_thread();
                });
            }
        }

        //*be init use by video_refresh for Synchronize audio calculate*//
        max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

        ffp_notify_msg(this, FFP_MSG_PREPARED);
        INFO_LOG<<("Notify ffplayer message of ( FFP_MSG_PREPARED )");

        // ---------------- 主循环：读包入队 ----------------
        while (1)
        {
            if(abort_request.load())
                break;
            /*req_seeking*/
            if(seek_req)
            {
                // seek：直播一般不支持 seek，这里直接拒绝
                if (is_live)
                {
                    // 直播不支持 seek：直接清掉请求并通知
                    seek_req = 0;
                    ffp_notify_msg(this, FFP_MSG_SEEK_COMPLETE);
                }
                else
                {
                    int64_t seek_target = seek_pos;
                    int64_t seek_min = seek_target;  // 确保最小位置不小于目标位置
                    int64_t seek_max = INT64_MAX; // 不设置上限
                    // 移除向后 seek 标志，强制向前 seek
                    seek_flags = seek_flags & ~AVSEEK_FLAG_BACKWARD;

                    ret = avformat_seek_file(ic, -1, seek_min, seek_target, seek_max,  seek_flags);
                    if(ret<0)
                    {
                        av_log(NULL, AV_LOG_ERROR,"%s: error while seeking\n", input_filename_);
                        // 允许向后 seek，但设置最小位置为 seek_target - tolerance
                        const int64_t tolerance = 10000; // 10秒容差（根据时间单位调整）10000ms
                        seek_min = seek_target - tolerance;
                        seek_flags = seek_flags | AVSEEK_FLAG_BACKWARD;
                        /*try again*/
                        continue;
                    }
                    else
                    {
                        /*send flush_packet for add 1 to player of serial*/
                        if (audio_stream >= 0)
                        {
                            packet_queue_flush(&audioq);
                            packet_queue_put(&audioq, &flush_pkt);
                        }
                        if (video_stream >= 0 && sureHaveVideo==true)
                        {
                            packet_queue_flush(&videoq);
                            packet_queue_put(&videoq, &flush_pkt);
                        }
                        seek_req = 0;
                        eof = 0;
                        ffp_notify_msg(this, FFP_MSG_SEEK_COMPLETE);
                    }
                }
            }
            /* if the queue are full, no need to read more */
            if (infinite_buffer < 1 &&
                (audioq.size + videoq.size  > MAX_QUEUE_SIZE ||
                 (stream_has_enough_packets(audio_st, audio_stream, &audioq) &&
                  stream_has_enough_packets(video_st, video_stream, &videoq) )))
            {
                /* wait 10 ms */
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            /*读取媒体数据，得到的是音视频分离后、解码前的数据*/
            ret = av_read_frame(ic, pkt); // 调用不会释放pkt的数据，需要我们自己去释放packet的数据
            if (ret == AVERROR(EAGAIN))
            {
                // 网络临时没数据
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if(ret < 0) // 出现异常或者已经读取完毕
            {
                if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !eof && !is_live)  // 数据读取完毕
                {
                    // 刷空包给队列
                    if (video_stream >= 0)
                        packet_queue_put_nullpacket(&videoq, video_stream);
                    if (audio_stream >= 0)
                        packet_queue_put_nullpacket(&audioq, audio_stream);
                    eof = 1;
                }
                SDL_CondSignal(sampq.cond);//通知不要再等待读取数据帧，直接退出
                SDL_CondSignal(pictq.cond);
                if (ic->pb && ic->pb->error)  // io异常 --->退出循环
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); //--->读取完数据了，这里休眠等待下一步的检测
                continue; // 继续循环
            }
            else
            {
                eof = 0;
            }
            //*插入音频包或视频包到数据包packet_queue队列*//
            if (pkt->stream_index == audio_stream)
            {
                packet_queue_put(&audioq, pkt);
            }
            else if (pkt->stream_index == video_stream && sureHaveVideo==true)
            {
                // static int put_video_pkt_num = 0;
                // put_video_pkt_num++;
                // av_log(NULL,AV_LOG_INFO,"ReadThread of put video packet num:%d\n\n",put_video_pkt_num);
                packet_queue_put(&videoq, pkt);
            }
            else
            {
                av_packet_unref(pkt);// 不入队列则直接释放数据
            }
        }
        INFO_LOG<<("Read thread Out");
        cleanup();
        return 0;
    }
    catch (const std::exception& e)
    {
        av_log(NULL, AV_LOG_FATAL, "[FFPlayer::read_thread:]ERROR Exception: %s\n", e.what());
        cleanup();
        return -1;
    }
}

/**
 * @brief FFPlayer::video_refresh_thread
 * @return
 */
int FFPlayer::video_refresh_thread()
{
    double remaining_time = 0.0;
    while (!abort_request.load())
    {
        if (remaining_time > 0.0)
            av_usleep((int)(int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        video_refresh(&remaining_time);
    }
    INFO_LOG<<("Video refresh thread Out");
    return 0;
}


#include <QElapsedTimer>
/**
 * @brief FFPlayer::video_refresh
 * @param remaining_time
 */
void FFPlayer::video_refresh(double *remaining_time)
{
    Frame *vp = NULL;
    Frame *lastvp = NULL;
    double last_duration, duration, delay;

    static int64_t run_times = 0;
    run_times+=1;
    //av_log(NULL, AV_LOG_INFO,"video_refresh run_times: %d\n",(int)run_times);

    // 判断有没有视频画面
    if(video_st)
    {
#if 1
        //====================
        //性能调试：
        static int64_t last_check_time = 0;
        int64_t current_time = av_gettime_relative();
        if (current_time - last_check_time > 10*1000000)// 10秒
        {
            last_check_time = current_time;
            av_log(NULL, AV_LOG_INFO,
                   "[AV_LOG_INFO:]--->VideoQ: packets:%d-bytes:%d, FrameQ: %d ,AlreadyDropFrame: %d\n",
                   videoq.nb_packets, videoq.size,
                   pictq.size,frame_drops_late);
        }
        //=====================
        //运行频率调试：
        // 每20秒打印一次刷新次数
        static int64_t last_check_time2 = 0;
        int64_t current_time2 = av_gettime_relative();
        if (current_time2 - last_check_time2 > 20*1000000)// 20秒
        {
            last_check_time2 = current_time2;
            //av_log(NULL, AV_LOG_INFO,":-->Video_refresh times 1 seconds of :%d\n",(int)run_times/20);
            run_times = 0;
        }
        //=====================
#endif
        static int discard_pkt_num=0;
        while(1)
        {
            if (frame_queue_nb_remaining(&pictq) == 0)
            {
                // 什么都不用做，可以直接退出了
                if(eof==1)
                {
                    Video_EOF=true;//视频播放结束
                }
                av_log(NULL, AV_LOG_WARNING,"[AV_LOG_WARNING:]-->Video frame queue now remaining of frame is Empty! InRunTime:%d\n\n",(int)run_times);
                *remaining_time = FFMIN(*remaining_time, 0.01); // 10ms
                return;
            }
            // 能跑到这里说明帧队列不为空，肯定有frame可以读取|-----------|同步丢包策略算法
            lastvp = frame_queue_peek_last(&pictq);//读取上一帧
            vp = frame_queue_peek(&pictq);  // 读取待显示帧
            if(vp->serial!=videoq.serial)
            {
                frame_queue_next(&pictq);   // 当前vp帧出队列，在内部已调用av_frame_unref
                INFO_LOG<<"VideoRefresh:Discard video frame because serial not same.-->vp->serial:"<<vp->serial<<"-video.pkt_serial_:"<<viddec.pkt_serial_;
                discard_pkt_num++;
                continue;
            }
            if(discard_pkt_num!=0)
            {
                INFO_LOG<<"VideoRefresh discard video frame num:"<<discard_pkt_num;
                discard_pkt_num=0;
            }
            if (lastvp->serial != vp->serial)
            {
                // 新的播放序列重置当前时间，记录最后一帧播放的时刻
                last_frame_timer = av_gettime_relative() / 1000000.0;
            }
            if (paused) {
                //INFO_LOG<<"Video Now is Pause.";
                return;
            }
            /* compute nominal last_duration */
            //lastvp上一帧，vp当前帧 ，nextvp下一帧
            //last_duration 计算上一帧应显示的时长
            last_duration = vp_duration(lastvp, vp);
            delay=compute_target_delay(last_duration,lastvp,vp);
            double time = av_gettime_relative() / 1000000.0;
            if (time <  last_frame_timer + delay)
            {
                *remaining_time = FFMIN( last_frame_timer + delay - time, *remaining_time);
                if (*remaining_time < 0.001) *remaining_time = 0.001;
                // INFO_LOG<<"time :"<<time;
                // INFO_LOG<<"last_frame_timer: "<<last_frame_timer;
                // INFO_LOG<<"delay :"<<delay;
                return;
            }
            last_frame_timer += delay;
            if (delay > 0 && time -  last_frame_timer > AV_SYNC_THRESHOLD_MAX)
                last_frame_timer = time;
            // SDL_LockMutex(pictq.mutex);
            if (!std::isnan(vp->pts))
            {
                update_video_pts(vp->pts, vp->pos, vp->serial);
                //printf("Update_video_pts of vp->serial is--->%d\n", vp->serial);
            }
            // SDL_UnlockMutex(pictq.mutex);
            if (frame_queue_nb_remaining(&pictq) > 1)
            {
                Frame *nextvp = frame_queue_peek_next(&pictq);
                duration = vp_duration(vp, nextvp);
                if (!step && (framedrop == 0 || (framedrop && get_master_sync_type() != AV_SYNC_VIDEO_MASTER))
                    && time > last_frame_timer + duration)
                {
                    frame_drops_late++;
                    frame_queue_next(&pictq);
                    INFO_LOG<<"Frame Drop. in RunTime:"<<run_times;
                    continue;
                }

            }
            break;
        }
        // static int c = 0;
        // c++;
        // av_log(NULL, AV_LOG_INFO,
        //        "[AV_LOG_INFO:]--->videoRefresh_Callback.%d\n",c);
        //**刷新显示**//
        /**
         *  @brief
         *  这里采用双引用计数帧策略：防止GUI线程视频渲染时调用rescale时正在使用frame->data数据，而视频刷新线程提前使用--->frame_queue_next(&pictq);---->提前释放frame->data，
         *                                          导致GUI线程使用空数据而引发的崩溃
         *  修复手法---------------->：这里多传入一个备份的renderRef并在GUI线程渲染函数内使用-->
         *                                                                                          std::unique_ptr<AVFrame, void(*)(AVFrame*)> frame_guard(renderRef, [](AVFrame* f) { av_frame_free(&f); });
         *                                                                                          从而可以确保在使用完数据后释放当前视频帧资源renderRef And renderRef->data
         *  @note uesing renderRef
         *///
        if(video_refresh_callback_ == nullptr)
        {
            WARNING_LOG<<"FFPlayer of video_refresh_callback_ is nullptr.";
        }
        if(video_refresh_callback_ && vp)
        {
            AVFrame *renderRef=nullptr;
            SDL_LockMutex(pictq.mutex);
            renderRef = av_frame_alloc();
            if (renderRef)
            {
                int rr = av_frame_ref(renderRef, vp->frame);
                if (rr < 0)
                {
                    av_frame_free(&renderRef);
                    renderRef = nullptr;
                }
            }
            SDL_UnlockMutex(pictq.mutex);

            // 锁外回调：renderRef 作为 copy_frame 传出去,--->renderRef 的释放权交给渲染侧
            int64_t last_current_time = av_gettime_relative();

            if (video_refresh_callback_ && vp)
            {
                int ret = video_refresh_callback_(vp, renderRef);
                if(ret <0) av_frame_free(&renderRef);
            }
            else
                if (renderRef) av_frame_free(&renderRef);

            int64_t next_current_time = av_gettime_relative();

            if(last_current_time - last_check_time >= 10 * 1000000)
            {
                last_check_time = last_current_time;
                //INFO_LOG<<"video_refresh_callback by "<<next_current_time -  last_current_time<<" us.";
            }
        }
        else
            WARNING_LOG<< "Attention video_refresh_callback_ function is NULL !!";
        // std::cout << "[Queue] Before next: size=" << pictq.size << std::endl;
        frame_queue_next(&pictq);
        // std::cout << "[Queue] After next: size=" << pictq.size << std::endl;
    }
    else
        WARNING_LOG<<"video_refresh_thread->:video_stream no exist.";
    return;
}

/**
 * @brief FFPlayer::AddVideoRefreshCallback
 * @param callback
 */
void FFPlayer::AddVideoRefreshCallback(
    std::function<int (const Frame *, AVFrame *)> callback)
{
    video_refresh_callback_ = callback;
}

/**
 * @brief FFPlayer::check_play_finished
 */
void FFPlayer::check_play_finished()
{
    if(eof==1&&All_finished_==0)
    {
        if(video_stream>0&&audio_stream>0)
        {
            if(Video_EOF==true&&Audio_EOF==true)
            {
                All_finished_=1;
                ffp_notify_msg(this,FFP_MSG_PLAY_FNISH);
            }
        }
        else if(video_stream>0)
        {
            if(Video_EOF==true)
            {
                All_finished_=1;
                ffp_notify_msg(this,FFP_MSG_PLAY_FNISH);
            }
        }
        else if(audio_stream>0)
        {
            if(Audio_EOF==true)
            {
                All_finished_=1;
                ffp_notify_msg(this,FFP_MSG_PLAY_FNISH);
            }
        }
        else
        {
            INFO_LOG<<"[FFPlayer::check_play_finished:]There is no audio_stream or video_stream!.";
            All_finished_=1;
            ffp_notify_msg(this,FFP_MSG_PLAY_FNISH);
        }
    }
}

int FFPlayer::get_master_sync_type()
{
    if (av_sync_type == AV_SYNC_VIDEO_MASTER)
    {
        if (video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;	 /* 如果没有视频成分则使用 audio master */
    }
    else if (av_sync_type == AV_SYNC_AUDIO_MASTER)
    {
        if (audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else if(video_st)
            return AV_SYNC_VIDEO_MASTER;        // 只有音频的存在
        else
            return AV_SYNC_UNKNOW_MASTER;
    }
    return 0;
}

double FFPlayer::get_master_clock()
{
    double val;

    switch (get_master_sync_type())
    {
    case AV_SYNC_VIDEO_MASTER:
        val = get_clock(&vidclk);
        break;
    case AV_SYNC_AUDIO_MASTER:
        val = get_clock(&audclk);
        break;
    default:
        val = get_clock(&audclk);
        break;
    }
    return val;
}

/**
 * @brief FFPlayer::ffp_start_l
 * @return
 */
int FFPlayer::ffp_start_l()
{
    INFO_LOG << "Start palying......";
    toggle_pause_(0);// 触发播放
    return 0;
}

/**
 * @brief FFPlayer::ffp_pause_l
 * @return
 */
int FFPlayer::ffp_pause_l()
{
    toggle_pause_(1);
    return 0;
}

/**
 * @brief FFPlayer::ffp_stop_l
 * @return
 */
int FFPlayer::ffp_stop_l()
{
    abort_request = 1;  // 请求退出
    msg_queue_abort(&msg_queue_);  // 禁止再插入消息
    return 0;
}

/**
 * @brief FFPlayer::stream_open
 * @param file_name
 * @return
 */
int FFPlayer::stream_open(const char *file_name)
{
    (void)file_name;

    try
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
        {
            av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
            av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
            return -1;
        }
        // 初始化Frame帧队列
        if (frame_queue_init(&pictq, &videoq, VIDEO_PICTURE_QUEUE_SIZE_DEFAULT,1) < 0)
            throw std::runtime_error("Cannot initialize picture queue for video.");
        if (frame_queue_init(&sampq, &audioq, SAMPLE_QUEUE_SIZE,1) < 0)
            throw std::runtime_error("Cannot initialize sample queue for audio.");


        // 初始化Packet包队列serial设为0
        if (packet_queue_init(&videoq,AVMEDIA_TYPE_VIDEO) < 0)
            throw std::runtime_error("Cannot initialize video packet queue.");

        if (packet_queue_init(&audioq,AVMEDIA_TYPE_AUDIO) < 0 )
            throw std::runtime_error("Cannot initialize auido packet queue.");


        // 初始化时钟-->时钟序列c->queue_serial，实际上指向的是is->videoq.serial
        init_clock(&vidclk, &videoq.serial);
        init_clock(&audclk, &audioq.serial);
        audio_clock_serial = -1;

        // 初始化音量等
        startup_volume = av_clip(startup_volume, 0, 100);
        startup_volume = av_clip(SDL_MIX_MAXVOLUME *  startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
        audio_volume =  startup_volume;

        // 创建解复用器读数据线程read_thread
        read_thread_ = new std::thread(&FFPlayer::read_thread, this);

        // 创建视频刷新线程--->move to read_thread
        // video_refresh_thread_ = new std::thread([this]() {
        //     this->video_refresh_thread();
        // });
    }
    catch (const std::exception& e)
    {
        av_log(NULL,AV_LOG_FATAL,"[FFPlayer::stream_open:]Failed to stream open for %s : %s",file_name,e.what());
        stream_close();
        return -1;
    }
    return 0;
}

/**
 * @brief FFPlayer::stream_close
 */
void FFPlayer::stream_close()
{
    abort_request = 1; // 请求退出
    SDL_CondSignal(sampq.cond);
    SDL_CondSignal(pictq.cond);

    if (read_thread_) {
        safe_join(read_thread_, "read_thread");
        delete read_thread_;
        read_thread_ = nullptr;
    }

    /* close each stream */
    if (audio_stream >= 0)
        stream_component_close(audio_stream);  // 解码器线程请求abort的时候，已调用 packet_queue_abort
    if (video_stream >= 0)
        stream_component_close(video_stream);

    if(ic)avformat_close_input(&ic);

    // 释放packet队列
    packet_queue_destroy(&videoq);
    packet_queue_destroy(&audioq);
    // 释放frame队列
    frame_queue_destory(&pictq);
    frame_queue_destory(&sampq);

    if(input_filename_)
    {
        free(input_filename_);
        input_filename_ = NULL;
    }
    destory_clock(&audclk);
    destory_clock(&vidclk);
}

/**
 * @brief FFPlayer::initialize_hardware_acceleration
 * @param codec
 * @param hw_type
 * @param hw_pix_fmt
 * @return
 */
 bool FFPlayer::initialize_hardware_acceleration(const AVCodec *codec)
{
    /*根据探测结果启用硬件加速*/
    for(int lib_index=0;Hardware_Support_Library[lib_index]!=nullptr; lib_index++)
    {
        const char * hw_devcie_name = Hardware_Support_Library[lib_index];
        enum AVHWDeviceType cur_hw_type=av_hwdevice_find_type_by_name(hw_devcie_name);
        if(cur_hw_type == AV_HWDEVICE_TYPE_NONE)
        {
            //*Skip Current lib_index of library not supported*//
            continue;
        }
        //匹配支持的Config
        for(int config_index=0;;config_index++)
        {
            //*查询特定编解码器支持的硬件加速配置*//
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec,config_index);
            if(!config)//搜索到末尾--->无符合要求的硬件加速配置
            {
                av_log(NULL,AV_LOG_FATAL,"[INFO]Decoder %s does not support device type %s --->Failed to avcodec_get_hw_config.\n"
                       ,codec->name,av_hwdevice_get_type_name(cur_hw_type));
                return false;
            }
            //*检测硬件设备特定要求*//
            if(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == cur_hw_type)
            {
                // hw_pix_fmt = config->pix_fmt;
                // //*验证设备可用性*//
                // AVBufferRef * hw_device_ctx =nullptr;
                // int ret = av_hwdevice_ctx_create(&hw_device_ctx,cur_hw_type,nullptr,nullptr,0);
                // if(ret >= 0)
                // {
                //     hw_type = cur_hw_type;
                //     hw_pix_fmt = config->pix_fmt;
                //     av_buffer_unref(&hw_device_ctx);
                //     return true;
                // }
                // else
                // {
                //     av_log(NULL,AV_LOG_WARNING,"Hardware Device of %s could not be used.\n",hw_devcie_name);
                //     //*try next hardware device*//
                //     break;
                // }
                hw_type = cur_hw_type;
                hw_pix_fmt = config->pix_fmt;
                INFO_LOG<<"Successed initialize_hardware_acceleration hardware pixelFormat is :"<<av_get_pix_fmt_name((AVPixelFormat)config->pix_fmt);
                return true;
            }
            //==for config
        }
        //==for library
    }
    return false;
}

// 成员：ID3D11Device* m_qtDevice = nullptr; (AddRef/Release 自己管理)
void FFPlayer::setD3D11Device(ID3D11Device *dev)
{
    if (m_qtDevice == dev) return;
    if (m_qtDevice) m_qtDevice->Release();
    m_qtDevice = dev;
    if (m_qtDevice)
    {
        m_qtDevice->AddRef();
        INFO_LOG<<"FFPlayer Successed add m_qtDevice Ref.";
    }
}

bool FFPlayer::create_hw_device_ctx_from_qt_device()
{
    if (!m_qtDevice) return false;
    //很多情况下 FFmpeg/渲染线程都会触碰 D3D11 device/context，多线程保护能减少莫名崩溃
    Microsoft::WRL::ComPtr<ID3D10Multithread> mt;
    if (SUCCEEDED(m_qtDevice->QueryInterface(__uuidof(ID3D10Multithread), (void**)mt.GetAddressOf()))) {
        mt->SetMultithreadProtected(TRUE);
    }

    AVBufferRef *hw = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!hw) return false;

    AVHWDeviceContext *hwctx = (AVHWDeviceContext*)hw->data;
    auto *d3d = (AVD3D11VADeviceContext*)hwctx->hwctx;

    //把 Qt 的 device 填进去（必须由用户设置）:contentReference[oaicite:5]{index=5}
    d3d->device = m_qtDevice;
    d3d->device->AddRef();

    int err = av_hwdevice_ctx_init(hw);
    if (err < 0) {
        av_buffer_unref(&hw);
        return false;
    }

    // 保存到成员 hw_device_ctx_buffer_ref
    hw_device_ctx_buffer_ref = hw;
    return true;
}


/**
 * @brief FFPlayer::stream_component_open
 * @param stream_index
 * @return
 */
int FFPlayer::stream_component_open(int stream_index)
{
    AVCodecContext *avctx;
    const AVCodec *codec;
    AVChannelLayout av_ch_layout;
    int sample_rate;
    //int nb_channels;            //desperate
    //int64_t channel_layout;//desperate
    int ret = 0;

    try
    {
        // 判断stream_index是否合法
        if (stream_index < 0 || stream_index >= (int)ic->nb_streams)
            return -1;
        /*  为解码器分配一个编解码器上下文结构体 */
        avctx = avcodec_alloc_context3(NULL);
        if (!avctx)
            return AVERROR(ENOMEM);

        /* 将码流中的编解码器信息拷贝到新分配的编解码器上下文结构体 */
        ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
        if (ret < 0)
        {
            throw std::runtime_error("Failed to copy stream of codecpar to AVCodecContext !");
        }
        // 设置pkt_timebase
        avctx->pkt_timebase = ic->streams[stream_index]->time_base;

        /* 根据codec_id查找解码器 */
        codec = avcodec_find_decoder(avctx->codec_id);
        // codec = avcodec_find_decoder_by_name("");
        if (!codec)
        {
            av_log(NULL, AV_LOG_WARNING,
                   "No decoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));
            ret = AVERROR(EINVAL);
            throw std::runtime_error(std::string("Failed to find decoder "+ std::string(avcodec_get_name(avctx->codec_id))+" !"));
        }
        else av_log(NULL,AV_LOG_INFO,"[INFO]Sueccess to find deocder : -->Stream index:%d  -->Decoder name:%s\n ",stream_index,avcodec_get_name(codec->id));

        // ret = avcodec_open2(avctx, codec, NULL);
        // if(ret <0)
        // {
        //     throw std::runtime_error("Faild to open Audio decoder.");
        // }
        switch (avctx->codec_type)
        {
        case AVMEDIA_TYPE_AUDIO:
            //**OPEN DECODER**//
            ret = avcodec_open2(avctx, codec, NULL) < 0;
            if(ret <0)
            {
                throw std::runtime_error("Faild to open Audio decoder.");
            }
            //从avctx(即AVCodecContext)中获取音频格式参数
            if (avctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            {
                av_channel_layout_default(&av_ch_layout, avctx->ch_layout.nb_channels);
            }
            else
            {
                av_channel_layout_copy(&av_ch_layout, &avctx->ch_layout);
            }
            sample_rate=avctx->sample_rate;
            /* prepare audio output 准备音频输出设备*/
            //调用audio_open打开sdl音频输出，实际打开的设备参数保存在audio_tgt，返回值表示输出设备的缓冲区大小
            if ((ret = audio_open(av_ch_layout, av_ch_layout.nb_channels, sample_rate, &audio_tgt)) < 0)
            {
                throw std::runtime_error("Failed to open SLD_AudioDevice.");
            }
            audio_hw_buf_size = ret;//SDL音频缓冲区的大小(字节为单位)
            audio_src = audio_tgt;   //暂且将数据源参数等同于目标输出参数
            /* 初始化audio_buf相关参数 */
            audio_buf_size  = 0;
            audio_buf_index = 0;


            audio_stream = stream_index;    // 获取audio的stream索引
            audio_st = ic->streams[stream_index];  // 获取audio的stream指针

            // 初始化ffplay封装的音频解码器, 并将解码器上下文 avctx和Decoder绑定
            auddec.decoder_init(avctx, &audioq);
            // 启动音频解码线程
            auddec.decoder_start(AVMEDIA_TYPE_AUDIO, "audio_thread", this);
            if ((ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !ic->iformat->read_seek) {
                auddec.start_pts = audio_st->start_time;
                auddec.start_pts_tb = audio_st->time_base;
            }
            // 允许音频输出
            /*play audio*/
            // SDL_PauseAudioDevice(audio_dev, 0);//已移动到ffp_pause_l()
            break;
        case AVMEDIA_TYPE_VIDEO:
            video_stream = stream_index;    // 获取video的stream索引
            video_st = ic->streams[stream_index];// 获取video的stream指针
            //*根据探测结果启用硬件加速*//
            // if(Use_Hardware == true)  // 2026/1/27   -->Discard
            // {
            //     //*initialize_hardware_acceleration for get "hw_type" and "hw_pix_fmt"*//
            //     Use_Hardware = initialize_hardware_acceleration(codec);
            //     if(Use_Hardware ==true)
            //     {
            //         bool ok = create_hw_device_ctx_from_qt_device();//兼容qt渲染d3d11
            //         // ret = av_hwdevice_ctx_create(&hw_device_ctx_buffer_ref,hw_type,nullptr,nullptr,0);//默认使用
            //         if(!ok)
            //         {
            //             av_log(NULL, AV_LOG_FATAL, "Failed to create D3D11VA hwdevice from Qt device\n");
            //             Use_Hardware = false;
            //         }
            //         //为硬件设备上下文增加一个引用计数,使用hw_device_ctx_buffer_ref初始化
            //         else
            //         {
            //             avctx->get_format = get_hw_format;
            //             avctx->hw_device_ctx = av_buffer_ref(hw_device_ctx_buffer_ref);
            //         }
            //     }
            //     if(Use_Hardware == true)
            //     {
            //         av_log(NULL,AV_LOG_INFO,"Successed to initialize hardware device of %s --->And hardware pixelfotmat:%s\n"
            //                ,av_hwdevice_get_type_name(hw_type),av_get_pix_fmt_name(hw_pix_fmt));
            //     }
            // }
            if (Use_Hardware)//2026/1/27 -->New
            {
                //*initialize_hardware_acceleration for get "hw_type" and "hw_pix_fmt"*//
                Use_Hardware = initialize_hardware_acceleration(codec); // 设置 hw_type/hw_pix_fmt
                if (Use_Hardware)
                {
                    if (hw_type == AV_HWDEVICE_TYPE_D3D11VA)
                    {
                        bool ok = create_hw_device_ctx_from_qt_device();
                        if (!ok)
                        {
                            av_log(NULL, AV_LOG_FATAL, "Failed to create D3D11VA device from Qt D3D11 device\n");
                            Use_Hardware = false;
                        }
                        else
                        {
                            avctx->get_format = get_hw_format;//传递协商获取格式-->函数指针
                            //为硬件设备上下文增加一个引用计数,使用hw_device_ctx_buffer_ref初始化
                            avctx->hw_device_ctx = av_buffer_ref(hw_device_ctx_buffer_ref);
                            INFO_LOG<<"Successed create D3D11VA hardware device form qt device.";
                        }
                    }
                    else
                    {
                        // 未来支持其它 hwtype，就走原来的 create
                        // ret = av_hwdevice_ctx_create(&hw_device_ctx_buffer_ref,hw_type,nullptr,nullptr,0);//默认使用
                    }
                }
            }
            //**OPEN DECODER**//
            ret = avcodec_open2(avctx, codec, NULL) < 0;
            if(ret <0)
            {
                throw std::runtime_error("Faild to open Video decoder.");
            }
            // 初始化ffplay封装的视频解码器
            viddec.decoder_init(avctx, &videoq); //  is->continue_read_thread
            // 启动视频频解码线程
            if ((ret = viddec.decoder_start(AVMEDIA_TYPE_VIDEO, "video_decoder", this)) < 0)
            {
                throw std::runtime_error("Failed to start video decode thread !");
            }
            break;
        default:
            break;
        }
    }
    catch (const std::exception& e)
    {
        if(avctx)avcodec_free_context(&avctx);
        av_log(NULL,AV_LOG_FATAL,"Failed to stream componment open:%s",e.what());
    }
    return ret;
}

/**
 * @brief FFPlayer::stream_component_close
 * @param stream_index
 */
void FFPlayer::stream_component_close(int stream_index)
{
    AVCodecParameters *codecpar;

    if (stream_index < 0 || stream_index >= (int)ic->nb_streams)
        return;
    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type)
    {
    case AVMEDIA_TYPE_AUDIO:
        INFO_LOG<< "Stream component close AVMEDIA_TYPE_AUDIO";
        // 请求终止解码器线程
        auddec.decoder_abort(&sampq);

        /*关闭音频设备*/
        audio_close();

        // 销毁解码器
        auddec.decoder_destroy();

        // 释放重采样器
        swr_free(&swr_ctx);
        // 释放audio buf
        av_freep(&audio_buf1);
        audio_buf1_size = 0;
        audio_buf = NULL;
        break;
    case AVMEDIA_TYPE_VIDEO:
    {
        INFO_LOG << "Stream component close AVMEDIA_TYPE_VIDEO (begin)";

        viddec.decoder_abort(&pictq);
        SDL_CondSignal(pictq.cond);
        SDL_CondSignal(sampq.cond);

        video_refresh_callback_ = nullptr;

        INFO_LOG << "joinable=" << (video_refresh_thread_ && video_refresh_thread_->joinable());
        safe_join(video_refresh_thread_, "video_refresh_thread");

        viddec.decoder_destroy();

        INFO_LOG << "Stream component close AVMEDIA_TYPE_VIDEO (end)";
        break;
    }
    default:
        break;
    }

    //    ic->streams[stream_index]->discard = AVDISCARD_ALL;  //
    switch (codecpar->codec_type)
    {
    case AVMEDIA_TYPE_AUDIO:
        audio_st = NULL;
        audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        video_st = NULL;
        video_stream = -1;
        break;
    default:
        break;
    }
}

/**
 * Decode one audio frame and return its uncompressed size.
 *
 * The processed audio frame is decoded, converted if required, and
 * stored in is->audio_buf, with size in bytes given by the return
 * value.
 * 从数据帧队列取出一帧，可能需要经过重采样，
 */
/**
 * @brief audio_decode_frame
 * @param is
 * @return
 */
static int audio_resample_frame(FFPlayer *is)
{
    int data_size, resampled_data_size;
    //int64_t dec_channel_layout; //desperate
    AVChannelLayout dec_channel_layout;
    // 定义输入和输出的 AVChannelLayout
    AVChannelLayout in_ch_layout, out_ch_layout;
    int wanted_nb_samples;
    Frame *af;
    int ret = 0;

    dec_channel_layout.order = AV_CHANNEL_ORDER_UNSPEC;
    dec_channel_layout.nb_channels = 0;
    in_ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
    in_ch_layout.nb_channels = 0;
    out_ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
    out_ch_layout.nb_channels = 0;

    try
    {
        if(is->paused)return -1;

        //从数据帧队列中读取一帧数据
        while(1)
        {
            //====================
            //性能调试：
            // 每10秒打印一次队列状态
            static int64_t last_check_time = 0;
            int64_t current_time = av_gettime_relative();
            if (current_time - last_check_time > 30*1000000)// 30秒
            {
                last_check_time = current_time;
                av_log(NULL, AV_LOG_INFO,
                       "[AV_LOG_INFO:]--->AudioQ: packets:%d-bytes:%d, FrameQ: %d\n",
                       is->audioq.nb_packets, is->audioq.size,
                       is->sampq.size);
            }
            //=====================
            // std::cout<<"Continue peek readable"<<std::endl;
            if (frame_queue_nb_remaining(&is->sampq) == 0)
            {
                // 什么都不用做，可以直接退出了
                if(is->eof==1)
                {
                    is->Audio_EOF=true;//播放结束
                }
                return -1;
            }
            // 若队列头部可读，则由af指向可读帧
            if (!(af = frame_queue_peek_readable(&is->sampq)) )
            {
                return -1;
                // std::cout<<"Exit peek readable================"<<std::endl;
            }
            if(af->serial != is->audioq.serial)//不同播放序列的直接清理
            {
                // std::cout<<"Continue frame queue next================"<<std::endl;
                frame_queue_next(&is->sampq);//rindex+rindex_shown
            }
            else break;
        }

        // 根据frame中指定的音频参数获取缓冲区的大小 af->frame->channels * af->frame->nb_samples * 2
        data_size = av_samples_get_buffer_size(NULL,
                                               af->frame->ch_layout.nb_channels,
                                               af->frame->nb_samples, // 一帧总采样点数
                                               (enum AVSampleFormat)af->frame->format, 1);
        // 获取声道布局
        if (af->frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        {
            av_channel_layout_default(&dec_channel_layout, af->frame->ch_layout.nb_channels);
        }
        else
        {
            av_channel_layout_copy(&dec_channel_layout, &af->frame->ch_layout);
        }
        // 获取样本数校正值：若同步时钟是音频，则不调整样本数；否则根据同步需要调整样本数
        //    wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);  // 目前不考虑非音视频同步的是情况
        wanted_nb_samples = af->frame->nb_samples;


        // is->audio_tgt是SDL可接受的音频帧数，是audio_open()中取得的参数
        // 在audio_open()函数中又有"is->audio_src = is->audio_tgt""
        // 此处表示：如果frame中的音频参数 == is->audio_src == is->audio_tgt，
        // 那音频重采样的过程就免了(因此时is->swr_ctr是NULL)
        // 否则使用frame(源)和is->audio_tgt(目标)中的音频参数来设置is->swr_ctx，
        // 并使用frame中的音频参数来赋值is->audio_src//av_channel_layout_compare(&layout1, &layout2) == 0
        if (af->frame->format           != is->audio_src.fmt                                                    || // 采样格式
            av_channel_layout_compare(&dec_channel_layout,&is->audio_src.ch_layout) ||     // 通道布局
            af->frame->sample_rate  != is->audio_src.freq ||                                                   // 采样率
            (wanted_nb_samples != af->frame->nb_samples && !is->swr_ctx) )
        {
            swr_free(&is->swr_ctx);// 先释放旧的
            // 创建新的 SwrContext
            is->swr_ctx = swr_alloc();
            if (!is->swr_ctx) {
                ret = -1;
                throw std::runtime_error("Failed to allocate SwrContext");
            }

            // 从解码器帧获取输入通道布局（FFmpeg 6.0+）
            av_channel_layout_copy(&in_ch_layout, &af->frame->ch_layout);

            // 设置目标输出通道布局（is->audio_tgt.ch_layout 已经初始化）
            av_channel_layout_copy(&out_ch_layout, &is->audio_tgt.ch_layout);

            // 使用 swr_alloc_set_opts2() 初始化 SwrContext
            ret = swr_alloc_set_opts2(
                &is->swr_ctx,              // 传入 SwrContext 指针
                &out_ch_layout,          // 目标输出通道布局
                is->audio_tgt.fmt,       // 目标输出格式
                is->audio_tgt.freq,      // 目标输出采样率
                &in_ch_layout,            // 输入通道布局
                (enum AVSampleFormat)af->frame->format,  // 输入格式
                af->frame->sample_rate, // 输入采样率
                0,                                      // 日志偏移
                NULL                                // 日志上下文
                );

            if (!is->swr_ctx || swr_init(is->swr_ctx) < 0)
            {
                // av_log(NULL, AV_LOG_ERROR,
                //        "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                //        af->frame->sample_rate, av_get_sample_fmt_name((enum AVSampleFormat)af->frame->format),
                //        af->frame->ch_layout.nb_channels,
                //        is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt),
                //        is->audio_tgt.ch_layout.nb_channels);
                swr_free(&is->swr_ctx);
                ret = -1;
                throw std::runtime_error("Cannot create sample rate converter for conversion of "
                                         +std::to_string(af->frame->sample_rate)+"Hz "+std::string(av_get_sample_fmt_name((enum AVSampleFormat)af->frame->format))
                                         +" "+std::to_string(af->frame->ch_layout.nb_channels)+"channels "
                                         +"to "+std::to_string(is->audio_tgt.freq)+" Hz "+std::string(av_get_sample_fmt_name(is->audio_tgt.fmt))
                                         +" "+std::to_string(is->audio_tgt.ch_layout.nb_channels)+" channels!");
            }
            // 释放临时 AVChannelLayout
            av_channel_layout_uninit(&in_ch_layout);
            av_channel_layout_uninit(&out_ch_layout);

            //更新源格式
            av_channel_layout_uninit(&is->audio_src.ch_layout);
            if (av_channel_layout_copy(&is->audio_src.ch_layout, &dec_channel_layout) < 0) {
                throw std::runtime_error("Failed to cpoy dec_channedl_layout to is->audio_src.ch_layout.");
            }
            is->audio_src.channels       = af->frame->ch_layout.nb_channels;
            is->audio_src.freq = af->frame->sample_rate;
            is->audio_src.fmt = (enum AVSampleFormat)af->frame->format;
        }

        if (is->swr_ctx)
        {
            // 重采样输入参数1：输入音频样本数是af->frame->nb_samples
            // 重采样输入参数2：输入音频缓冲区
            const uint8_t **in = (const uint8_t **)af->frame->extended_data; // data[0] data[1]

            // 重采样输出参数1：输出音频缓冲区
            uint8_t **out = &is->audio_buf1; //真正分配缓存audio_buf1，指向是用audio_buf

            // 重采样输出参数2：输出音频缓冲区尺寸， 高采样率往低采样率转换时得到更少的样本数量，比如 96k->48k, wanted_nb_samples=1024
            // 则wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate 为1024*48000/96000 = 512
            // +256 的目的是重采样内部是有一定的缓存，就存在上一次的重采样还缓存数据和这一次重采样一起输出的情况，所以目的是多分配输出buffer
            int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate
                            + 4096;
            // 计算对应的样本数 对应的采样格式 以及通道数，需要多少buffer空间
            int out_size  = av_samples_get_buffer_size(NULL, is->audio_tgt.channels,
                                                      out_count, is->audio_tgt.fmt, 0);
            int len2;
            if (out_size < 0){
                ret = -1;
                throw std::runtime_error("Failed to av_samples_get_buffer_size()");
            }
            // if(audio_buf1_size < out_size) {重新分配out_size大小的缓存给audio_buf1, 并将audio_buf1_size设置为out_size }
            av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
            if (!is->audio_buf1) {
                ret = AVERROR(ENOMEM);
                throw std::runtime_error("Failed to av_fast_malloc() is->audio_buf1.");
            }
            // 音频重采样：len2返回值是重采样后得到的音频数据中单个声道的样本数
            len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
            if (len2 < 0) {
                ret = -1;
                throw std::runtime_error("Failed to swr_convert().");
            }

            if (len2 == out_count)//多分配的count buffer，实际输出的样本数不应该超过多分配的数量
            {
                av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
                if (swr_init(is->swr_ctx) < 0)
                    swr_free(&is->swr_ctx);
            }
            // 重采样返回的一帧音频数据大小(以字节为单位)
            is->audio_buf = is->audio_buf1;
            resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
        }
        else
        {
            // 未经重采样，则将指针指向frame中的音频数据
            is->audio_buf = af->frame->data[0]; // s16交错模式data[0], fltp data[0] data[1]
            resampled_data_size = data_size;
        }
        if (!isnan(af->pts))
            is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
        else
            is->audio_clock = NAN;

        frame_queue_next(&is->sampq);       // 才会真正释放frame的引用
        is->audio_clock_serial = af->serial;
        ret = resampled_data_size;
    }
    catch (const std::exception& e)
    {
        av_log(NULL,AV_LOG_FATAL,"[Static::audio_decode_frame:]Accour Error in audio_decode_frame:%s\n",e.what());
        av_channel_layout_uninit(&in_ch_layout);
        av_channel_layout_uninit(&out_ch_layout);
        av_channel_layout_uninit(&is->audio_src.ch_layout);
    }
    return ret;
}

/* prepare a new audio buffer */
/**
 * @brief sdl_audio_callback        回调函数，由SDL音频设备请求调用，获取音频数据
 * @param opaque   指向user的数据    本程序中在audio_open中传入了FFPlayer对象
 * @param stream    拷贝PCM的地址
 * @param len          SDL音频设备，请求需要拷贝的长度
 */
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    FFPlayer *is = (FFPlayer *)opaque;
    int audio_size, len1;
    is->audio_callback_time = av_gettime_relative();
    // 现在设置是每次需要2048个采样点
    // len = 8192
    // len 4096
    // len 0
    /* 回调函数主循环读取音频数据，直到读取到足够长度的请求数据len*/
    /* 在循环中会取出音频数据帧队列中的数据(可能会经过重采样)，每取出一定长度的数据对应会减少SDL音频设备需求长度len的值，直到len<=0 */
    /** (1)如果is->audio_buf_index < is->audio_buf_size则说明上次拷贝还剩余一些数据，
          先拷贝到stream再调用audio_decode_frame
          (2)如果audio_buf消耗完了，则调用audio_decode_frame重新填充audio_buf
          [可以先假设is->audio_buf_index==is->audio_buf_size==0开始理解过程]
    */
    while (len > 0)
    {
        if(is->abort_request.load()==1)break;
        if(is->Audio_EOF==true)
        {
            /*检测是否播放结束*/
            is->check_play_finished();
            continue;
        }
        if (is->audio_buf_index >= static_cast<int>(is->audio_buf_size))
        {
            audio_size = audio_resample_frame(is); // 返回有效的PCM数据长度
            if (audio_size < 0)
            {
                // 静音的逻辑
                /* if error, just output silence */
                is->audio_buf = NULL;
                is->audio_buf_size = is->audio_tgt.frame_size * SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size;
                if(is->eof )
                {
                    is->Audio_EOF=true;//音频播放结束
                    //std::cout<<"===========================================================================================AudioEoF"<<std::endl;
                    continue;
                }
            }
            else
            {
                is->audio_buf_size = audio_size; //单位：字节
            }
            is->audio_buf_index = 0;                // 重置为0
            /*变速请求处理*/
            if(is->ffp_get_playback_rate_change())
            {
                INFO_LOG<<"[sdl_audio_callback:]Speed initialize deal;\n";
                is->ffp_set_playback_rate_change(0);
                // 初始化
                if(is->audio_speed_convert)
                {
                    // 先释放
                    sonicDestroyStream(is->audio_speed_convert);
                }
                // 再创建
                is->audio_speed_convert = sonicCreateStream(is->get_target_frequency(),is->get_target_channels());
                // 设置变速系数
                sonicSetSpeed(is->audio_speed_convert, is->ffp_get_playback_rate());
                sonicSetPitch(is->audio_speed_convert, 1.0);
                sonicSetRate(is->audio_speed_convert, 1.0);
            }
            /*调用sonic库，处理变速对应audio_buf内音频数据*/
            if(!is->is_normal_playback_rate()&&is->audio_buf)
            {
                // 需要修改  audio_buf_index ； audio_buf_size ；audio_buf
                int actual_out_samples = is->audio_buf_size /
                                         (is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt));
                // 计算处理后的点数
                int out_ret = 0;
                int out_size = 0;
                int num_samples = 0;
                int sonic_samples = 0;
                if(is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT)
                {
                    out_ret = sonicWriteFloatToStream(is->audio_speed_convert,
                                                      (float *)is->audio_buf,
                                                      actual_out_samples);
                }
                else  if(is->audio_tgt.fmt == AV_SAMPLE_FMT_S16)
                {
                    out_ret = sonicWriteShortToStream(is->audio_speed_convert,
                                                      (short *)is->audio_buf,
                                                      actual_out_samples);
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "sonic unspport ......\n");
                }
                num_samples =  sonicSamplesAvailable(is->audio_speed_convert);
                //目前只支持2通道的
                out_size = (num_samples) * av_get_bytes_per_sample(is->audio_tgt.fmt) * is->audio_tgt.channels;
                av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
                if(out_ret)
                {
                    // 从流中读取处理好的数据
                    if(is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT)
                    {
                        sonic_samples = sonicReadFloatFromStream(is->audio_speed_convert,
                                                                 (float *)is->audio_buf1,
                                                                 num_samples);
                    }
                    else  if(is->audio_tgt.fmt == AV_SAMPLE_FMT_S16) {
                        sonic_samples = sonicReadShortFromStream(is->audio_speed_convert,
                                                                 (short *)is->audio_buf1,
                                                                 num_samples);
                    }
                    else
                    {
                        INFO_LOG<< "[ERROR]sonic unspport fmt: " << is->audio_tgt.fmt;
                    }
                    is->audio_buf = is->audio_buf1;
                    // std::cout << "mdy num_samples: " << num_samples<<std::endl;
                    // std::cout << "orig audio_buf_size: " << is->audio_buf_size<<std::endl;
                    is->audio_buf_size = sonic_samples * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
                    //                    LOG(INFO) << "mdy audio_buf_size: " << audio_buf_size;
                    is->audio_buf_index = 0;
                }
            }
        }
        if(is->audio_buf_size == 0) continue;
        //根据缓冲区剩余大小量力而行
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;

        if (is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
            memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        else
        {
            /*First slience then be mix Audio if have data*/
            memset(stream,0,len1);
            if(is->audio_buf)
                SDL_MixAudioFormat(stream, (uint8_t *)is->audio_buf + is->audio_buf_index,
                                   AUDIO_S16SYS, len1, is->audio_volume.load());
        }
        len -= len1;            // 读了多少数据后，len要减少相应的长度
        stream += len1;     // stream 拷贝的位置也发生偏移
        /* 更新is->audio_buf_index，指向audio_buf中未被拷贝到stream的数据（剩余数据）的起始位置 */
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    if (!std::isnan(is->audio_clock))
    {
        double audio_clock = is->audio_clock / is->ffp_get_play_backrate();
        set_clock_at(&is->audclk,
                     audio_clock  - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec,//SDL音频采用双队列缓冲区
                     is->audio_clock_serial,
                     is->audio_callback_time / 1000000.0);
    }
}

/**
 * @brief FFPlayer::audio_open
 * @param wanted_channel_layout
 * @param wanted_nb_channels
 * @param wanted_sample_rate
 * @param audio_hw_params
 * @return
 */
int FFPlayer::audio_open(AVChannelLayout wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, AudioParams *audio_hw_params)
{
    (void)wanted_channel_layout;//--->Discard
    SDL_AudioSpec wanted_spec,spec;
    // 音频参数设置SDL_AudioSpec
    wanted_spec.freq = wanted_sample_rate;          // 采样频率
    wanted_spec.format = AUDIO_S16SYS;              // 采样点格式
    wanted_spec.channels = wanted_nb_channels;  // 通道数
    wanted_spec.silence = 0;                                    // 是否静音
    //    wanted_spec.samples = 2048;       // 23.2ms -> 46.4ms 每次读取的采样数量，多久产生一次回调和 samples
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE,
                                2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback; // 回调函数
    wanted_spec.userdata = this;

    /* SDL_OpenAudioDevice */
    //打开音频设备SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE
    if(!(audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE)))
    {
        INFO_LOG<<"[ERROR]Failed to open audio device(SDL_GetError()): "<<std::string(SDL_GetError());
        return -1;
    }
    // wanted_spec是期望的参数，spec是实际的参数，wanted_spec和spec都是SDL中的结构。
    // 此处audio_hw_params是FFmpeg中的参数，输出参数供上级函数使用
    // audio_hw_params保存的参数，就是在做重采样的时候要转成的格式。
    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    av_channel_layout_default(&audio_hw_params->ch_layout,spec.channels);
    audio_hw_params->channels =  spec.channels;
    /* audio_hw_params->frame_size这里只是计算一个采样点占用的字节数 */
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels,
                                                             1,
                                                             audio_hw_params->fmt, 1);
    // 1秒需要的字节数
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels,
                                                                audio_hw_params->freq,      // 44100
                                                                audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    // 如2帧数据，一帧就是1024个采样点， 1024*2*2 * 2 = 8192字节 (1024采样点，2通道，每个采样点2字节(AV_SAMPLE_FMT_S16)，2帧)
    return spec.size;	/* SDL内部缓存的数据字节, samples * channels *byte_per_sample */
}

/**
 * @brief FFPlayer::audio_close
 */
void FFPlayer::audio_close()
{
    SDL_LockAudioDevice(audio_dev);

    SDL_PauseAudioDevice(audio_dev, 1);
    SDL_ClearQueuedAudio(audio_dev);
    SDL_CloseAudioDevice(audio_dev);        // SDL_CloseAudioDevice

    SDL_UnlockAudioDevice(audio_dev);

    INFO_LOG<<"SDL Audio deviceo be closed.";
}

Decoder::Decoder()
{
    pkt_.data = nullptr;
    pkt_.size = 0;
}

Decoder::~Decoder()
{
    if(software_frame)av_frame_free(&software_frame);
}

/**
 * @brief Decoder::decoder_init
 * @param avctx
 * @param queue
 */
void Decoder::decoder_init(AVCodecContext *avctx, PacketQueue *queue)
{
    avctx_ = avctx;
    queue_ = queue;
}

/**
 * @brief Decoder::decoder_start
 * @param codec_type
 * @param thread_name
 * @param arg
 * @return
 */
int Decoder::decoder_start(AVMediaType codec_type, const char *thread_name, void *arg)
{
    (void)thread_name;
    // 启用包队列
    packet_queue_start(queue_);
    // 创建音频视频解码线程
    if(AVMEDIA_TYPE_VIDEO == codec_type)
        decoder_thread_ = new std::thread(&Decoder::video_thread, this, arg);
    else if (AVMEDIA_TYPE_AUDIO == codec_type)
        decoder_thread_ = new std::thread(&Decoder::audio_thread, this, arg);
    else
        return -1;
    return 0;
}

/**
 * @brief Decoder::decoder_abort
 * @param fq
 */
void Decoder::decoder_abort(FrameQueue *fq)
{
    packet_queue_abort(queue_);     // 请求退出包队列
    frame_queue_signal(fq);     // 唤醒阻塞的帧队列
    if(decoder_thread_ && decoder_thread_->joinable())
    {
        decoder_thread_->join(); // 等待解码线程退出
        delete decoder_thread_;
        decoder_thread_ = NULL;
    }
    packet_queue_flush(queue_);  // 刷新packet队列，并释放数据
}

void Decoder::decoder_destroy()
{
    if(avctx_)avcodec_free_context(&avctx_);
}


/**
 * @brief Decoder::decode_packet_to_frame     使用AVCodec解码包packet获取帧frame,以及根据视频序列号serial丢包策略
 *             此函数属于深层私有函数，在本程序中由audio_thread/video_thread中调用
 * @param frame
 * @return -1: 请求退出
 *                0: 解码已经结束了，不再有数据可以读取
 *                1: 获取到解码后的frame
 */
int Decoder::decode_packet_to_frame(AVFrame *frame)
{
    int ret = AVERROR(EAGAIN);
    /*总解码循环*/
    while (1)
    {
        AVPacket pkt;
        pkt.data = nullptr;
        pkt.size = 0;
        /*在seek操作后，serial会立刻加一，这时候解码线程仍在运行，要根据实时serial是否相等来立即阻断解码运行，立刻前往avcodec_flush_buffers(avctx_);*/
        if(queue_->serial==pkt_serial_)
        {
            /*第一个循环 先把codec里的frame 全部读取*/
            do {
                if (queue_->abort_request)      // decoder_abort调用的时候 触发queue_->abort_request为1
                    return -1;  // 请求退出解码
                switch (avctx_->codec_type)
                {
                case AVMEDIA_TYPE_VIDEO:
                    ret = avcodec_receive_frame(avctx_, frame);
                    if (ret >= 0)
                    {
                        if(frame->pts == AV_NOPTS_VALUE)
                        {
                            printf("Warning:--->Video decode of avcodec_receive_frame return frame->pts is AV_NOPTS_VALUE!!\n\n");
                        }
                        if (decoder_reorder_pts == -1)
                            frame->pts = frame->best_effort_timestamp;
                        else if (decoder_reorder_pts)
                            frame->pts = frame->pkt_dts;

                        if(frame->pts == AV_NOPTS_VALUE)
                        {
                            printf("Warning:--->Return frame->pts is AV_NOPTS_VALUE!!\n\n");
                        }
                    }
                    else
                    {
                        char errStr[256] = { 0 };
                        av_strerror(ret, errStr, sizeof(errStr));
                    }
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    ret = avcodec_receive_frame(avctx_, frame);
                    if (ret >= 0)
                    {
                        AVRational tb{1, frame->sample_rate};
                        if (frame->pts != AV_NOPTS_VALUE)
                        {
                            // 如果frame->pts正常则先将其从pkt_timebase转成{1, frame->sample_rate}
                            // pkt_timebase实质就是stream->time_base
                            frame->pts = av_rescale_q(frame->pts, avctx_->pkt_timebase, tb);
                        }
                        else if (next_pts != AV_NOPTS_VALUE)//帧前预估pts
                        {
                            // 如果frame->pts不正常则使用上一帧更新的next_pts和next_pts_tb
                            // 转成{1, frame->sample_rate}
                            frame->pts = av_rescale_q(next_pts, next_pts_tb, tb);
                        }
                        if(frame->pts != AV_NOPTS_VALUE)//预估->帧后pts
                        {
                            next_pts=frame->pts+frame->nb_samples;
                            next_pts_tb=tb;
                        }
                    }
                    else
                    {
                        char errStr[256] = { 0 };
                        av_strerror(ret, errStr, sizeof(errStr));
                        //                        printf("audio dec:%s, ret:%d,%d\n", errStr, ret, AVERROR(EAGAIN));
                    }
                    break;
                default:
                    av_log(NULL,AV_LOG_WARNING,"UnSupported AVContexte->Codec->Type.");
                    break;
                }
                /*检测ret是否请求直接退出解码decoder_decode_frame，否则ret==AVERROR(EAGAIN)->进行获取packet环节*/
                // 1.3. 检查解码是否已经结束，解码结束返回0
                if (ret == AVERROR_EOF)
                {
                    INFO_LOG<<"AVCodec be flushed inner buffers";
                    avcodec_flush_buffers(avctx_);
                    return 0;
                }
                // 1.4. 正常解码返回1
                if (ret >= 0)
                    return 1;// 成功获取到一帧frame，先退出返回出frame
            }while (ret != AVERROR(EAGAIN));   //没帧可读时ret返回EAGIN，需要继续送packet
        }


        do{
            if(queue_->nb_packets<=0)
            {
                SDL_CondSignal(queue_->cond);//唤醒数据包packet读取，
            }
            if(packet_pending_)
            {
                /*使用预先缓存packet*/
                av_packet_move_ref(&pkt, &pkt_);
                packet_pending_=0;
            }
            else/*阻塞式读取packet*/
            {
                if (packet_queue_get(queue_, &pkt, 1, &pkt_serial_) < 0)//获取pkt_serial_
                    return -1;
                //printf("pkt_serial be value to --->%d\n",pkt_serial_);
            }
            if(queue_->serial != pkt_serial_)/*非相同序列号，丢包处理，后循环重新读取*/
            {
                INFO_LOG<< "Discarded packet:queue_->serial:" << queue_->serial << ", pkt_serial_:" << pkt_serial_;
                av_packet_unref(&pkt);
            }
        }while(queue_->serial!=pkt_serial_);

        if(pkt.data==flush_pkt.data)
        {
            avcodec_flush_buffers(avctx_);//清空解码器缓存帧
            finished_=0;                            //重置数据包packet解码完成
            next_pts = start_pts;           // 主要用在了audio
            next_pts_tb = start_pts_tb;// 主要用在了audio
        }
        else
        {
            if(avctx_->codec_type == AVMEDIA_TYPE_SUBTITLE)
            {
                //--->字幕流分支
            }
            else//否则就是video / audio packt
            {
                if (avcodec_send_packet(avctx_, &pkt) == AVERROR(EAGAIN))
                {
                    // av_log(avctx_, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    packet_pending_ = 1;
                    av_packet_move_ref(&pkt_, &pkt);
                }
            }
        }
        //pkt已经发送至解码器，解引用刚处理的packet音视频数据，即释放旧数据
        av_packet_unref(&pkt);
    }
}

/**
 * @brief Decoder::get_video_frame
 * @param frame
 * @return
 */
int Decoder::get_video_frame(AVFrame *frame)
{
    int ret;

    try
    {
        // 获取解码后的视频帧
        if ((ret = decode_packet_to_frame(frame)) < 0)
        {
            return -1;//返回-1------>退出解码线程
        }
        else
        {
            //**硬件加速检测，缓存转移，格式转换操作**//
            if(frame->format==hw_pix_fmt)
            {
                // if(!software_frame)
                //     software_frame = av_frame_alloc();//首次帧数据CPU缓存内存分配
                // if(!software_frame)
                //     throw std::runtime_error("Failed to allocate software frame.");
                // av_frame_unref(software_frame);

                // const int64_t pts = (frame->pts != AV_NOPTS_VALUE)
                //                         ? frame->pts
                //                         : frame->best_effort_timestamp;
                // const int64_t pkt_dts = frame->pkt_dts;

                // //* retrieve data from GPU to CPU 转换硬解码的数据   转移出来后看sw_frame->format ，一般是AV_PIX_FMT_NV12*//
                // // auto start = std::chrono::high_resolution_clock::now();
                // ret = av_hwframe_transfer_data(software_frame, frame, 0);
                // // auto end = std::chrono::high_resolution_clock::now();
                // // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                // // INFO_LOG << "Transfer time:" << duration.count() << "us";
                // if(!software_frame->data[0] || !software_frame->linesize[0] || ret < 0)
                // {
                //     throw std::runtime_error("Failed to transfer video frame data from hardware to software.");
                // }

                // //copy srcframe params!!
                // av_frame_copy_props(software_frame, frame);
                // software_frame->pts = pts;
                // software_frame->best_effort_timestamp = pts;
                // software_frame->pkt_dts = pkt_dts;

                // //*解引用GPU显存缓存的数据帧frame*//
                // av_frame_unref(frame);
                // //*将转换的CPU缓存的数据帧转移引用到frame指针传出*//
                // av_frame_move_ref(frame, software_frame);
                static bool logged = false;
                if (!logged) {
                    logged = true;
                    av_log(NULL, AV_LOG_INFO, "[AV_LOG_INFO:]-->Decoder::get_video_frame:decoded frame format=%s\n",
                           av_get_pix_fmt_name((AVPixelFormat)frame->format));
                }
                return 1;//返回1------>成功获取一帧硬件帧
            }
        }
    }
    catch (const std::exception& e)
    {
        if (software_frame)
        {
            av_frame_unref(software_frame);
        }
        av_log(NULL,AV_LOG_FATAL,"Failed to Get video frame:%s.\n",e.what());
        return 0;//返回0------>通知外部重新取帧
    }
    return 1;//返回1------>成功获取一帧
}

/**
 * @brief Decoder::queue_picture    此函数用于将src_frame解码后的数据帧写入数据帧队列中
 * @param fq
 * @param src_frame
 * @param pts
 * @param duration
 * @param pos
 * @param serial
 * @return
 */
int Decoder::queue_picture(FrameQueue *frameq, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    (void)pos;

    Frame *vp;
    if (!(vp = frame_queue_peek_writable(frameq))) // 检测队列是否有可写空间
        return -1;      // 请求退出则返回-1

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;
    //printf("new queue picture frame serial is:--->%d\n",vp->serial);

    SDL_LockMutex(frameq->mutex);
    av_frame_move_ref(vp->frame, src_frame); // 将src中所有数据转移到dst中，并复位src。
    SDL_UnlockMutex(frameq->mutex);
    frame_queue_push(frameq);   // 更新写索引位置
    return 0;
}

/**
 * @brief Decoder::audio_thread
 * @param arg
 * @return
 */
int Decoder::audio_thread(void *arg)
{
    INFO_LOG<< "Audio decode thread has into";
    FFPlayer *is = (FFPlayer *)arg;
    AVFrame *frame = av_frame_alloc();  // 分配解码帧
    Frame *af;
    AVRational tb;        // timebase
    int got_frame = 0;  // 是否读取到帧
    int ret = 0;

    if (!frame)
        return AVERROR(ENOMEM);
    /*音频解码线程主解码循环*/
    do {
        if(is->seek_req==1)
            continue;
        //**读取解码帧
        if ((got_frame = decode_packet_to_frame(frame)) < 0)   // 是否获取到一帧数据 --->got_frame <=0 abort
        {
            break;
        }
        if (got_frame)
        {
            tb = {1, frame->sample_rate};   // 设置为sample_rate为timebase
            //**获取可写Frame
            if (!(af = frame_queue_peek_writable(&is->sampq)))  // 获取可写位置数据帧指针
                break;
            // **设置Frame并放入FrameQueue
            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);  // 转换成单位为秒的pts
            af->pos = frame->pkt_pos;
            af->serial = is->auddec.pkt_serial_;
            af->duration = av_q2d({frame->nb_samples, frame->sample_rate});

            av_frame_move_ref(af->frame, frame);// 成功插入一帧数据
            frame_queue_push(&is->sampq);// 将写指针移向下一位置
        }
    } while ((ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF )&& ret!=-1);

    INFO_LOG<<"Audio decode thread has leaved ";
    av_frame_free(&frame);
    return ret;
}

/**
 * @brief Decoder::video_thread
 * @param arg
 * @return
 */
int Decoder::video_thread(void *arg)
{
    INFO_LOG<< "Video decode thread has into";
    FFPlayer *is = (FFPlayer *)arg;
    AVFrame *frame = av_frame_alloc();  // 分配解码帧
    //AVFrame *temp_frame =av_frame_alloc(); //分配software转移帧
    double pts;                 // pts
    double duration;        // 帧持续时间
    int ret;
    //获取stream timebase
    AVRational tb = is->video_st->time_base; // 获取stream timebase
    //获取帧率，以便计算每帧picture的duration
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);

    if (!frame)
        return AVERROR(ENOMEM);
    /*视频解码线程主循环，循环取出视频解码的帧数据*/
    while(1)
    {
        av_frame_unref(frame);
        //获取解码后的视频帧
        ret = get_video_frame(frame);
        if (ret < 0)
            break;               //解码结束, 解码器无解码帧返回EOF解码结束，或无法从原文件解复用后在数据读取线程读取出数据包
        if (!ret)                  //没有解码得到画面, 什么情况下会得不到解后的帧
            continue;

        //计算帧持续时间和换算pts值为秒    1/25 = 0.04秒
        // 1/帧率 = duration 单位秒, 没有帧率时则设置为0, 有帧率帧计算出帧间隔
        duration = (frame_rate.num && frame_rate.den ? av_q2d(/*AVRational*/{frame_rate.den, frame_rate.num}) : 0);
        // 根据AVStream timebase计算出pts值, 单位为秒
        if(frame->pts == AV_NOPTS_VALUE)
        {
            //printf("Warning:get_video_frame - frame->pts === AV_NOPTS_VALUE!\n");
        }
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);

        static int put_video_hw_frame_num = 0;
        put_video_hw_frame_num++;
        //av_log(NULL, AV_LOG_INFO, "put_video_hw_frame_num :%d\n",put_video_hw_frame_num);
        // 将解码后的视频帧插入队列
        ret = queue_picture(&is->pictq, frame, pts, duration, frame->pkt_pos, is->viddec.pkt_serial_);
        // 释放临时frame对应的
        av_frame_unref(frame);

        if (ret < 0) // 返回值小于0则退出线程
            break;
    }

    INFO_LOG<<"Video decode thread has leaved ";
    av_frame_free(&frame);
    return 0;
}

/**
 * @brief FFPlayer::toggle_pause_
 * @param pause_on
 * @return
 */
int FFPlayer::toggle_pause_(int pause_on)
{
    if(pause_req && !pause_on)//如果上次是请求的暂停，这次要正确更新时钟pts
    {
        set_clock(&vidclk,get_clock(&vidclk),vidclk.serial);
        set_clock(&audclk,get_clock(&audclk),audclk.serial);
    }
    pause_req = pause_on;//更新上一次暂停请求标志
    auto_resume = ! pause_on;
    stream_update_pause_l();
    step = 0;
    return 0;
}

/**
 * @brief FFPlayer::stream_update_pause_l
 */
void FFPlayer::stream_update_pause_l()
{
    if (!step && (pause_req || buffering_on))//是否单步运行
    {
        stream_toggle_pause_l(1);
    }
    else
    {
        stream_toggle_pause_l(0);
    }
}

/**
 * @brief FFPlayer::stream_toggle_pause_l
 * @param pause_on
 */
void FFPlayer::stream_toggle_pause_l(int pause_on)
{
    if (paused && !pause_on)//如果当前是暂停状态，并且将要启动，这次要正确更新时钟pts
    {
        last_frame_timer += av_gettime_relative() / 1000000.0 - vidclk.last_updated;
        set_clock(&vidclk, get_clock(&vidclk), vidclk.serial);
        set_clock(&audclk, get_clock(&audclk), audclk.serial);
    }
    if (step && (pause_req || buffering_on))
    {
        paused = vidclk.paused = pause_on;
    }
    else
    {
        paused = audclk.paused = vidclk.paused =  pause_on;
    }
    if(paused==1)
    {
        SDL_LockAudioDevice(audio_dev);
        SDL_PauseAudioDevice(audio_dev,1);
        SDL_UnlockAudioDevice(audio_dev);
    }
    else
    {
        SDL_LockAudioDevice(audio_dev);
        SDL_PauseAudioDevice(audio_dev,0);
        SDL_UnlockAudioDevice(audio_dev);
    }
}

/**
 * @brief FFPlayer::ffp_update_reqVolum
 * @param value
 * @return
 */
void FFPlayer::ffp_update_reqVolum(int value)
{
    // std::cout<<"req volum:"<<value<<std::endl;
    value = av_clip(SDL_MIX_MAXVOLUME *  value * 1.0 / 100, 0, SDL_MIX_MAXVOLUME);
    DEFAULT_VOLUM_SIZE = value;
    this->audio_volume=value;
    // /std::cout<<"now volum:"<<value<<std::endl;
}

/**
 * @brief FFPlayer::compute_target_delay  Video同步PTS算法Alogrithm
 * @param delay
 * @param last_vp
 * @param vp
 * @return      Get this frame reamin need to delay " "
 */
double FFPlayer::compute_target_delay(double delay,Frame *last_vp,Frame *vp)
{
    double sync_threshold, diff = 0;
    /* update delay to follow master synchronisation source */
    if (get_master_sync_type() != AV_SYNC_VIDEO_MASTER)
    {
        /* if video is slave, we try to correct big delays by
        duplicating or deleting a frame */
        // 计算出待显示帧vp需要等待的时间
        double videoClockCur_pts=get_clock(&vidclk);
        if(last_vp->serial != vp->serial)
            diff = NAN;
        else
            diff = videoClockCur_pts - get_master_clock();
        /* skip or repeat frame. We take into account the
        delay to compute the threshold. I still don't know
        if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (! std::isnan(diff) && fabs(diff) <  max_frame_duration)
        {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
        //printf("video: delay=%0.3f video_pts:%0.3f audio_pts:%0.3 A-V=%f\n",delay,videoClockCur_pts,get_master_clock(), -diff);
    }
    return delay;
}

/**
 * @brief FFPlayer::ffp_get_play_backrate
 * @return
 */
float FFPlayer::ffp_get_play_backrate()
{
    return pf_playback_rate;
}

/**
 * @brief FFPlayer::ffp_get_duration_l
 * @return
 */
long FFPlayer::ffp_get_duration_l()
{
    if(!ic)return 0;
    int64_t duration=fftime_to_milliseconds(ic->duration);
    if(duration<0)return 0;
    return static_cast<long>(duration);
}

/**
 * @brief FFPlayer::ffp_seek_to_l
 * @param msec
 * @return
 */
int FFPlayer::ffp_seek_to_l(long msec)
{
    int64_t start_time = 0;
    int64_t seek_pos = milliseconds_to_fftime(msec);
    int64_t duration = milliseconds_to_fftime(ffp_get_duration_l());
    if (duration > 0 && seek_pos >= duration)
    {
        ffp_notify_msg(this, FFP_MSG_SEEK_COMPLETE);        // 超出了范围
        return 0;
    }
    start_time =  ic->start_time;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE)
    {
        seek_pos += start_time;
    }
    stream_seek(seek_pos, 0, 0);
    return 0;
}

/**
 * @brief FFPlayer::stream_seek
 * @param pos
 * @param rel
 * @param seek_by_bytes
 */
void FFPlayer::stream_seek(int64_t pos, int64_t rel, int seek_by_bytes)
{
    if (!seek_req)
    {
        seek_pos = pos;
        seek_rel = rel;
        seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
        {
            seek_flags |= AVSEEK_FLAG_BYTE;
        }
        seek_req = 1;
        //        SDL_CondSignal( continue_read_thread);
    }
}

/**
 * @brief FFPlayer::stream_has_enough_packets
 * @param st
 * @param stream_id
 * @param queue
 * @return
 */
int FFPlayer::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue)
{
    return stream_id<0 ||queue->abort_request==1 ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
           (queue->nb_packets > MAX_PACKET_QUEUE_NUM && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0));
}

/**
 * @brief FFPlayer::ffp_get_current_position_l   return m
 * @return
 */
long FFPlayer::ffp_get_current_position_l()
{
    return static_cast<long>(ffp_get_current_position_d());
}

/**
 * @brief FFPlayer::ffp_get_current_position_d   返回浮点秒精度
 * @return
 */
double FFPlayer::ffp_get_current_position_d()
{
    if(!ic)return 0.0;
    int64_t start_time = ic->start_time;
    double start_diff_sec = 0.0;
    if(start_time >0 && start_time!=AV_NOPTS_VALUE)
    {
        start_diff_sec = fftime_to_milliseconds(start_time) / 1000.0;
    }
    double sec_pos = 0.0;
    double fftime_pos_clock = get_master_clock();  // 获取当前时钟
    if(std::isnan(fftime_pos_clock))
    {
        sec_pos = fftime_to_milliseconds(seek_pos) / 1000.0;
    }
    else
    {
        sec_pos = fftime_pos_clock;//单位秒
    }
    if(sec_pos < 0.0 || sec_pos < start_diff_sec) return 0.0;
    double adjust_sec_pos = sec_pos - start_diff_sec;
    return adjust_sec_pos * pf_playback_rate;
}

/**
 * @brief FFPlayer::update_video_pts
 * @param pts
 * @param pos
 * @param serial
 */
void FFPlayer::update_video_pts(double pts, int64_t pos, int serial)
{
    (void)pos;
    /* update current video pts */
    set_clock(&vidclk, pts / pf_playback_rate, serial);
}

/**
 *              变速相关辅助函数
 */
/**
 * @brief FFPlayer::get_target_frequency
 * @return
 */
int FFPlayer::get_target_frequency()
{
    return audio_tgt.freq;
}

/**
 * @brief FFPlayer::get_target_channels
 * @return
 */
int FFPlayer::get_target_channels()
{
    return audio_tgt.channels;
}

/**
 * @brief FFPlayer::ffp_set_playback_rate
 * @param rate
 */
void FFPlayer::ffp_set_playback_rate(float rate)
{
    pf_playback_rate = rate;
    pf_playback_rate_changed = 1;
}

/**
 * @brief FFPlayer::ffp_get_playback_rate
 * @return
 */
float FFPlayer::ffp_get_playback_rate()
{
    return pf_playback_rate;
}

/**
 * @brief FFPlayer::is_normal_playback_rate
 * @return
 */
bool FFPlayer::is_normal_playback_rate()
{
    if(pf_playback_rate > 0.99 && pf_playback_rate < 1.01)
        return true;
    else
    {
        return false;
    }
}

/**
 * @brief FFPlayer::ffp_get_playback_rate_change
 * @return
 */
int FFPlayer::ffp_get_playback_rate_change()
{
    return pf_playback_rate_changed;
}

/**
 * @brief FFPlayer::ffp_set_playback_rate_change
 * @param change
 */
void FFPlayer::ffp_set_playback_rate_change(int change)
{
    pf_playback_rate_changed = change;
}

