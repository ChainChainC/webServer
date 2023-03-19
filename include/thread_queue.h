#pragma once
#include "event_loop.h"
#include "header.h"
#include "task_msg.h"

template <typename T>
class thread_queue
{
private:
    // 让某个线程监听
    int _evfd;
    // 目前被呢个epoll监听
    event_loop *_loop;
    std::queue<T> _event_queue;
    // 队列锁
    pthread_mutex_t _queue_mutex;
    
public:
    thread_queue(/* args */) {
        _loop = nullptr;
        pthread_mutex_init(&_queue_mutex, nullptr);
        // 创建一个能够被 epoll监听的fd
        _evfd = eventfd(0, EFD_NONBLOCK);

        if (_evfd == -1) {
            perror("eventfd()");
            exit(1);
        }
    }

    ~thread_queue() {
        pthread_mutex_destroy(&_queue_mutex);
        close(_evfd);
    }

    // 向队列中添加任务 ()
    void send_event(const T &task) {
        pthread_mutex_lock(&_queue_mutex);
        _event_queue.push(task);
        pthread_mutex_unlock(&_queue_mutex);
        // 将激活的_evfd可读事件 ， 向_evfd写数据
        unsigned long long idle_num = 1;
        int ret = write(_evfd, &idle_num, sizeof(unsigned long long));
        if (ret == -1) {
            perror("evfd write error.");
        }
        
    }
    // 从队列中取数据  将整个queue返回给上层
    void recv_event(std::queue<T> &o_queue) {
        unsigned long long idle_num;
        pthread_mutex_lock(&_queue_mutex);
        // 将evfd所绑定 事件的 读写缓存清空，将占位激活符号 读出
        int ret = read(_evfd, &idle_num, sizeof(unsigned long long));
        if (ret == -1) {
            perror("evfd write error.");
        }
        // 交换对象指针，需要保证o_queue是空队列
        std::swap(o_queue, _event_queue);
        pthread_mutex_unlock(&_queue_mutex);
    }

    // 让当前的thread_queue被哪个event_loop监听
    void set_loop(event_loop *loop) {
        this->_loop = loop;
    }

    // 给当前对应_evfd对应的EPOLLIN事件设置回调函数
    void set_callback(io_callback *cb, void *args = nullptr) {
        if (_loop != nullptr) {
            _loop->add_io_event(_evfd, cb, EPOLLIN, args);
        }
    }

};
