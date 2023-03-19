#pragma once
#include "event_loop.h"
#include "header.h"
#include "reactor_buf.h"
#include "net_connection.h"
// 

class tcp_conn;

class tcp_conn: public net_connection{
public:
    // 初始化conn （对应套接子，以及所属的 epoll）
    tcp_conn(int connfd, event_loop *loop);
    // 被动处理读业务的方法（回调方法在此处实现）
    // 回调反法应该与业务直接相连，所以应该在此处实现
    // 被动读
    void do_read();
    // 被动写
    void do_write();
    // 主动发送消息的方法（如广播？）
    virtual int send_message(const char *data, int msglen, int msgid);
    // 销毁当前链接
    void destory_conn();

private:
    // 当前conn 对应的套接字 fd
    int _connfd;
    // 当前链接归属哪个event_loop
    event_loop *_loop;
    // 输出output_buf
    output_buf obuf;
    // 输入input_buf
    input_buf ibuf;
};