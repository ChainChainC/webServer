#include "tcp_client.h"
#include "echoMessage.pb.h"
#include <unistd.h>
struct qpsCount
{
    // 最后一次发包的时间
    long last_time;
    // 成功收到服务器回显业务的次数
    int succ_cnt;
    qpsCount() {
        last_time = time(nullptr);
        succ_cnt = 0;
    }
};

// 测试qps
void qps_t(const char *data, uint32_t len, int msgid, net_connection *client, void *user_data) {
    qpsCount *qps_count = (qpsCount *)user_data;
    qps_test::EchoMessage request, response;
    // 解析server返回的数据包
    if (response.ParseFromArray(data, len) == false) {
        printf("server call back data error\n");
        return;
    }
    // std::cout << response.content() << std::endl;
    // if (response.content() == "QPS test!") {
    //     // 回显业务成功，认为QPS请求一次成功
    //     ++(qps_count->succ_cnt);
    // }
    
    
    ++(qps_count->succ_cnt);
    
    // 当前时间
    long current_time = time(nullptr);
    if (current_time - qps_count->last_time >= 1) {
        // 时间大于1s 需要统计qps成功的次数
        printf("QPS=[%d]\n", qps_count->succ_cnt);
        qps_count->last_time = current_time;
        qps_count->succ_cnt = 0;
    }

    // 发送request给客户端
    request.set_id(response.id() + 1);
    request.set_content(response.content());
    std::string requestStinrg;
    request.SerializeToString(&requestStinrg);
    client->send_message(requestStinrg.c_str(), requestStinrg.size(), msgid);
}

// ---v0.7客户端创建连接之后的业务
void on_client_build(net_connection *conn, void *args) {
    // 当与server连接成功后，主动发送一个包
    qps_test::EchoMessage request;
    request.set_id(1);
    request.set_content("QPS test! QPS test!QPS test!QPS \
    test!QPS test!QPS test!QPS test!QPS test! QPS test!QPS test!\
    QPS test!QPS test!QPS test!QPS test!QPS test!QPS test!\
    QPS test!QPS test!QPS test!QPS test!QPS test!QPS test!\
    QPS test!QPS test!QPS test!QPS test!QPS test!QPS test!\
    QPS test! QPS test!QPS test!QPS \
    test!QPS test!QPS test!QPS test!QPS test! QPS test!QPS test!\
    QPS test!QPS test!QPS test!QPS test!QPS test!QPS test!\
    QPS test!QPS test!QPS test!QPS test!QPS test!QPS test!\
    QPS test!QPS test!QPS test!QPS test!QPS test!QPS test!");
    std::string requestString;
    request.SerializeToString(&requestString);
    // 26字节左右
    requestString.size();
    
    // 发送给server一个消息
    int msgid = 1;
    conn->send_message(requestString.c_str(), requestString.size(), msgid);
}


// int main() {
//     event_loop loop;
//     qpsCount qps_count;
//     // tcp_client client(&loop, "172.16.164.130", 7777);
    
//     tcp_client client(&loop, "127.0.0.1", 7777);
//     client.add_msg_router(1, qps_t, (void *)&qps_count);


//     // 注册hook函数
//     client.set_conn_start_cb(on_client_build);
//     // client.set_conn_close_cb(on_client_lost);

//     loop.event_process();
//     return 0;
// }
void *thread_main(void *args) {
    event_loop loop;
    qpsCount qps_count;
    // tcp_client client(&loop, "172.16.164.130", 7777);
    // printf("");
    tcp_client client(&loop, "172.16.164.130", 7777);
    client.add_msg_router(1, qps_t, (void *)&qps_count);


    // 注册hook函数
    client.set_conn_start_cb(on_client_build);
    // client.set_conn_close_cb(on_client_lost);

    loop.event_process();
    return nullptr;
}

extern "C"
int main(int argc, char **argv) {
    if (argc == 1) {
        printf("./cilent conn num");

    }
    int num = atoi(argv[1]);
    pthread_t *tids;
    tids = new pthread_t[num];
    for (int i = 0; i < num; i++) {
        printf("thread %d", i);
        pthread_create(&tids[i], NULL, thread_main, NULL);
        usleep(10000);
    }
    for (int i = 0; i < num; i++) {
        pthread_join(tids[i], NULL);
        // sleep(1);
    }
    return 0;
}