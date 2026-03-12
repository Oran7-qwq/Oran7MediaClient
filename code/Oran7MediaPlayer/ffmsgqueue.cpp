#include "ffmsgqueue.h"
#include"FFMsg.h"
extern "C"
{
#include "libavutil/mem.h"
}

void msg_free_obj_res(AVMessage *msg)
{
    if(!msg||!msg->obj)return;
    msg->free_obj(msg->obj);
    msg->obj=nullptr;
}

int msg_queue_put_private(MessageQueue *q, AVMessage *msg)
{
    AVMessage *msg_temp;

    if(q->abort_request==1)return -1;

    /*是否利用消息内存空间循环机制*/
    msg_temp=q->recycle_msg_queue_first_node;
    //判断消息循环内存头节点是否有内存（不为空
    if(msg_temp)
    {
        //有内存，则利用，更新头节点
        q->recycle_msg_queue_first_node=msg_temp->next;
        q->recyle_count++;
    }
    else
    {
        //内存循环机制队列中，没有现有内存，直接分配
        msg_temp =(AVMessage*)av_malloc(sizeof(AVMessage));
        q->alloc_count++;
    }
    /*拷贝内存，浅拷贝，对于指针变量->只拷贝内部指针地址*/
    *msg_temp=*msg;
    msg_temp->next=nullptr;

    /* 将拷贝的消息插入消息队列队尾 */
    if(!q->first_msg)
        q->first_msg=msg_temp;
    else
        q->last_msg->next=msg_temp;
    q->last_msg=msg_temp;
    q->nb_messages++;

    SDL_CondSignal(q->cond);//通知唤醒线程等待，有新信息插入...
    return 0;
}

int msg_queue_put(MessageQueue *q, AVMessage *msg)
{
    SDL_LockMutex(q->mutex);
    int ret = msg_queue_put_private(q,msg);
    SDL_UnlockMutex(q->mutex);
    return ret;
}

void msg_init_msg(AVMessage *msg)
{
    memset(msg,0,sizeof(AVMessage));
}

void msg_queue_put_simple(MessageQueue *q, int value)
{
    AVMessage msg;
    msg_init_msg(&msg);
    msg.value=value;
    msg_queue_put(q,&msg);
}

void msg_queue_put_simple(MessageQueue *q, int value, int arg1)
{
    AVMessage msg;
    msg_init_msg(&msg);
    msg.value=value;
    msg.arg1=arg1;
    msg_queue_put(q,&msg);
}

void msg_queue_put_simple(MessageQueue *q, int value, int arg1, int arg2)
{
    AVMessage msg;
    msg_init_msg(&msg);
    msg.value=value;
    msg.arg1=arg1;
    msg.arg2=arg2;
    msg_queue_put(q,&msg);
}

void msg_obj_free(void *obj)
{
    av_freep(obj);
}

void msg_queue_put_simple(MessageQueue *q, int value, int arg1, int arg2, void *obj, int obj_len)
{
    AVMessage msg;
    msg_init_msg(&msg);
    msg.value = value;
    msg.arg1=arg1;
    msg.arg2=arg2;
    msg.obj=av_malloc(obj_len);
    memcpy(msg.obj,obj,obj_len);
    msg.free_obj=msg_obj_free;
    msg_queue_put(q,&msg);
}

void msg_queue_init(MessageQueue *q)
{
    memset(q,0,sizeof(MessageQueue));
    q->mutex=SDL_CreateMutex();
    q->cond=SDL_CreateCond();
    q->abort_request=1;
    q->first_msg=nullptr;
    q->last_msg=nullptr;
    q->nb_messages=0;
    q->recycle_msg_queue_first_node=nullptr;
    q->recyle_count=0;
    q->alloc_count=0;
}

void msg_queue_flush(MessageQueue *q)
{
    AVMessage *msg_tmp,*msg_tmp_next;

    SDL_LockMutex(q->mutex);

    for(msg_tmp=q->first_msg;msg_tmp!=nullptr;msg_tmp=msg_tmp_next)
    {
        msg_tmp_next=msg_tmp->next;
        //回收内存到内存循环队列
        msg_free_obj_res(msg_tmp);
        msg_init_msg(msg_tmp);
        msg_tmp->next=q->recycle_msg_queue_first_node;
        q->recycle_msg_queue_first_node=msg_tmp;
    }
    q->first_msg=nullptr;
    q->last_msg=nullptr;
    q->nb_messages=0;

    SDL_UnlockMutex(q->mutex);
}

void msg_queue_destory(MessageQueue *q)
{
    /* 先将消息队列内存全部回收到内存循环队列 */
    msg_queue_flush(q);

    SDL_LockMutex(q->mutex);

    while(q->recycle_msg_queue_first_node)
    {
        AVMessage *msg_tmp=q->recycle_msg_queue_first_node;
        if(msg_tmp)
            q->recycle_msg_queue_first_node=msg_tmp->next;
        av_freep(&msg_tmp);
    }

    SDL_UnlockMutex(q->mutex);

    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

void msg_queue_abort(MessageQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request=1;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
}

void msg_queue_start(MessageQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request=0;
    AVMessage msg;
    msg_init_msg(&msg);
    msg.value=FFP_MSG_FLUSH;
    msg_queue_put_private(q,&msg);

    SDL_UnlockMutex(q->mutex);
}

void msg_queue_remove(MessageQueue *q, int value)
{
    SDL_LockMutex(q->mutex);

    AVMessage queue_emp_head;
    msg_init_msg(&queue_emp_head);
    queue_emp_head.next=q->first_msg;

    AVMessage *msg_tmp=queue_emp_head.next;
    AVMessage *pre_msg_tmp=&queue_emp_head;
    AVMessage *msg_tmp_next;

    while(msg_tmp)
    {
        msg_tmp_next=msg_tmp->next;

        if(msg_tmp->value==value)
        {
            //调整队列
            if(msg_tmp==queue_emp_head.next)
            {
                queue_emp_head.next=msg_tmp_next;
                msg_tmp->next=nullptr;
            }
            //回收内存
            msg_free_obj_res(msg_tmp);
            msg_init_msg(msg_tmp);
            q->nb_messages--;
            msg_tmp->next=q->recycle_msg_queue_first_node;
            q->recycle_msg_queue_first_node=msg_tmp;
        }
        else
        {
            pre_msg_tmp=msg_tmp;
        }
        msg_tmp=msg_tmp_next;
    }
    //更新消息队列队头队尾
    q->first_msg=queue_emp_head.next;
    if(pre_msg_tmp==&queue_emp_head)
        q->last_msg=nullptr;
    else
    {
        q->last_msg=pre_msg_tmp;
        q->last_msg->next=nullptr;
    }

    SDL_UnlockMutex(q->mutex);
}

//获取消息队列内部消息
int msg_queue_get(MessageQueue *q, AVMessage *msg, int block)
{
    AVMessage *msg1;
    int ret;

    SDL_LockMutex(q->mutex);

    while(1)
    {
        if(q->abort_request)
        {
            ret=-1;
            break;
        }
        //get message
        msg1 = q->first_msg;
        if(msg1)
        {
            q->first_msg=msg1->next;
            if(q->first_msg==NULL)
                q->last_msg=NULL;
            q->nb_messages--;
            //传出msg
            *msg=*msg1;
            msg1->obj=NULL;
            //回收内存
            msg1->next=q->recycle_msg_queue_first_node;
            q->recycle_msg_queue_first_node=msg1;
            ret=1;
            break;
        }
        else if(!block)
        {
            ret = 0;
            break;
        }
        else
        {
            //等待唤醒，重新获取消息
            SDL_CondWait(q->cond,q->mutex);
        }
    }

    SDL_UnlockMutex(q->mutex);
    return ret;
}

