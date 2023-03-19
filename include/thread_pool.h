#pragma once
#include "thread_queue.h"
#define THREAD_NUM 6

class thread_pool
{
private:
    // 当前thread_queue集合
    thread_queue<task_msg> **_queues;
    // 线程的个数
    int _thread_cnt;
    // 线程ID
    pthread_t *_tids;
    // 获取线程的索引
    int _index;

public:
    thread_pool(int thread_cnt);
    ~thread_pool();
    // 提供一个获取 thread_queue 的方法
    thread_queue<task_msg> *get_thread();
};

