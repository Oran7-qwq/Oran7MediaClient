#ifndef ORAN7MEDIAPLAYER_H
#define ORAN7MEDIAPLAYER_H

#include  <mutex>
#include <thread>
#include <functional>
#include <atomic>      // destroying_
#include <string>

#include "FFPlay_Def.h"   // Frame / AVFrame（AddVideoRefreshCallback 形参）
#include "FFMsgQueue.h"   // AVMessage（oran7mp_get_msg 形参）

// 前向声明：Oran7FFplayerCore 仅作指针成员；ID3D11Device 仅作指针形参（完整类型在 .cpp 侧 include）
class Oran7FFplayerCore;
struct ID3D11Device;

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

    /*! @note 设置要播放的url */
    int oran7mp_set_data_source(const char *url);
    // 返回副本；调用者须与设置数据源/销毁串行，不暴露内部缓冲区。
    std::string dataSource() const { return data_source_ ? data_source_ : ""; }

    /*! @note 准备播放 */
    int oran7mp_prepare_async();

    /*! @note 触发播放 */
    int oran7mp_start();

    /*! @note 停止 */
    int oran7mp_stop();

    /*! @note 暂停 */
    int oran7mp_pause();

    /*! @note seek到指定位置 */
    int oran7mp_seek_to(long msec);

    /*! @note 获取播放状态 */
    int oran7mp_get_state();

    /*! @note 改变播放器状态 */
    int oran7mp_change_state_l(int new_state);

    /*! @note 是不是播放中 */
    bool oran7mp_is_playing();

    /*! @note 当前播放位置（整数秒） */
    long oran7mp_get_current_position();

    /*! @note 当前播放位置（浮点秒） */
    double oran7mp_get_current_position_double();

    /*! @note 总时长 */
    long oran7mp_get_duration();

    /*! @note 已经播放的长度 */
    long oran7mp_get_playable_duration();

    /*! @note 设置循环播放 */
    void oran7mp_set_loop(int loop);

    ///*! @note 获取是否循环播放 */
    int  oran7mp_get_loop();

    /*! @note 读取消息 */
    int oran7mp_get_msg(AVMessage *msg, int block);

   /*! @note 设置音量 */
    void oran7mp_set_playback_volume(int value);

    /*! @note 获取以%为单位的当前播放音量大小*/
    int oran7mp_get_percent_volume()const;

    /**
     * @brief 启动外部传入消息循环线程函数
     */
    int oran7mp_msg_loop(void *arg);

    /**
     * @brief 外层请求设置ffplayer_播放速度
     * @param speed 播放速度百分比 参数范围 3.0 - 1.0 - 0.0, default=1.0
     */
    int oran7mp_set_playback_rate(double speed);

    /**
     * @brief 添加视频帧回调函数，
     * @param callback
     * @note 回调函数参数列表设计思路:
     *             请参考---->void Oran7FFplayerCore::video_refresh(double *remaining_time)
     */
    void AddVideoRefreshCallback(std::function<int(const Frame *,AVFrame *)> callback);

    /**
     * @brief 拓展接口：传入Qt场景图的d3d11Device设备，
     *                              可以打通从解码器到渲染整套链路用同一个设备零拷贝
     *                              - 编码器使用 同一ID3D11Device
     *                              - 渲染端使用 同一ID3D11Device
     *                              实现4K60fps 解码->渲染 "零拷贝"
     * @param dev
     */
    void oran7mp_setD3D11Device(ID3D11Device* dev);

private:
    std::mutex mutex_;                    // 线程同步互斥量
    Oran7FFplayerCore *ffplayer_ = NULL;       //内核播放器Core-ffplay模块
    std::function<int(void *)> msg_loop_ = NULL; //函数指针, 指向创建的message_loop，即内部消息循环函数 ->ui处理消息的循环
    std::thread *msg_thread_ = nullptr; // 消息循环机制线程,执行msg_loop
    char *data_source_ = nullptr;       // 播放资源文件路径或url链接
    std::atomic<int> mp_state_{MP_STATE_IDLE}; // 消息线程写、GUI 线程读

    /*! @note Player percent_volume*/
    int percent_volume = 25;

   /*! @note isPlaying*/
    std::atomic_bool isPlaying{false};

    /*! @note req_seeking*/
    int seek_req = 0;
    long seek_msec = 0;

    std::atomic<bool> destroying_{false};//防止重复destroy
};

#endif // ORAN7MEDIAPLAYER_H
