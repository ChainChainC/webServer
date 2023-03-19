#pragma once

/**
 * 定义IO复用机制 时间的封装（epoll原生事件）
 * 
 * */ 
class event_loop;

// IO事件触发的回调函数
typedef void io_callback(event_loop *loop, int fd, void *args);

/**
 * 封装一次IO触发的事件
 **/
struct io_event {
    io_event() {
        mask = 0;
        read_callback = nullptr;
        write_callback = nullptr;
        rcb_args = nullptr;
        wcb_args = nullptr;
    }

    // 时间的读写属性
    // EPOLLIN, EPOLLOUT
    int mask;

    // 读事件触发绑定的回调函数 (函数指针，事件绑定)
    io_callback *read_callback;

    // 写事件触发所绑定的回调函数
    io_callback *write_callback;

    // 读事件回调函数形参
    void *rcb_args;

    // 写事件回调函数形参
    void *wcb_args;

};
 
