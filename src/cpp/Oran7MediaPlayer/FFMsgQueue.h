#ifndef FFMSGQUEUE_H
#define FFMSGQUEUE_H

#include "SDL2/SDL.h"

/**
 *   FFMsgQueue单个消息结构,链表单例
 */
typedef struct AVMessage
{
    int value;//消息类型
    int arg1;                                //参数1
    int arg2;                                //参数2
    void *obj;                              //额外参数
    void (*free_obj)(void *obj);//释放消息obj
    struct AVMessage *next;    //指向消息队列中的下一个消息指令
}AVMessage;

/**
 *  消息队列，链表队列
 */
typedef struct MessageQueue
{
    AVMessage *first_msg, *last_msg;//消息队列头节点和尾节点
    int nb_messages;//当前消息总数
    int abort_request;//请求中止消息队列标志
    SDL_mutex *mutex;
    SDL_cond *cond;
    AVMessage *recycle_msg_queue_first_node;//循环使用消息
    int recyle_count;//循环使用的次数，利用局部性原理
    int alloc_count;  //分配的次数
}MessageQueue;


///消息队列操作相关公共接口///

//释放msg中obj指针内存资源
void msg_free_obj_res(AVMessage *msg);
//插入消息私有接口
int msg_queue_put_private(MessageQueue *q,AVMessage *msg);
//插入消息公有接口
int msg_queue_put(MessageQueue *q,AVMessage *msg);
//初始化消息
void msg_init_msg(AVMessage *msg);
//插入简单消息，只带消息类型，不带参数
void msg_queue_put_simple(MessageQueue *q,int value);
//插入简单消息，带消息类型，带1个参数
void msg_queue_put_simple(MessageQueue *q,int value,int arg1);
//插入简单消息，带消息类型，带2个参数
void msg_queue_put_simple(MessageQueue *q,int value,int arg1,int arg2);
//释放obj资源
void msg_obj_free(void *obj);
//插入消息，带消息类型，带两个int参数，带obj参数
void msg_queue_put_simple(MessageQueue *q, int value, int arg1, int arg2,void *obj,int obj_len);
//消息队列初始化
void msg_queue_init(MessageQueue *q);
//消息队列flush，清空所有消息
void msg_queue_flush(MessageQueue *q);
//消息队列销毁
void msg_queue_destory(MessageQueue *q);
//消息队列中止
void msg_queue_abort(MessageQueue *q);
//启用消息队列
void msg_queue_start(MessageQueue *q);
//读取消息
/*return < 0 if aborted,0 if no msg and >0 if have reading msg*/
int msg_queue_get(MessageQueue *q,AVMessage *msg,int block);
//消息删除 把队列里同一消息类型的消息全部删除u
void msg_queue_remove(MessageQueue *q,int value);

#endif // FFMSGQUEUE_H
