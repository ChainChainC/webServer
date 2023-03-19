#pragma once

/**
 * 定义一个 buffer 一块内存的数据结构
 * **/ 
class io_buf {
 public:
    // 构造函数
    io_buf(int size);
    // 清空函数（当模块所需内存需要释放时，将其清空而不是free）
    void clear();
    // 处理长度为len的数据
    void pop(int len);
    // 将已经处理的数据清空（内存抹去），将未处理的数据移动到buf首地址
    // length减小
    void adjust();

    // 将其他的io_buf 对象拷贝到自己中
    void copy(const io_buf *other);

    // 存在多个io_buf
    io_buf *next;

//  private:
    // 当前buf的总容量
    int capacity;

    // 当前buf的有效数据长度
    int length;

    // 当前未处理有效数据的头部索引（有效数据的位置）
    int head;

    // 当前buf的内存首地址
    char *data;

    
};

