#pragma once
#include "header.h"
#include "tcp_conn.h"

// 固定消息头长度
#define MESSAGE_HEAD_LEN 8
// 消息体 最大长度限制
#define MESSAGE_LENGTH_LIMIT (65535 - MESSAGE_HEAD_LEN)

// 消息封装头
struct msg_head {
    // 消息id 以及消息长度
    int msgid;
    int msglen;
};


/**
 * 定义消息路由分发机制
 * */ 
// 定义一个路由回调函数类型
typedef void msg_callback(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data);
// 定义一个抽象连接类
// tcp_conn(TCP) tcp_conn(UDP)

class msg_router {
public:
    // msg_router构造函数，初始化两个map
    msg_router(): _router(), _args() {
        // std::cout << "msg_router init." << std::endl;
    }

    // 给一个消息ID注册一个对应的回调业务函数
    int register_msg_router(int msgid, msg_callback *msg_cb, void *user_data) {
        // 
        if (_router.find(msgid) != _router.end()) {
            std::cout << "msgid: " << msgid << " already registed" << std::endl;
            return -1;
        }
#if DEBUG
        std::cout << " add msg callback " << msgid << std::endl;
#endif
        _router[msgid] = msg_cb;
        _args[msgid] = user_data;
        return 0;
    }

    // 调用注册的回调函数业务
    void call(int msgid, uint32_t msglen, const char *data, net_connection *conn) {
#if DEBUG

        std::cout << "call msgid " << msgid << std::endl;
#endif
        if (_router.find(msgid) == _router.end()) {
            std::cout << "msgid " << msgid << " not registe" << std::endl;
            return;
        }
        // 直接取回调函数，并执行
        msg_callback *callback = _router[msgid];
        void *user_data = _args[msgid];
        // 调用最终的回调业务
        callback(data, msglen, msgid, conn, user_data);
    }

private:
    // 针对消息的路由分发，key msg_id value为回调函数
    std::unordered_map<int, msg_callback*> _router;
    // 每个路由业务函数对应的形参
    std::unordered_map<int, void*> _args;
};

