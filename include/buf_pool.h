#pragma once
// linux环境下的 map包
#include <ext/hash_map>
#include <unordered_map>
#include "io_buf.h"

// 总内存池大小(1GB) 单位KByte
#define MEM_LIMIT (1024U * 1024 * 1024)

// 定义一个hash_map
// typedef __gnu_cxx::hash_map<int, io_buf*> pool_t;
typedef std::unordered_map<int, io_buf*> pool_t;


// 定义一些刻度，字节（Byte）
enum MEM_CAP {
    m4K = 4096,
    m16K = 16384,
    m64K = 65536,
    m512K = 524288,
    m1M = 1048576,
    m4M = 4194304,
};

// 单例模式
class buf_pool {
 public:
    
    // 提供一个静态的获取instance的方法
    static buf_pool *instance() {
        // 保证once来保证init在进程中只进行一次
        pthread_once(&_once, init);
        return _instance;
    }

    // 从内存池中申请内存
    io_buf *alloc_buf(int N);
    io_buf *alloc_buf();

    // 重置一个io_buf 方会pool中
    void revert(io_buf *buffer);

 private:
    // ========= 创建单例 ========
    // 构造函数私有化（拷贝构造，等号）
    buf_pool();
    buf_pool(const buf_pool&);
    const buf_pool &operator=(const buf_pool&);
    // 初始化单例对象
    static void init() {
        _instance = new buf_pool();
    }


    // 单例的对象（需要为静态）
    static buf_pool *_instance;
    // 加锁，用于保证创建单例的方法全局只执行一次
    static pthread_once_t _once;

    // ------- buf_pool 属性 ---------
    // 存放所有io_buf的map句柄
    pool_t _pool;
    // 存放当前内存池的总体大小
    uint64_t _total_mem;

    void make_io_buf_list(int cap, int num);

    // 需要一个锁，来对io_buf链表的运行中的 增删进行保护
    static pthread_mutex_t _mutex_buf_list;
};