#include "buf_pool.h"
#include <assert.h>

// 单例对象（懒汉，不new）
buf_pool *buf_pool::_instance = nullptr;

pthread_once_t buf_pool::_once = PTHREAD_ONCE_INIT;

// 初始化锁
pthread_mutex_t buf_pool::_mutex_buf_list = PTHREAD_MUTEX_INITIALIZER;

// 
void buf_pool::make_io_buf_list(int cap, int num) {
    io_buf *prev = new io_buf(cap);
    // 开辟4K内存池
    assert(prev != nullptr);
    _pool[cap] = prev;
    _total_mem += cap;
    // 4k的buf 预先开辟1024个（除去最开始的1个）4M
    for (int i = 1; i < num; i++) {
        if (_total_mem > MEM_LIMIT - cap) {
            return;
        }
        prev->next = new io_buf(cap);
        // 如果其值为假（即为0），则会stderr打印错误信息，并终止程序
        assert(prev->next != nullptr);
        _total_mem += cap;
        prev = prev->next;
    }

}

// 构造函数
buf_pool::buf_pool():_total_mem(0) {
    make_io_buf_list(m4K, 1024);
    make_io_buf_list(m16K, 512);
    make_io_buf_list(m64K, 256);
    make_io_buf_list(m512K, 64);
    make_io_buf_list(m1M, 32);
    make_io_buf_list(m4M, 16);
}

io_buf *buf_pool::alloc_buf(int N) {
    int index;
    // 1、找到大于N 大小的io_buf
    if (N <= m4K) {
        index = m4K;
    }
    else if (N <= m16K) {
        index = m16K;
    }
    else if (N <= m64K) {
        index = m64K;
    }
    else if (N <= m512K) {
        index = m512K;
    }
    else if (N <= m1M) {
        index = m1M;
    }
    else if (N <= m4M) {
        index = m4M;
    }
    else {
        return nullptr;
    }

    // 2、如果链表空了，则需要再次申请内存（new）
    // 由于申请过程需要变动链表基本结构，因此需要加锁操作
    pthread_mutex_lock(&_mutex_buf_list);
    if (_pool[index] == nullptr) {
        // index空闲链表为空，需要重新申请index大小的io_buf
        if (_total_mem + index >= MEM_LIMIT) {
            // 超出内存限制，可能存在内存泄露，直接终止
            fprintf(stderr, "buf_pool out of memery\n");
            exit(-1);
        }
        io_buf *n_buf = new io_buf(index);
        assert(n_buf != nullptr);
        _total_mem += index;
        pthread_mutex_unlock(&_mutex_buf_list);
        // 此处应需要将拿到的buf放到以分配内存 链表
        return n_buf;
    }
    
    // 3、如果index有内存，从pool中拆除一块内存
    io_buf *target = _pool[index];
    _pool[index] = target->next;
    pthread_mutex_unlock(&_mutex_buf_list);
    target->next = nullptr;
    return target;
}

io_buf *buf_pool::alloc_buf() {
    return alloc_buf(m4K);
}

void buf_pool::revert(io_buf *buffer) {
    // 将取到的buffer放回（这样就没法避免内存泄露问题）
    int index = buffer->capacity;
    if (_pool.find(index) == _pool.end()) {
        // 非法内存
        fprintf(stderr, "invalid memery revert\n");
        exit(-1);
    }
    buffer->length = 0;
    buffer->head = 0;
    pthread_mutex_lock(&_mutex_buf_list);
    buffer->next = _pool[index];
    _pool[index] = buffer;
    pthread_mutex_unlock(&_mutex_buf_list);
}