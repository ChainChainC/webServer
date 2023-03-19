#include "thread_pool.h"

// typedef void io_callback(event_loop *loop, int fd, void *args);
/**
 * 一旦有task任务消息过来， 该业务函数
 * */ 
void deal_task(event_loop *loop, int fd, void *args) {
    // 1、从队列中取数据
    thread_queue<task_msg> *i_queue = (thread_queue<task_msg> *)args;

    // 取出queue中的revc event方法， 因此n_task_queue指向队列中的所有消息
    std::queue<task_msg> n_task_queue;
    i_queue->recv_event(n_task_queue);
    while (!n_task_queue.empty()) {
        task_msg task = n_task_queue.front();
        n_task_queue.pop();
        // 2、判断任务类型  如果任务1：新建连接任务
        //
        if (task.t_type == task_msg::NEW_CONN) {
            // 连接任务
            // 让当前线程来创建一个连接，同时将这个连接connfd加入到当勤thread的回调函数来执行
            tcp_conn *conn = new tcp_conn(task.connfd, loop);
            if (conn == nullptr) {
                fprintf(stderr, "in thread new tcp_conn error\n");
                exit(1);
            }
            printf("[thread]: create new connection\n");
        }
        else if (task.t_type == task_msg::NEW_TASK) {
            // TODO
        }
        else {
            // TODO
            fprintf(stderr, "unknow task!\n");
        }
    }

}

// 线程的主业务函数
void *thread_main(void *args) {
    thread_queue<task_msg> *queues = (thread_queue<task_msg> *)args;
    event_loop *loop = new event_loop();
    if (loop == nullptr) {
        fprintf(stderr, "new epoll for thread error\n");
        exit(1);
    }

    queues->set_loop(loop);
    queues->set_callback(deal_task, queues);

    // 启动loop监听
    loop->event_process();
    return nullptr;
}

// 
thread_queue<task_msg> *thread_pool::get_thread() {
    if (_index == _thread_cnt) {
        _index = 0;
    }
    return _queues[_index++];
}

// 初始化线程池的构造函数，每次创建server对象时 创建一个线程池
thread_pool::thread_pool(int thread_cnt) {
    _queues = nullptr;
    _thread_cnt = thread_cnt;
    _index = 0;
    if (_thread_cnt <= 0) {
        fprintf(stderr, "thread num >= 0 needed\n");
        exit(1);
    }
    // 创建thread_queue
    _queues = new thread_queue<task_msg> *[thread_cnt];
    _tids = new pthread_t[thread_cnt];
    // 开辟线程
    int ret;
    for (int i = 0; i < thread_cnt; i++) {
        // 给一个thread_queue开辟内存空间
        _queues[i] = new thread_queue<task_msg>();
        // 第i个线程 绑定到第i个thread_queue
        ret = pthread_create(&_tids[i], nullptr, thread_main, _queues[i]);
        if (ret == -1) {
            fprintf(stderr, "thread_pool thread create error.\n");
            exit(1);
        }
        // 将线程设置为detach模式 线程分离模式，子线程不会随主线程下线而下线
        pthread_detach(_tids[i]);
    }

    return;
}