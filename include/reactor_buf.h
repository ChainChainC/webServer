#pragma once
#include "io_buf.h"
#include "buf_pool.h"

// 提供读写duf 的基类
class reactor_buf {
 public:
    reactor_buf();
    ~reactor_buf();

    // 得到当前的buf含有多少有效数据
    int length();

    // 已经处理了多少数据
    void pop(int len);

    // 将当前buf清空
    void clear();

 protected:
    io_buf *_buf;

};

// 读buffer
class input_buf : public reactor_buf {
 public:
    // 从一个fd中读取数据到reactor_buf当中
    int read_data(int fd);

    // 获取当前数据 
    const char *data();

    // 重置缓冲区
    void adjust();
};

// 写buffer
class output_buf : public reactor_buf {
 public:
    // 将一段数据 写到 自身_buf中，方便后续写到fd
    int send_data(const char *data, int datalen);
    // 将buf中的数据写到fd中（封装io 层面的write方法）
    int write_to_fd(int fd);

};