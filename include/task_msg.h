#pragma once
#include "message.h"

/**
 * thread_queue消息队列 所能接收的消息类型
 * */ 
struct task_msg
{
    // 两种task 类型
    // 1、新建连接任务
    // 2、一般普通任务（如主线程希望分发一些任务，非读写任务）
    enum TASK_TYPE {
        // 新建连接的任务
        NEW_CONN,
        // 一般任务
        NEW_TASK
    };
    // 任务类型
    TASK_TYPE t_type;

    // 任务本身数据内容
    union {
        // 如果是任务1，task_msg的任务内容就是一个 新建的connfd
        int connfd;
        // 任务2，task_msg的内容应该由具体的     参数 和 回调函数决定
        struct {
            void (*task_cb) (event_loop *loop, void *args);
            void *args;
        };

    };

};
