#pragma once
#include "reactor_buf.h"
#include "event_loop.h"
#include "tcp_conn.h"
#include "header.h"
#include "message.h"
#include "thread_pool.h"


void accept_callback(event_loop *loop, int fd, void *args);


class tcp_server {
private:
    // 套接子 listen fd
    int _sockfd;
    // 客户端连接地址
    struct sockaddr_in _connaddr;
    // 客户端连接地址长度
    socklen_t _addrlen;

    // epoll io多路复用
    event_loop *_loop;

public:
    tcp_server(event_loop *p_loop, const char *ip, uint16_t port);
    ~tcp_server();
    // accpet
    void do_accept();
    
    // v0.5版本增加
    // 全部在线连接
    static std::unordered_map<int, tcp_conn *> _conn_map;
    static tcp_conn **_conns;
    // 新增一个连接
    static void add_conn(int connfd, tcp_conn *conn);
    static void delete_conn(int connfd);
    // 获取当前在线连接个数
    static int get_conn_num();
    // 操作共有结构，加锁
    static pthread_mutex_t _conns_mutex;
// 之后应该放入配置文件当中
#define MAX_CONNS 10128
    // 当前允许的最大连接数
    static int _max_conns;
    // 当前管理的连接个数
    static int _curr_conn_num;

    // -----添加路由分发机制句柄
    static msg_router router;
    // 添加路由的方法
    void add_msg_router(int msgid, msg_callback *callback, void *user_data = nullptr) {
        router.register_msg_router(msgid, callback, user_data);
    }

    // v0.7
    // 设置连接创建之后
    static void set_conn_start_cb(conn_callback callback, void *args = nullptr) {
        conn_start_cb = callback;
        conn_start_cb_args = args;
    }
    static void set_conn_close_cb(conn_callback callback, void *args = nullptr) {
        conn_close_cb = callback;
        conn_close_cb_args = args;
    }
    // 创建连接之后 需要触发的回调函数
    static conn_callback conn_start_cb;
    static void *conn_start_cb_args;
    // 销毁连接之前 需要触发的回调函数
    static conn_callback conn_close_cb;
    static void *conn_close_cb_args;

    // ---v0.8 线程池 连接池
    thread_pool *_thread_p;
};