#pragma once

/**
 * 抽象连接类
 * */ 

class net_connection {
public:
    net_connection() {}
    virtual int send_message(const char *data, int msglen, int msgid) = 0;
private:

};

// 创建连接/销毁连接 需要触发的回调函数类型
typedef void (*conn_callback)(net_connection *conn, void *args);