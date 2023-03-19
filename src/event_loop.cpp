#include "event_loop.h"
#include "header.h"

// 构造函数
event_loop::event_loop() {
    // 创建一个epoll句柄 文件描述符
    _epfd = epoll_create1(0);
    if (_epfd == -1) {
        fprintf(stderr, "epoll initial failed\n");
        exit(1);
    }
}

// 阻塞 循环监听事件，并且处理(epoll_wait()，并且直接进行相应处理)
void event_loop::event_process() {
    io_event_map_it ev_it;
    while (true) {
#if DEBUG
        std::cout << "WAITING EVENT" << std::endl;
#endif
#if DEBUG
        for (auto it : listen_fds) {
            std::cout << "fd " << it << " listening" << std::endl;
        }
#endif

        int num_fds = epoll_wait(_epfd, _fired_evs, MAXEVENTS, -1);
        for (int i = 0; i < num_fds; i++) {
            ev_it = _io_evs.find(_fired_evs[i].data.fd);
            // _io_evs[_fired_evs[i].data.fd];
            assert(ev_it != _io_evs.end());

            // 取出对应的事件
            io_event *ev = &(ev_it->second);
            if (_fired_evs[i].events & EPOLLIN) {
                // 读事件
                void *args = ev->rcb_args;
                ev->read_callback(this, _fired_evs[i].data.fd, args);
            }
            else if (_fired_evs[i].events & EPOLLOUT) {
                // 写事件
                void *args = ev->wcb_args;
                ev->write_callback(this, _fired_evs[i].data.fd, args);
            }
            else if (_fired_evs[i].events & (EPOLLHUP|EPOLLERR)) {
                // 水平触发未处理， 可能会出现HUP事件
                // 如果当前事件 events 没有写也没有读， 将events从epoll中删除
                if (ev->read_callback != nullptr) {
                    void *args = ev->rcb_args;
                    ev->read_callback(this, _fired_evs[i].data.fd, args);                    
                }
                else if (ev->write_callback != nullptr) {
                    void *args = ev->wcb_args;
                    ev->write_callback(this, _fired_evs[i].data.fd, args);
                }
            }
            else {
                // 删除事件
                fprintf(stderr, "fd %d get error, delete from epoll", _fired_evs[i].data.fd);
                this->del_io_event(_fired_evs[i].data.fd);
            }
        }
    }
}

// 添加一个io事件到event_loop中
// 内部会调用io_event
void event_loop::add_io_event(int fd, io_callback *proc, int mask, void *args) {
    //
    int final_mask; 
    int op;
    // 1、找到当前的 fd 是否为已有事件
    if (_io_evs.find(fd) != _io_evs.end()) {
        // 存在，则修改的方式添加到epoll中
        op = EPOLL_CTL_MOD;
        final_mask = _io_evs[fd].mask | mask;
        
    }
    else {
        // 如果没有该事件， ADD方式添加到epoll中
        op = EPOLL_CTL_ADD;
        final_mask = mask;
    }

    // 2、将fd 和 io_callback绑定 添加到map中
    if (mask & EPOLLIN) {
        // 为读事件，注册回调函数
        // 该部分event_base中的mask 需要赋值 因为计算修改的mask时需要使用
        _io_evs[fd].mask = final_mask;
        _io_evs[fd].read_callback = proc;
        _io_evs[fd].rcb_args = args;
    }
    else if (mask & EPOLLOUT) {
        // 写事件
        _io_evs[fd].mask = final_mask;
        _io_evs[fd].write_callback = proc;
        _io_evs[fd].wcb_args = args;
    }

    // 3、将当前事件加入到 原生 epoll中(该部分才是真正的加入)
    struct epoll_event event;
    event.events = final_mask;
    event.data.fd = fd;
    if (epoll_ctl(_epfd, op, fd, &event) == -1) {
        fprintf(stderr, "epoll ctl %d error\n", fd);
        return;
    }

    // 4、将当前fd加入到 正在监听的 fd集合
    listen_fds.insert(fd);
}

// 从event_loop中删除一个io事件
void event_loop::del_io_event(int fd) {
    if (_io_evs.find(fd) == _io_evs.end()) {
        return;
    }
    // 如果一个事件由于没有注册被删除，那么下次要再使用怎么办，不应该直接关闭连接吗？
    // 将事件从io_evs map中删除 
    _io_evs.erase(fd);

    // 将fd 从 listen_fds set中删除
    listen_fds.erase(fd);

    // 从原生 epoll中删除(所属的 epfd， 删除标志， 目标fd)
    epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
    // close(fd);
}

// 删除一个io事件的触发条件（如EPOLLIN/EPOLLOUT变为只读）
void event_loop::del_io_event(int fd, int mask) {
    io_event_map_it it = _io_evs.find(fd);
    if (it == _io_evs.end()) {
        return;
    }

    // 存在，则开始删除操作 key: fd value: io_event
    // 0011  mask:0001 --> 0011 & 1110 = 0010
    it->second.mask = it->second.mask & (~mask);

    if (it->second.mask == 0) {
        // 通过删除后，没有可触发条件（mask全为0），此时删除
        std::cout << "mask zero delete" << std::endl;
        this->del_io_event(fd);
    }
    else {
        // 事件存在，进行poll原生修改
        struct epoll_event ev;
        ev.events = it->second.mask;
        ev.data.fd = fd;

        epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &ev);
    }
}