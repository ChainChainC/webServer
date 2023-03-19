#include "tcp_conn.h"
#include "tcp_server.h"
#include "message.h"


void conn_rd_callback(event_loop *loop, int fd, void *args) {
    tcp_conn *conn = (tcp_conn *)args;
    conn->do_read();
}

void conn_wd_callback(event_loop *loop, int fd, void *args) {
    tcp_conn *conn = (tcp_conn *)args;
    conn->do_write();
}

// void callback_test(const char *data, int msglen, int msgid, void *args, tcp_conn *conn) {
//     // 让tcp主从发消息给对应client
//     conn->send_message(data, msglen, msgid);
// }


// 初始化conn （对应套接子，以及所属的 epoll）
tcp_conn::tcp_conn(int connfd, event_loop *loop) {
    _connfd = connfd;
    _loop = loop;

    // 1、将connfd设置成非阻塞状态
    // 将connfd状态全部清空-> 设置非阻塞
    int flag = fcntl(_connfd, F_GETFL, 0);
    fcntl(_connfd, F_SETFL, O_NONBLOCK|flag);
    // 2、设置TCP_NODELAY状态， 禁止读写缓存， 降低小包延迟
    // （内核中如果接收到较小的数据包，会放入缓存而不进行epoll通知）
    int op = 1;
    setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));

    // 3、v0.7----执行创建连接成功后需要触发的回调函数
    if (tcp_server::conn_start_cb != nullptr) {
        tcp_server::conn_start_cb(this, tcp_server::conn_start_cb_args);
    }

    // 创建完成后，将监听部分操作转移到该部分实现
    _loop->add_io_event(_connfd, conn_rd_callback, EPOLLIN, this);

    // v0.5将自己添加到conns管理中
    tcp_server::add_conn(_connfd, this);
}

// 被动处理读业务的方法（回调方法在此处实现）
// 回调反法应该与业务直接相连，所以应该在此处实现
// 被动读
void tcp_conn::do_read() {
    // 隐藏了一个this指针，直接将do_read当callback会导致参数部匹配
    // 1、从connfd中读取数据
    int ret = ibuf.read_data(_connfd);
    if (ret == -1) {
        fprintf(stderr, "read data from socket error\n");
        this->destory_conn();
        return;
    }
    else if (ret == 0) {
        // 对端客户正常关闭
        printf("peer client closed\n");
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
        // 将该部分数据进行一个固定的链接业务（回显）
        // TODO根据不同的msgid执行不同的业务
        // callback_test(ibuf.data(), head.msglen, head.msgid, nullptr, this);
        tcp_server::router.call(head.msgid, head.msglen, ibuf.data(), this);
        // 该部分消息处理完成
        ibuf.pop(head.msglen);
    }

    ibuf.adjust();
    
}

// 被动写
void tcp_conn::do_write() {
    // 调用给方法表示obuf中数据已经准备好了，因此将obuf中数据发出
    while (obuf.length()) {
        int write_num = obuf.write_to_fd(_connfd);
        if (write_num == -1) {
            fprintf(stderr, "tcp_conn write connfd error\n");
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
        _loop->del_io_event(_connfd, EPOLLOUT);
    }

    return;
}

// 主动发送消息的方法（如广播？、webSocket）
int tcp_conn::send_message(const char *data, int msglen, int msgid) {
#if DEBUG
    printf("Server send back client message: data: %s, msglen: %d, msgid: %d\n", data, msglen, msgid);
#endif
    bool activate_epollout = false;
    if (obuf.length() == 0) {
        // （面对多次调用写事件时，每个事件对应一次写事件激活）
        // 如果obuf为空，代表是一个新的消息，需要激活写事件
        // 如果obuf不为空，说明数据没有完全写道套接字，那么不需要激活写事件
        activate_epollout = true;
    }

    // 1、封装message 头
    msg_head head;
    head.msgid = msgid;
    head.msglen = msglen;
    // ibuf obuf都已经封装好了，直接调用方法会自动判断并申请合适的空间，自动释放
    // 概括下来，obuf发完会自动释放，ibuf写前会自动申请
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
        _loop->add_io_event(_connfd, conn_wd_callback, EPOLLOUT, this);
    }

    return 0;
}

// 销毁当前链接;
void tcp_conn::destory_conn() {
    // v0.7----将连接要销毁前需要触发的回调
    if (tcp_server::conn_close_cb != nullptr) {
        tcp_server::conn_close_cb(this, tcp_server::conn_close_cb_args);
    }
    // ---
    // 链接清理，需要清除io事件，缓存交还，描述符关闭
    _loop->del_io_event(_connfd);
    ibuf.clear();
    obuf.clear();
    
    // --v0.5增加
    tcp_server::delete_conn(_connfd);
    // -- 

    close(_connfd);

}