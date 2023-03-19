#include "tcp_server.h"

// -------连接管理相关
// 静态遍历需要在外部初始化
std::unordered_map<int, tcp_conn *> tcp_server::_conn_map;
tcp_conn **tcp_server::_conns = nullptr;
pthread_mutex_t tcp_server::_conns_mutex = PTHREAD_MUTEX_INITIALIZER;
int tcp_server::_max_conns = 0;
int tcp_server::_curr_conn_num = 0;
// --路由分发机制句柄初始化
msg_router tcp_server::router;

// ----v0.7初始化hook函数
conn_callback tcp_server::conn_start_cb = nullptr;
conn_callback tcp_server::conn_close_cb = nullptr;
void *tcp_server::conn_close_cb_args = nullptr;
void *tcp_server::conn_start_cb_args = nullptr;

// 新增一个连接
void tcp_server::add_conn(int connfd, tcp_conn *conn) {
    pthread_mutex_lock(&_conns_mutex);
    _conn_map[connfd] = conn;
    // _conns[connfd] = conn;
    ++_curr_conn_num;
    pthread_mutex_unlock(&_conns_mutex);
}

void tcp_server::delete_conn(int connfd) {
    pthread_mutex_lock(&_conns_mutex);
    if (_conn_map[connfd] != nullptr) {
        free(_conn_map[connfd]);
    }
    _conn_map[connfd] = nullptr;
    _conn_map.erase(connfd);
    // 放回线程池，或者回收
    // if (_conns[connfd] != nullptr) {
    //     free(_conns[connfd]);
    // }
    // _conns[connfd] = nullptr;
    --_curr_conn_num;
    pthread_mutex_unlock(&_conns_mutex);
}

// 获取当前在线连接个数
int tcp_server::get_conn_num() {
    return _curr_conn_num;
}

// -------

// accept 回调函数
void accept_callback(event_loop *loop, int fd, void *args) {
    // server->do_accept
    tcp_server *server = (tcp_server *)args;
    server->do_accept();
}

// 构造函数
tcp_server::tcp_server(event_loop *p_loop, const char *ip, uint16_t port) {
    // 0.忽略一些信号   SIGHUP, SIGPIPE
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "signal ignore SIGHUB\n");
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "signal ignore SIGHUB\n");
    }

    // 1、创建套接字
    _sockfd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC, IPPROTO_TCP);
    if (_sockfd == -1) {
        fprintf(stderr, "TCP::server socke() open failed\n");
        exit(-1);
    }

    // 2、初始化服务器地址
    struct sockaddr_in server_addr;

    // strings.h中的遗留函数，用memset代替 
    // string.h 是c标准库中的， strings.h是linux下某个库遗留的
    // memset(&server_addr, sizeof(server_addr), 0);
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_aton(ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);
    // 3、绑定端口
    if (bind(_sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "bind error\n");
        exit(-1);
    }

    // 2.5设置 sock可以重复监听 SO_REUSE
    int op = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op)) < 0) {
        fprintf(stderr, "set socketopt reuse error\n");
    }

    // 4、监听
    if (listen(_sockfd, 500) == -1) {
        fprintf(stderr, "listen error\n");
        exit(-1);
    }

    // 5、将形参p_loop加入到tcp_serve _loop中
    _loop = p_loop;

    // 7、创建连接管理
    // TODO从配置文件中读取个数
    // --v0.8创建线程池
    int thread_cnt = THREAD_NUM;
    if (thread_cnt > 0) {
        _thread_p = new thread_pool(thread_cnt);
        if (_thread_p == nullptr) {
            fprintf(stderr, "tcp server new thread pool error\n");
            exit(1);
        }
    }

    _max_conns = MAX_CONNS;
    // stdin, stdout, stderr, 3个进程默认的文件描述符
    // _conns = new tcp_conn*[_max_conns + 5 + 2 * thread_cnt];
    _conn_map.reserve(MAX_CONNS);
    // if (_conns == nullptr) {
    //     fprintf(stderr, "new conns [%d] error\n", _max_conns);
    //     exit(1);
    // }
    
    // 8、注册_sockfd 读事件 ---> accept处理
    _loop->add_io_event(_sockfd, accept_callback, EPOLLIN, this);

}

// 析构
tcp_server::~tcp_server() {
    // for (auto value : )
    close(_sockfd);
}

void tcp_server::do_accept() {
    int connfd;
    // 1、accpet操作
    // accept成功后会获取到客户端地址
    connfd = accept(_sockfd, (struct sockaddr *)&_connaddr, &_addrlen);
    if (connfd == -1) {
        if (errno == EINTR) {
            // 中断错误
            fprintf(stderr, "accept errno = EINTR\n");
            // continue;
        }
        else if (errno == EAGAIN) {
            // 非阻塞错误
            fprintf(stderr, "accept errno = EAGAIN\n");
            // break;
        }
        else if (errno == EMFILE) {
            // 建立连接过多，文件描述符不够
            fprintf(stderr, "accept errno = EMFILE\n");
            // continue;
        }
        else {
            fprintf(stderr, "accept errno\n");
            exit(-1);
        }
    }
    else {
        // 判断当前连接数是否超出限制
        if (this->get_conn_num() >= _max_conns) {
            fprintf(stderr, "too many connections\n");
            close(connfd);
            return;
        }
        if (_thread_p != nullptr) {
            // 开启了多线程模式
            // 将该connfd交给线程来创建
            // 1 从线程池中拿到thread
            // 2 将connfd发送到对应线程队列
            thread_queue<task_msg> *queue_1 = _thread_p->get_thread();

            // 创建一个任务
            task_msg task;
            task.t_type = task_msg::NEW_CONN;
            task.connfd = connfd;
            queue_1->send_event(task);
        }
        else {
            // 没有开启多线程模式
            // accept成功
#if DEBUG
            std::cout << "accept new client.\n" << std::endl;
#endif
            // 创建连接对象conn
            tcp_conn *conn = new tcp_conn(connfd, _loop);
            if (conn == nullptr) {
                fprintf(stderr, "new tcp_conn object failed.\n");
                exit(1);
            }
#if DEBUG
            printf("Get new connection succ.\n");
#endif
        }
        
        // this->_loop->add_io_event(connfd, server_rd_callback, EPOLLIN, &msg);
        // TODO 心跳机制

        // TODO 消息队列
    }
}