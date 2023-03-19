#include "io_buf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

io_buf::io_buf(int size) {
    // 容量
    capacity = size;
    length = 0;
    head = 0;
    next = nullptr;

    data = new char[size];

    // data为空会导致后面内存的非法访问，因此不能为空
    assert(data != nullptr);
}

// 清空函数（当模块所需内存需要释放时，将其清空而不是free）
void io_buf::clear() {
    length = head = 0;
}

// 处理长度为len的数据
void io_buf::pop(int len) {
    length -= len;
    head += len;
}

// 将已经处理的数据清空（内存抹去），将未处理的数据移动到buf首地址
// length减小
void io_buf::adjust() {
    // 如果head等于0
    if (head != 0) {
        // 或者length为0都不需要操作
        if (length != 0) {
            // memcpy（从头部移）  memmove(从尾部往前移动)
            // 将
            memmove(data, data + head, length);
        }
        head = 0;
    }
}

// 将其他的io_buf 对象拷贝到自己中
void io_buf::copy(const io_buf *other) {
    // 将other的数据拷贝到自身（从目标的head 位置，拷贝目标有效长度的数据）
    memcpy(data, other->data + other->head, other->length);
    head = 0;
    length = other->length;
}