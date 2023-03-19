#pragma once
#include "event_loop.h"
#include "reactor_buf.h"
#include "message.h"
#include "net_connection.h"


// typedef void msg_callback(const char *data, uint32_t len, int msgid, tcp_client *client, void *user_data);

class tcp_client:public net_connection {
public:
    tcp_client(event_loop *loop, const char *ip, unsigned short port);

    // 数据发送方法
    virtual int send_message(const char *data, int msglen, int msgid);

    // 处理读业务
    void do_read();
    // 处理写业务
    void do_write();
    // 释放连接
    void destory_conn();
    // 连接服务器
    void do_connect();
   
    // 输入输出缓冲区
    input_buf ibuf;
    output_buf obuf;
    // server 端的ip地址 端口
    struct sockaddr_in _server_addr;
    socklen_t _addrlen;
    void add_msg_router(int msgid, msg_callback *callback, void *user_data = nullptr) {
        _router.register_msg_router(msgid, callback, user_data);
    }

    // ---v0.7
    void set_conn_start_cb(conn_callback callback, void *args = nullptr) {
        this->_conn_start_cb = callback;
        this->_conn_start_cb_args = args;
    }
    void set_conn_close_cb(conn_callback callback, void *args = nullptr) {
        this->_conn_close_cb = callback;
        this->_conn_close_cb_args = args;
    }
    // 创建 销毁 hook回调
    conn_callback _conn_start_cb;
    void *_conn_start_cb_args;
    conn_callback _conn_close_cb;
    void *_conn_close_cb_args;

private:
    // 当前客户端连接
    int _sockfd;
    // 客户端事件epoll
    event_loop *_loop;
    // // 客户端处理服务器消息的回调业务函数
    // msg_callback *_msg_callback;
    // 消息分发机制
    msg_router _router;
    
};