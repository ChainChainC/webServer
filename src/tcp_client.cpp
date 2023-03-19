#include "tcp_client.h"


void read_callback(event_loop *loop, int fd, void *args) {
    tcp_client *cli = (tcp_client *)args;
    cli->do_read();
}

void write_callback(event_loop *loop, int fd, void *args) {
    tcp_client *cli = (tcp_client *)args;
    cli->do_write();
}

// 帮助判断连接是否可写（是否成功）
void connection_succ_callback(event_loop *loop, int fd, void *args) {
    tcp_client *cli = (tcp_client *)args;
    // fd不存在的话接口也是可靠的
    // 判断完可写后，需要将EPOLLOUT事件摘除，不然会一直处于可写状态，一只进入回调
    loop->del_io_event(fd);

    // 再对当前fd进行一次 错误编码的获取 如果没有任何错误，那么一定成功
    // 否则认为创建失败
    int result = 0;
    socklen_t res_len = sizeof(result);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &res_len);
    if (result == 0) {
        // 没有任何错误，连接创建成功
        printf("connection succ!\n");
        // 连接成功后，进行业务操作
        // ----v0.7
        if (cli->_conn_start_cb != nullptr) {
            cli->_conn_start_cb(cli, cli->_conn_start_cb_args);
        }
#if 0
        // 客户端业务，主动发送一个消息给server
        const char *msg = "hello lars, i'm client.";
        int msgid = 1;
        // send_message是将数据写到obuf当中
        cli->send_message(msg, strlen(msg), msgid);
#endif
        // 发完初始化消息，准备读
        loop->add_io_event(fd, read_callback, EPOLLIN, cli);

        if (cli->obuf.length() != 0) {
            // 让event_loop触发写回调业务
            // loop->add_io_event(fd, write_callback, EPOLLOUT, cli);
        }
    }
    else {
        fprintf(stderr, "connection %s : %d error\n", inet_ntoa(cli->_server_addr.sin_addr), ntohs(cli->_server_addr.sin_port));
        return;
    }

}


tcp_client::tcp_client(event_loop *loop, const char *ip, unsigned short port): _router() {
    _sockfd = -1;
    _loop = loop;
    // ----v0.7
    _conn_start_cb = nullptr;
    _conn_close_cb = nullptr;
    _conn_start_cb_args = nullptr;
    _conn_close_cb_args = nullptr;
    // ---
    // 封装即将要连接的远程server端的ip地址
    bzero(&_server_addr, sizeof(_server_addr));
    // ipv4
    _server_addr.sin_family = AF_INET;
    inet_aton(ip, &_server_addr.sin_addr);
    _server_addr.sin_port = htons(port);
    _addrlen = sizeof(_server_addr);

    // 连接元词客户端
    do_connect();
}

// 数据发送方法
int tcp_client::send_message(const char *data, int msglen, int msgid) {
#if DEBUG
    printf("client send message\n");
#endif
    bool activate_epollout = false;
    if (obuf.length() == 0) {
        activate_epollout = true;
    }
    msg_head head;
    head.msgid = msgid;
    head.msglen = msglen;

    int ret = obuf.send_data((const char *)&head, MESSAGE_HEAD_LEN);
    if (ret != 0 ) {
        fprintf(stderr, "send head error\n");
        // 包头部数据写入错误，需要将包头部信息弹出
        // obuf.pop(MESSAGE_HEAD_LEN);
        return -1;
    }
    // 2、写消息体
    ret = obuf.send_data(data, msglen);
    if (ret != 0 ) {
        fprintf(stderr, "send data error\n");
        // 包头部数据写入错误，需要将包头部信息弹出
        obuf.pop(MESSAGE_HEAD_LEN);
        return -1;
    }
    // 3、将_connfd添加一个写事件 EPOLLOUT， 让回调函数将obuf中的数据如套接字
    if (activate_epollout == true) {
        _loop->add_io_event(_sockfd, write_callback, EPOLLOUT, this);
    }
    return 0;
}

// 处理读业务
void tcp_client::do_read() {
#if DEBUG
    printf("client: do_read\n");
#endif
    // 1、从connfd中读取数据
    int ret = ibuf.read_data(_sockfd);
    if (ret == -1) {
        fprintf(stderr, "read data from socket error\n");
        this->destory_conn();
        return;
    }
    else if (ret == 0) {
        // 对端客户正常关闭
        printf("peer server closed\n");
        this->destory_conn();
        return;
    }
    // 2、度过来的数据是否满足8字节（头部字段大小）
    msg_head head;
    while (ibuf.length() >= MESSAGE_HEAD_LEN) {
        // 2.1 先拿到头部，得到msgid msglen
        memcpy(&head, ibuf.data(), MESSAGE_HEAD_LEN);
        if (head.msglen > MESSAGE_LENGTH_LIMIT || head.msglen < 0) {
            fprintf(stderr, "data format error, invalid\n");
            this->destory_conn();
            break;
        }
        
        // 2.2 判断得到的消息体的长度和头部里的长度是否一致
        if (ibuf.length() < MESSAGE_HEAD_LEN + head.msglen) {
            // 取道的数据小于应该接受到的数据
            // 说明包不完整
            break;
        }
        // 2.3 数据包完整，再根据msglen 再去读取 从connfd中读取
        ibuf.pop(MESSAGE_HEAD_LEN);
#if DEBUG
        std::cout << "read data = " << ibuf.data() << std::endl;
#endif
#if 0
        // 将该部分数据进行一个固定的链接业务（回显）
        if (_msg_callback != nullptr) {
            // 该部分提供了set方法，因此需要使用者定义目标方法
            this->_msg_callback(ibuf.data(), head.msglen, head.msgid, this, nullptr);
        }
#endif
        // 调用注册好的回调业务
        this->_router.call(head.msgid, head.msglen, ibuf.data(), this);
        // 该部分消息处理完成
        ibuf.pop(head.msglen);
    }

    ibuf.adjust();
}

// 处理写业务
void tcp_client::do_write() {
#if DEBUG
    printf("client: do_write\n");
#endif
    // 调用给方法表示obuf中数据已经准备好了，因此将obuf中数据发出
    while (obuf.length()) {
        int write_num = obuf.write_to_fd(_sockfd);
        if (write_num == -1) {
            fprintf(stderr, "tcp_client write fd error\n");
            this->destory_conn();
            // 此处不需要将obuf重置吗
            return;
        }
        else if (write_num == 0) {
            // 当前不可写，让epoll wait阻塞而不是一直循环
            break;
        }
    }
    if (obuf.length() == 0) {
        // 数据写完，将_connfd的写事件删除
        _loop->del_io_event(_sockfd, EPOLLOUT);
        // _loop->add_io_event(_sockfd, write_callback, EPOLLIN, this);
    }

    return;
}

// 释放连接
void tcp_client::destory_conn() {
    if (_sockfd == -1) {
        return;
    }
    // 在销毁之前调用 hook函数
    if (_conn_close_cb != nullptr) {
        _conn_close_cb(this, _conn_close_cb_args);
    }

    printf("close socked\n");
    _loop->del_io_event(_sockfd);
    close(_sockfd);
    

}

// 连接服务器
void tcp_client::do_connect() {
    // 如果已经有连接，关闭重连 
    if (_sockfd != -1) {
        close(_sockfd);
    }

    // 创建套接字,非阻塞
    // 如果无epoll，套接字会在read地方阻塞
    // 使用epoll，都需要将套接字设置为非阻塞？
    _sockfd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, IPPROTO_TCP);
    if (_sockfd == -1) {
        fprintf(stderr, "create tcp client failed\n");
        exit(-1);
    }

    // 使用非阻塞套接字创建连接，该地方会出现问题，连接后对端显示成功，但是会立刻中断
    // 这是由于此时connect会返回-1，且errno为EINPROGRESS的情况
    // 但client使用event loop形式，socket需要设置为非阻塞
    int ret = connect(_sockfd, (const sockaddr *)&_server_addr, _addrlen);
    if (ret == 0) {
        // 有可能能够成功
        connection_succ_callback(_loop, _sockfd, this);
    }
    else {
        if (errno == EINPROGRESS) {
            // fd是非阻塞的，会出现该错误，判断其是否可写
#if DEBUG
            printf("do_connect, EINPROGRESS\n");
#endif         
            _loop->add_io_event(_sockfd, connection_succ_callback, EPOLLOUT, this);
            
        }
        else {
            // 连接创建失败
            fprintf(stderr, "connection failed\n");
            exit(-1);
        }
        
    }
}