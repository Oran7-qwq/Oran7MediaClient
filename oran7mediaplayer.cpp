#include "oran7mediaplayer.h"

#include "ffmsg.h"
#include "ffmsgqueue.h"
#include "globalhelper.h"

#include<iostream>
#include<string.h>

/*Oran7MediaPlayer   Native层播放器总管理类*/

Oran7MediaPlayer::Oran7MediaPlayer()
{
}

Oran7MediaPlayer::~Oran7MediaPlayer()
{
    oran7mp_destroy();//统一销毁的接口
}

/**
 * @brief Oran7MediaPlayer::oran7mp_create  ​回调函数注册接口​​
 * @param msg_loop
 * @return
 */
int Oran7MediaPlayer::oran7mp_create(std::function<int (void *)> msg_loop)
{
    //创建内层播放器FFplayer对象实例
    if(ffplayer_)
        INFO_LOG<<"Oran7MediaPlayer::oran7mp_create:ffplayer still exist.\n";
    else
        INFO_LOG<<"Oran7MediaPlayer::oran7mp_create:ffplayer not exist,prepare to new one.\n";
    ffplayer_ = new FFPlayer();
    if(!ffplayer_)
    {
        WARNING_LOG << " new FFPlayer() failed\n ";
        return -1;
    }
    //绑定消息循环回调函数
    msg_loop_=msg_loop;

    //创建ffplayer_(初始化FFPlayer内部AVMessageQueue消息队列msg_queue_)
    int ret =ffplayer_->ffp_create();
    if(ret < 0)
    {
        return -1;
    }
    return 0;
}

/**
 * @brief Oran7MediaPlayer::oran7mp_destroy  Native层管理FFPlayer销毁
 * @return
 */
int Oran7MediaPlayer::oran7mp_destroy()
{
    bool expected = false;
    if (!destroying_.compare_exchange_strong(expected, true)) {
        return 0;
    }

    //先让消息线程能退出：abort msg_queue_
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (ffplayer_)
        {
            ffplayer_->ffp_stop_l();              // 设置 abort_request
            msg_queue_abort(&ffplayer_->msg_queue_); // 让 get_msg(block=1) 立刻返回
        }
    }

    //join 消息线程
    if (msg_thread_)
    {
        if (msg_thread_->joinable() &&
            msg_thread_->get_id() != std::this_thread::get_id())
        {
            msg_thread_->join();
        }
        delete msg_thread_;
        msg_thread_ = nullptr;
    }

    //销毁 ffplayer_
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (ffplayer_)
        {
            ffplayer_->ffp_destroy();
            delete ffplayer_;
            ffplayer_ = nullptr;
        }
    }

    //free data_source_
    if (data_source_) {
        free(data_source_);
        data_source_ = nullptr;
    }

    return 0;
}

/**
 * @brief Oran7MediaPlayer::oran7mp_set_data_source
 *             // 这个方法的设计来源于Android ijkmediaplayer, 其本意是
 *             //int IjkMediaPlayer::ijkmp_set_data_source(Uri uri)
 * @param url
 * @return
 */
int Oran7MediaPlayer::oran7mp_set_data_source(const char *url)
{
    if(!url)
        return -1;

    data_source_ = strdup(url); // strdup:分配内存 + 拷贝字符串
    return 0;
}

/**
 * @brief Oran7MediaPlayer::oran7mp_prepare_async (Asynchronous Programming)
 *              启用消息队列，创建消息循环机制线程，启动ffp_prepare_async_l调用ffplay的启动总入口stream_open
 * @return
 */
int Oran7MediaPlayer::oran7mp_prepare_async()
{
    // 判断mediaplayer的状态

    // 更新Oran7MediaPlayer状态显示 正在准备中
    mp_state_ = MP_STATE_ASYNC_PREPARING;
    //启用消息队列
    msg_queue_start(&ffplayer_->msg_queue_);
    //创建消息循环线程并启用
    //第一个this：绑定成员函数到当前对象（Oran7MediaPlayer实例）
    //​​第二个 this​​：作为参数传递给 oran7mp_msg_loop
    msg_thread_ = new std::thread(&Oran7MediaPlayer::oran7mp_msg_loop,this,this);
    //通过ffplayer_->ffp_prepare_async_l开始调用ffplay的启动总入口stream_open
    int ret =ffplayer_->ffp_prepare_async_l(data_source_);
    if(ret<0)
    {
        //若启动失败，更新Oran7MediaPlayer状态显示 MP_STATE_ERROR
        mp_state_  = MP_STATE_ERROR;
        return -1;
    }
    return 0;
}

/**
 * @brief Oran7MediaPlayer::oran7mp_start  触发开始播放
 * @return
 */
int Oran7MediaPlayer::oran7mp_start()
{
    //插入消息队列 开始播放请求
    ffp_remove_msg(ffplayer_,FFP_REQ_START);
    ffp_remove_msg(ffplayer_,FFP_REQ_PAUSE);
    ffp_notify_msg(ffplayer_,FFP_REQ_START);
    return 0;
}

/**
 * @brief Oran7MediaPlayer::oran7mp_stop  Native层管理FFPlayer停止
 * @return
 */
int Oran7MediaPlayer::oran7mp_stop()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (!ffplayer_) return 0;
    return ffplayer_->ffp_stop_l();
}

int Oran7MediaPlayer::oran7mp_pause()
{
    ffp_remove_msg(ffplayer_,FFP_REQ_START);
    ffp_remove_msg(ffplayer_,FFP_REQ_PAUSE);
    ffp_notify_msg(ffplayer_,FFP_REQ_PAUSE);
    return 0;
}

int Oran7MediaPlayer::oran7mp_seek_to(long msec)
{
    seek_req = 1;
    seek_msec = msec;
    ffp_remove_msg(ffplayer_, FFP_REQ_SEEK);
    ffp_notify_msg(ffplayer_, FFP_REQ_SEEK, (int)msec);
    return 0;
}

int Oran7MediaPlayer::oran7mp_get_state()
{
    return mp_state_;
}

int Oran7MediaPlayer::oran7mp_change_state_l(int new_state)
{
    mp_state_ = new_state;
    ffp_notify_msg(ffplayer_, FFP_MSG_PLAYBACK_STATE_CHANGED);
    return 0;
}

bool Oran7MediaPlayer::oran7mp_is_playing()
{
    return this->isPlaying;
}

long Oran7MediaPlayer::oran7mp_get_current_position()
{
    return ffplayer_->ffp_get_current_position_l();
}

long Oran7MediaPlayer::oran7mp_get_duration()
{
    return ffplayer_->ffp_get_duration_l();
}

/**
 * @brief Oran7MediaPlayer::oran7mp_get_msg     读取消息
 * @param msg
 * @param block
 * @return
 */
int Oran7MediaPlayer::oran7mp_get_msg(AVMessage *msg, int block)
{
    while(1)
    {
        int continue_wait_next_msg = 0; //是否继续读取下一消息标志

        int retval=msg_queue_get(&ffplayer_->msg_queue_,msg,block);//获取一条队头消息，没有则根据情况阻塞
        if(retval < 0)
            return retval;

        switch(msg->value)
        {
        case FFP_MSG_PREPARED:
            INFO_LOG<< "Oran7MediaPlayer Get message of FFP_MSG_PREPARED";
            break;
        case FFP_REQ_START:
            INFO_LOG<< "Oran7MediaPlayer Get message of FFP_REQ_START";
            continue_wait_next_msg = 1;
            retval=ffplayer_->ffp_start_l();
            if (retval == 0){
                oran7mp_change_state_l(MP_STATE_STARTED);
                this->isPlaying = true;
            }
            break;
        case FFP_REQ_PAUSE :
            INFO_LOG<< "Oran7MediaPlayer Get message of FFP_REQ_PAUSE";
            continue_wait_next_msg = 1;
            retval = ffplayer_->ffp_pause_l();
            if (retval == 0){
                this->isPlaying = false;
                oran7mp_change_state_l(MP_STATE_PAUSED);
            }
            break;
        case FFP_REQ_SEEK:
            INFO_LOG<< "Oran7MediaPlayer Get message of FFP_REQ_SEEK";
            continue_wait_next_msg = 1;
            ffplayer_->ffp_seek_to_l(msg->arg1);
            break;
        case FFP_MSG_SEEK_COMPLETE:
            INFO_LOG<< "Oran7MediaPlayer Get message of FFP_MSG_SEEK_COMPLETE";
            seek_req = 0;
            seek_msec = 0;
            break;
        default:
            INFO_LOG<< "Oran7MediaPlayer Get message of Unkonwn message of default " << msg->value;
            break;
        }
        if (continue_wait_next_msg)
        {
            msg_free_obj_res(msg);
            continue;
        }
        return retval;
    }
    return -1;//(void)
}

void Oran7MediaPlayer::oran7mp_set_playback_volume(int value)
{
    this->percent_volume = value;
    if(ffplayer_==nullptr)return;
    ffplayer_->ffp_update_reqVolum(value);
}

int Oran7MediaPlayer::oran7mp_get_percent_volume() const
{
    return this->percent_volume;
}

int Oran7MediaPlayer::oran7mp_msg_loop(void *arg)
{
    msg_loop_(arg);
    return 0;
}

int Oran7MediaPlayer::oran7mp_set_playback_rate(double speed)
{
    ffplayer_->ffp_set_playback_rate(speed);
    return 0;
}

void Oran7MediaPlayer::AddVideoRefreshCallback(std::function<int (const Frame *,AVFrame *)> callback)
{
    ffplayer_->AddVideoRefreshCallback(callback);
}

void Oran7MediaPlayer::oran7mp_setD3D11Device(ID3D11Device *dev)
{
    ffplayer_->setD3D11Device(dev);
}
