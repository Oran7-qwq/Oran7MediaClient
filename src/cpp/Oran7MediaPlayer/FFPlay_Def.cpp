#include "FFPlay_Def.h"
#include "stdio.h"

/**
 * @brief flush_pkt  be extern   刷新标志数据包(空包仅作处理标志)
 */
AVPacket flush_pkt;

/**
 *  @brief saved default volum be extern
 */
int DEFAULT_VOLUM_SIZE = SDL_MIX_MAXVOLUME;

/**
 * @brief 支持的video硬件解码库,解码器名称
 */
const char *Hardware_Support_Library[]={
    "d3d11va",
    "dxva2",
    nullptr
};

/**
 * @brief packet_queue_put_private   往数据包队列压入数据包，私有接口
 * @param q
 * @param pkt
 * @return
 */
static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
    if (q->abort_request)
        return -1;

    MyAVPacketList *pkt1 = (MyAVPacketList*)av_mallocz(sizeof(MyAVPacketList));
    if (!pkt1)
        return -1;

    if (pkt == &flush_pkt) {////如果放入的是flush_pkt，需要增加队列的播放序列号，以区分不连续的两段数据
        // flush_pkt 是特殊包：不做ref
        pkt1->pkt = *pkt;
        q->serial++;
        av_log(NULL, AV_LOG_INFO, "[Update Serial:]%s serial=%d\n",
               av_get_media_type_string(q->type), q->serial);
    } else {
        //拷贝AVPacket(浅拷贝，AVPacket.data等内存并没有拷贝)
        //pkt1->pkt = *pkt;//<---Discard 2026/2/3
        //--->
        /** 用 av_packet_ref，而不是结构体浅拷贝
        底层 AVBufferRef 引用计数 +1，队列节点安全拥有数据*/
        //--->
        if (av_packet_ref(&pkt1->pkt, pkt) < 0) {
            av_free(pkt1);
            return -1;
        }
    }

    pkt1->serial = q->serial;//用队列序列号标记节点
    pkt1->next = NULL;
    //--->
    /** 队列操作：如果last_pkt为空，说明队列是空的，新增节点为队头；
     * 否则，队列有数据，则让原队尾的next为新增节点。 最后将队尾指向新增节点
     */
    //--->
    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;

    /*队列属性操作：增加节点数、cache大小、cache总时长, 用来控制队列的大小*/
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    q->duration += pkt1->pkt.duration;

    /* XXX: should duplicate packet data in DV case */
    //发出信号，表明当前队列中有数据了，通知等待中的读线程可以取数据了
    SDL_CondSignal(q->cond);
    return 0;
}


/**
 * @brief packet_queue_put      往数据包队列压入数据包，公有接口，在此处加入互斥锁进行线程同步
 * @param q
 * @param pkt
 * @return
 */
int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    SDL_LockMutex(q->mutex);
    int ret = packet_queue_put_private(q, pkt);//主要实现
    SDL_UnlockMutex(q->mutex);

    //只要不是 flush_pkt，就应该 unref 释放“读线程手里的那份”:private里已经 ref 了一份
    if (pkt->data!= flush_pkt.data)
        av_packet_unref(pkt);       //放入失败，释放AVPacket

    return ret;
}

/**
 * @brief packet_queue_put_nullpacket       往对应stream_index数据包队列中发送空包
 * @param q
 * @param stream_index
 * @return
 */
int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket *pkt = av_packet_alloc();
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

/**
 * @brief packet_queue_init     packet queue handling   数据包队列初始化
 * @param q
 * @return
 */
int packet_queue_init(PacketQueue *q,AVMediaType type)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->cond = SDL_CreateCond();
    if (!q->cond) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->abort_request = 1;

    q->type=type;
    return 0;
}

/**
 * @brief packet_queue_flush    清空数据包队列，一般在seek后，播放序列号发生改变，需要刷新数据包队列，舍弃旧播放序列数据包
 * @param q
 */
void packet_queue_flush(PacketQueue *q)
{
    MyAVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);

    for (pkt = q->first_pkt; pkt; pkt = pkt1)
    {
        pkt1 = pkt->next;
        av_packet_unref(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;

    SDL_UnlockMutex(q->mutex);
}


/**
 * @brief packet_queue_destroy      数据包队列的销毁
 * @param q
 */
void packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q);              //先清除所有的节点
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

/**
 * @brief packet_queue_abort    数据包队列退出请求
 * @param q
 */
void packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;       // 请求退出

    SDL_CondSignal(q->cond);    //释放一个条件信号

    SDL_UnlockMutex(q->mutex);
}

/**
 * @brief packet_queue_start        数据包队列启动
 * @param q
 */
void packet_queue_start(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 0;
    packet_queue_put_private(q, &flush_pkt); //初次启动时首先插入flush_pkt，播放起点

    SDL_UnlockMutex(q->mutex);
}

/**
 * @brief packet_queue_get
 * @param q 队列
 * @param pkt 输出参数，即MyAVPacketList.pkt
 * @param block 调用者是否需要在没节点可取的情况下阻塞等待
 * @param serial 输出序列参数，即MyAVPacketList.serial
 * @return <0: aborted; =0: no packet; >0: has packet
 *               return < 0 if aborted, return = 0 if no packet ,return > 0 if has packet.
 */
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
    MyAVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);    // 加锁

    for (;;)
    {
        if (q->abort_request)
        {
            ret = -1;
            break;
        }
        pkt1 = q->first_pkt;    //MyAVPacketList *pkt1; 从队头拿数据
        if (pkt1)
        {   //<---队列中有数据
            q->first_pkt = pkt1->next;  //队头移到第二个节点
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;                                         //节点数减1
            q->size -= pkt1->pkt.size + sizeof(*pkt1); //cache大小扣除一个节点
            q->duration -= pkt1->pkt.duration;           //总时长扣除一个节点

            /** 返回AVPacket，这里发生一次AVPacket结构体拷贝，AVPacket的data只拷贝了指针*/
            *pkt = pkt1->pkt;//<---Discard2026/2/3
            // <---New2026/2/3 : 把节点里的 packet 引用“搬走”给输出 pkt
            // av_packet_move_ref(pkt, &pkt1->pkt);

            //输出packet包队列最新serial
            //printf("Get first new packet serial:--->%d\n", pkt1->serial);
            if (serial) *serial = pkt1->serial;

            av_free(pkt1);      //释放节点内存,只是释放节点结构体变量，并非释放AVPacket.data，pkt1中的AVPacket.data指针域内存已经被转移引用到pkt输出
            ret = 1;
            break;
        }
        else if (!block)
        {    //队列中没有数据，且非阻塞调用
            ret = 0;
            break;
        }
        else
        {   //队列中没有数据，且阻塞调用
            //这里没有break。for循环的另一个作用是在条件变量满足后重复上述代码取出节点
            SDL_CondWait(q->cond, q->mutex);//休眠等待数据读取线程通信: 已解复用文件并插入新的数据包到队列中，可以继续取数据
        }
    }
    SDL_UnlockMutex(q->mutex);  // 释放锁
    return ret;
}

/**
 * @brief frame_queue_unref_item    释放一帧数据内存av_frame_unref
 * @param vp
 */
static void frame_queue_unref_item(Frame *vp)
{
    av_frame_unref(vp->frame);	/* 释放数据 */
}

/**
 * @brief frame_queue_init      初始化FrameQueue，视频和音频keep_last设置为1，字幕设置为0
 * @param f
 * @param pktq
 * @param max_size
 * @return
 */
int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size,int keep_last)
{
    int i;
    memset(f, 0, sizeof(FrameQueue));
    if (!(f->mutex = SDL_CreateMutex()))
    {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    if (!(f->cond = SDL_CreateCond()))
    {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    f->keep_last = !!keep_last;
    f->pktq = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);

    for (i = 0; i < f->max_size; i++)
        if (!(f->queue[i].frame = av_frame_alloc())) // 分配AVFrame结构体
            return AVERROR(ENOMEM);
    return 0;
}

/**
 * @brief frame_queue_destory       销毁数据帧队列
 * @param f
 */
void frame_queue_destory(FrameQueue *f)
{
    int i;
    for (i = 0; i < f->max_size; i++)
    {
        Frame *vp = &f->queue[i];
        // 释放对vp->frame中的数据缓冲区的引用，注意不是释放frame对象本身
        frame_queue_unref_item(vp);
        // 释放vp->frame对象
        av_frame_free(&vp->frame);
    }
    SDL_DestroyMutex(f->mutex);
    SDL_DestroyCond(f->cond);
}

/**
 * @brief frame_queue_signal        主调用SDL_CondSignal(f->cond);
 * @param f
 */
void frame_queue_signal(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

/**
 * @brief frame_queue_peek      获取队列当前队尾Frame指针, 在调用该函数前先调用frame_queue_nb_remaining确保有frame可读
 * @param f
 * @return
 */
Frame *frame_queue_peek(FrameQueue *f)
{
    return &f->queue[(f->rindex+ f->rindex_shown) % f->max_size];
}

/**
 * @brief frame_queue_peek_next 获取当前Frame的下一Frame, 此时要确保queue里面至少有2个Frame
 * @param f
 * @return  never will be NULL
 */
Frame *frame_queue_peek_next(FrameQueue *f)
{
    return &f->queue[(f->rindex  + f->rindex_shown+1) % f->max_size];
}

/**
 * @brief frame_queue_peek_last
 * @param f
 * @return
 */
Frame *frame_queue_peek_last(FrameQueue *f)
{
    return &f->queue[f->rindex];
}

/**
 * @brief frame_queue_peek_writable 获取数据帧队列中可写位置 指针
 * @param f
 * @return
 */
Frame *frame_queue_peek_writable(FrameQueue *f)
{
    /* wait until we have space to put a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size && !f->pktq->abort_request)/* 检查是否需要退出 */
    {
        SDL_CondWait(f->cond, f->mutex);//当前数据帧队列已满，等待数据帧被读出后，通信唤醒继续输出可写位置指针
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)			 /* 检查是不是要退出 */
        return NULL;

    return &f->queue[f->windex];
}

/**
 * @brief frame_queue_peek_readable  获取可读数据帧位置 指针
 * @param f
 * @return
 */
Frame *frame_queue_peek_readable(FrameQueue *f)
{
    /* wait until we have a readable a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size<=f->rindex_shown && !f->pktq->abort_request)
    {
        SDL_CondWait(f->cond, f->mutex);//当前数据帧队列已空，等待数据帧写入后，通信唤醒继续获取可读位置指针
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)
        return NULL;

    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

/**
 * @brief frame_queue_push  更新写指针位置到下一个位置
 * @param f
 */
void frame_queue_push(FrameQueue *f)
{
    if (++f->windex == f->max_size)
        f->windex = 0;
    SDL_LockMutex(f->mutex);
    f->size++;
    SDL_CondSignal(f->cond);    // 当frame_queue_peek_readable在等待时则可以唤醒
    SDL_UnlockMutex(f->mutex);
}

/**
 * @brief frame_queue_next  释放当前frame，并更新读索引rindex到下一个位置
 * @param f
 */
void frame_queue_next(FrameQueue *f)
{
    if (f->keep_last && !f->rindex_shown)
    {
        f->rindex_shown = 1;
        return;
    }
    frame_queue_unref_item(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size)
        f->rindex = 0;
    SDL_LockMutex(f->mutex);
    f->size--;
    SDL_CondSignal(f->cond);// 当frame_queue_peek_writable在等待时可以唤醒
    SDL_UnlockMutex(f->mutex);
}

/**
 * @brief frame_queue_nb_remaining
 * @param f
 * @return  return the number of undisplayed frames in the queue
 */
int frame_queue_nb_remaining(FrameQueue *f)
{
    return f->size - f->rindex_shown;
}

/**
 * @brief frame_queue_last_pos
 * @param f
 * @return  return last shown position
 */
int64_t frame_queue_last_pos(FrameQueue *f)
{
    Frame *fp = &f->queue[f->rindex];
    if (f->rindex_shown && fp->serial == f->pktq->serial)
    {
        return fp->pos;
    }
    return -1;
}

/**
 * 获取到的实际上是:最后一帧的pts 加上 从处理最后一帧开始到现在的时间,具体参考set_clock_at 和get_clock的代码
 * c->pts_drift=最后一帧的pts-从处理最后一帧时间
 * clock=c->pts_drift+现在的时候
 * get_clock(&is->vidclk) ==is->vidclk.pts, av_gettime_relative() / 1000000.0 -is->vidclk.last_updated  +is->vidclk.pts
 */
double get_clock(Clock *c)
{
    //printf("*c->queue_serial:%d  c->serial:%d\n",*c->queue_serial,c->serial);
    if (*c->queue_serial != c->serial)
    {
        //printf("*c->queue_serial:%d  c->serial:%d\n",*c->queue_serial,c->serial);
        return NAN; // 不是同一个播放序列，时钟是无效
    }
    if (c->paused)
        return c->pts;  // 暂停的时候返回的是pts
    else
    {
        double time = av_gettime_relative() / 1000000.0;
        return c->drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

void set_clock_at(Clock *c, double pts,int serial, double time)
{
    c->pts		= pts;                      /* 当前帧的pts */
    c->last_updated = time;                 /* 最后更新的时间，实际上是当前的一个系统时间 */
    c->drift	= c->pts - time;        /* 当前帧pts和系统时间的差值，正常播放情况下两者的差值应该是比较固定的，因为两者都是以时间为基准进行线性增长 */
    c->serial = serial;
}

void set_clock(Clock *c, double pts,int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts,serial, time);
}


void init_clock(Clock *c,int *queue_serial)
{
    memset(c, 0, sizeof(Clock));
    c->mutex=SDL_CreateMutex();
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    c->pts = NAN;
    c->last_update = 0;
    c->drift = 0;
    c->serial = 0;
}

void destory_clock(Clock *c)
{
    if(c->mutex!=NULL)
        SDL_DestroyMutex(c->mutex);
}
