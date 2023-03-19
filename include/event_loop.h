#pragma once
#include "header.h"
#include "event_base.h"

#define MAXEVENTS 10000

// map: fd ---->  io_event
typedef std::unordered_map<int, io_event> io_event_map;
typedef std::unordered_map<int, io_event>::iterator io_event_map_it;

// set : value fd 全部正在监听的fd集合
typedef std::unordered_set<int> listen_fd_set;

class event_loop {
 public:
    // 构造函数
    event_loop();

    // 阻塞 循环监听事件，并且处理(epoll_wait()，并且直接进行相应处理)
    void event_process();

    // 添加一个io事件到event_loop中
    // 内部会调用io_event
    void add_io_event(int fd, io_callback *proc, int mask, void *args);

    // 从event_loop中删除一个io事件
    void del_io_event(int fd);

    // 删除一个io事件的触发条件（如EPOLLIN/EPOLLOUT变为只读）
    void del_io_event(int fd, int mask);

 private:
    // 通过epoll_create来创建的(对应的epoll 树)
    int _epfd;

    // 当前event_loop 监控的fd和对应事件的关系
    io_event_map _io_evs;

    // 当前event_loop 都有哪些fd正在监听  epoll_wait()在等待哪些fd触发状态（也就是未关闭）
    // 作用是将当前的服务器可以主动发送消息给客户端
    listen_fd_set listen_fds;
   
    // 每次epoll_wait，返回被激活的事件集合
    struct epoll_event _fired_evs[MAXEVENTS];
};