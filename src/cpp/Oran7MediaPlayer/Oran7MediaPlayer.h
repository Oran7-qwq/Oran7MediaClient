#ifndef ORAN7MEDIAPLAYER_H
#define ORAN7MEDIAPLAYER_H

#include  <mutex>
#include <thread>
#include <functional>

#include "FFPlay_Def.h"
#include "FFPlayer.h"
#include "FFMsgQueue.h"

/*-
 * oran7mp_set_data_source()  -> MP_STATE_INITIALIZED
 *
 * oran7mp_reset              -> self
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_IDLE               0

/*-
 * oran7mp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * oran7mp_reset              -> MP_STATE_IDLE
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_INITIALIZED        1

/*-
 *                   ...    -> MP_STATE_PREPARED
 *                   ...    -> MP_STATE_ERROR
 *
 * oran7mp_reset              -> MP_STATE_IDLE
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_ASYNC_PREPARING    2

/*-
 * oran7mp_seek_to()          -> self
 * oran7mp_start()            -> MP_STATE_STARTED
 *
 * oran7mp_reset              -> MP_STATE_IDLE
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_PREPARED           3

/*-
 * oran7mp_seek_to()          -> self
 * oran7mp_start()            -> self
 * oran7mp_pause()            -> MP_STATE_PAUSED
 * oran7mp_stop()             -> MP_STATE_STOPPED
 *                   ...    -> MP_STATE_COMPLETED
 *                   ...    -> MP_STATE_ERROR
 *
 * oran7mp_reset              -> MP_STATE_IDLE
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_STARTED            4

/*-
 * oran7mp_seek_to()          -> self
 * oran7mp_start()            -> MP_STATE_STARTED
 * oran7mp_pause()            -> self
 * oran7mp_stop()             -> MP_STATE_STOPPED
 *
 * oran7mp_reset              -> MP_STATE_IDLE
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_PAUSED             5

/*-
 * oran7mp_seek_to()          -> self
 * oran7mp_start()            -> MP_STATE_STARTED (from beginning)
 * oran7mp_pause()            -> self
 * oran7mp_stop()             -> MP_STATE_STOPPED
 *
 * oran7mp_reset              -> MP_STATE_IDLE
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_COMPLETED          6

/*-
 * oran7mp_stop()             -> self
 * oran7mp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * oran7mp_reset              -> MP_STATE_IDLE
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_STOPPED            7

/*-
 * oran7mp_reset              -> MP_STATE_IDLE
 * oran7mp_release            -> MP_STATE_END
 */
#define MP_STATE_ERROR              8

/*-
 * oran7mp_release            -> self
 */
#define MP_STATE_END                9

class Oran7MediaPlayer
{
public:
    Oran7MediaPlayer();
    ~Oran7MediaPlayer();
    int oran7mp_create(std::function<int(void *)> msg_loop);
    int oran7mp_destroy();

    // 设置要播放的url
    int oran7mp_set_data_source(const char *url);
    // 准备播放
    int oran7mp_prepare_async();
    // 触发播放
    int oran7mp_start();
    // 停止
    int oran7mp_stop();
    // 暂停
    int oran7mp_pause();
    // seek到指定位置
    int oran7mp_seek_to(long msec);
    // 获取播放状态
    int oran7mp_get_state();
    //改变播放器状态
    int oran7mp_change_state_l(int new_state);
    // 是不是播放中
    bool oran7mp_is_playing();
    // 当前播放位置
    long oran7mp_get_current_position();
    // 总长度
    long oran7mp_get_duration();
    // 已经播放的长度
    long oran7mp_get_playable_duration();
    // 设置循环播放
    void oran7mp_set_loop(int loop);
    // 获取是否循环播放
    int  oran7mp_get_loop();
    // 读取消息
    int oran7mp_get_msg(AVMessage *msg, int block);
    // 设置音量
    void oran7mp_set_playback_volume(int value);

    int oran7mp_get_percent_volume()const;

    int oran7mp_msg_loop(void *arg);//消息循环线程函数

    //外层请求设置ffplayer_播放速度
    int oran7mp_set_playback_rate(double speed);

    void AddVideoRefreshCallback(std::function<int(const Frame *,AVFrame *)> callback);

    void oran7mp_setD3D11Device(ID3D11Device* dev);

private:
    std::mutex mutex_;               // 线程同步互斥量
    FFPlayer *ffplayer_ = NULL; //内部播放器ffplay模块
    std::function<int(void *)> msg_loop_ = NULL; //函数指针, 指向创建的message_loop，即内部消息循环函数 ->ui处理消息的循环
    std::thread *msg_thread_; // 消息循环机制线程,执行msg_loop
    char *data_source_;           //播放资源文件路径或链接
    int mp_state_;                    //播放器状态，例如prepared,resumed,error,completed等

    /*Player percent_volume*/
    int percent_volume;

    /*isPlaying*/
    bool isPlaying=false;

    /*req_seeking*/
    int seek_req = 0;
    long seek_msec = 0;

    std::atomic<bool> destroying_{false};//防止重复destroy
};

#endif // ORAN7MEDIAPLAYER_H
