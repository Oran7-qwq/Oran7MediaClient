#include "Oran7FFmpegDecoderCore.h"

#include "Oran7FFplayerCore.h"       // audio_thread/video_thread 访问 Oran7FFplayerCore 内部状态，需完整类型
#include "Oran7GlobalHelper.h"   // INFO_LOG
#include <stdio.h>          // printf（原依赖 Oran7FFplayerCore.cpp 传递引入，此处显式补齐）

/* ====== Oran7FFmpegDecoderCore 静态成员定义（唯一定义点） ====== */
enum AVPixelFormat Oran7FFmpegDecoderCore::s_hw_pix_fmt = AV_PIX_FMT_NONE;

/* ====== 本文件私有常量 ====== */
/**
 * @brief decoder_reorder_pts  =-1 ：记录解码帧的实际pts使用 frame->best_effort_timestamp
 *       原为 Oran7FFplayerCore.cpp 文件级 static，随 Oran7FFmpegDecoderCore 实现一同迁入本文件。
 */
static const int decoder_reorder_pts = -1;

Oran7FFmpegDecoderCore::Oran7FFmpegDecoderCore()
{
    pkt_.data = nullptr;
    pkt_.size = 0;
}

Oran7FFmpegDecoderCore::~Oran7FFmpegDecoderCore()
{
    if(software_frame)av_frame_free(&software_frame);
}

/**
 * @brief Oran7FFmpegDecoderCore::decoder_init
 * @param avctx
 * @param queue
 */
void Oran7FFmpegDecoderCore::decoder_init(AVCodecContext *avctx, PacketQueue *queue)
{
    avctx_ = avctx;
    queue_ = queue;
}

/**
 * @brief Oran7FFmpegDecoderCore::decoder_start
 * @param codec_type
 * @param thread_name
 * @param arg
 * @return
 */
int Oran7FFmpegDecoderCore::decoder_start(AVMediaType codec_type, const char *thread_name, void *arg)
{
    (void)thread_name;
    // 启用包队列
    packet_queue_start(queue_);
    // 创建音频视频解码线程
    if(AVMEDIA_TYPE_VIDEO == codec_type)
        decoder_thread_ = new std::thread(&Oran7FFmpegDecoderCore::video_thread, this, arg);
    else if (AVMEDIA_TYPE_AUDIO == codec_type)
        decoder_thread_ = new std::thread(&Oran7FFmpegDecoderCore::audio_thread, this, arg);
    else
        return -1;
    return 0;
}

/**
 * @brief Oran7FFmpegDecoderCore::decoder_abort
 * @param fq
 */
void Oran7FFmpegDecoderCore::decoder_abort(FrameQueue *fq)
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

void Oran7FFmpegDecoderCore::decoder_destroy()
{
    if(avctx_)avcodec_free_context(&avctx_);
}


/**
 * @brief Oran7FFmpegDecoderCore::decode_packet_to_frame     使用AVCodec解码包packet获取帧frame,以及根据视频序列号serial丢包策略
 *             此函数属于深层私有函数，在本程序中由audio_thread/video_thread中调用
 * @param frame
 * @return -1: 请求退出
 *                0: 解码已经结束了，不再有数据可以读取
 *                1: 获取到解码后的frame
 */
int Oran7FFmpegDecoderCore::decode_packet_to_frame(AVFrame *frame)
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
                            WARNING_LOG << "[Decoder] 解码帧缺少 PTS";
                        }
                        if (decoder_reorder_pts == -1)
                            frame->pts = frame->best_effort_timestamp;
                        else if (decoder_reorder_pts)
                            frame->pts = frame->pkt_dts;

                        if(frame->pts == AV_NOPTS_VALUE)
                        {
                            WARNING_LOG << "[Decoder] 重排后帧仍缺少 PTS";
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
            next_pts = start_pts;               // audio
            next_pts_tb = start_pts_tb;     // audio
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
 * @brief Oran7FFmpegDecoderCore::get_video_frame
 * @param frame
 * @return
 */
int Oran7FFmpegDecoderCore::get_video_frame(AVFrame *frame)
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
            if(frame->format==s_hw_pix_fmt)
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
                    av_log(NULL, AV_LOG_INFO, "[AV_LOG_INFO:]-->Oran7FFmpegDecoderCore::get_video_frame:decoded frame format=%s\n",
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
 * @brief Oran7FFmpegDecoderCore::queue_picture    此函数用于将src_frame解码后的数据帧写入数据帧队列中
 * @param fq
 * @param src_frame
 * @param pts
 * @param duration
 * @param pos
 * @param serial
 * @return
 */
int Oran7FFmpegDecoderCore::queue_picture(FrameQueue *frameq, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
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
 * @brief Oran7FFmpegDecoderCore::audio_thread
 * @param arg
 * @return
 */
int Oran7FFmpegDecoderCore::audio_thread(void *arg)
{
    INFO_LOG<< "Audio decode thread has into";
    Oran7FFplayerCore *is = (Oran7FFplayerCore *)arg;
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
 * @brief Oran7FFmpegDecoderCore::video_thread
 * @param arg
 * @return
 */
int Oran7FFmpegDecoderCore::video_thread(void *arg)
{
    INFO_LOG<< "Video decode thread has into";
    Oran7FFplayerCore *is = (Oran7FFplayerCore *)arg;
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
