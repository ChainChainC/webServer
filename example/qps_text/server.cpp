#include "tcp_server.h"
#include "message.h"
#include "echoMessage.pb.h"

// 定义一个回显业务
void callback_test(const char *data, uint32_t msglen, int msgid, net_connection *conn, void *user_data) {
    // 定义 请求包 和 回复包
    qps_test::EchoMessage request, response;
    // 解包，将data中的proto协议解析出来 放到request 对象中
    request.ParseFromArray(data, msglen);
    
    // 回显 制作一个新的proto数据包 EchoMessage类型的包
    response.set_id(request.id());
    response.set_content(request.content());
    // 将response 序列化
    std::string responseString;
    response.SerializeToString(&responseString);
    // 赋值

    // 发送给客户端
    conn->send_message(responseString.c_str(), responseString.size(), msgid);
}

// v0.7新客户端创建成功后使用的回调  以及端开前的回调
// void on_client_build(net_connection *conn, void *args) {
//     int msgid = 200;
//     const char *msg = "welcome . You are online.";
//     conn->send_message(msg, strlen(msg), msgid);
// }
// void on_client_lost(net_connection *conn, void *args) {
//     std::cout << "Client lost." << std::endl;
// }

int main() {
    event_loop loop;
    
    // tcp_server server(&loop, "172.16.164.130", 7777);
    tcp_server server(&loop, "172.16.164.130", 7777);
    // 注册回调方法
    server.add_msg_router(1, callback_test);
    // server.add_msg_router(2, callback_test2);
    // 注册hook函数
    // server.set_conn_start_cb(on_client_build);
    // server.set_conn_close_cb(on_client_lost);
    loop.event_process();

    return 0;
}