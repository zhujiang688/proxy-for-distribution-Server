#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <json/json.h>
#include <string>

#include "structure.h"
#include "mini_log.h"
#include "vector"
#define COUNT 10000

std::vector<bufferevent *> buf_vec;
clock_t start_time;
clock_t final_time;
int receive_count;
void server_socket_read_cb(bufferevent *bev, void *arg) {
    //检查是否够一个头部的长度
    if (evbuffer_get_length(bufferevent_get_input(bev)) < sizeof(Head)) {
        LogWarn("不足头部长度");
        return;
    }
    Head head;
    //复制出头部
    assert(evbuffer_copyout(bufferevent_get_input(bev), (void *)&head, sizeof(head)) != -1);
    //检查该包是否完整到达
    if (evbuffer_get_length(bufferevent_get_input(bev)) < sizeof(Head) + head.length) {
        LogWarn("不足包体长度");
        return;
    }
    //取出一个包
    bufferevent_read(bev, (char *)&head, sizeof(head));
    char buffer[1024] = {0};
    bufferevent_read(bev, buffer, head.length);
    std::string msg(buffer);
    Json::Value response;
    Json::Reader reader;                 //用于将字字符串转换为value对象
    if (!reader.parse(msg, response)) {  //解码
        LogWarn("json reader parse error!");
        return;
    }
    LogInfo("服务器返回信息：%s.", response["message"].asCString());
    receive_count++;
    if ((receive_count % 1000) == 0) {
        std::cout << (clock() - start_time) / CLOCKS_PER_SEC << std::endl;
    }
}

void server_event_cb(struct bufferevent *bev, short event, void *arg) {
    if (event & BEV_EVENT_EOF) {
        LogWarn("server connection closed");
    } else if (event & BEV_EVENT_ERROR) {
        LogWarn("some other error in server");
    }
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
    event_base_loopexit((event_base *)arg, NULL);
}

void cmd_msg_cb(int fd, short events, void *arg) {
    char msg[1024];

    int ret = read(fd, msg, sizeof(msg));
    if (ret < 0) {
        perror("read fail ");
        exit(1);
    }
    // struct bufferevent *bev = (struct bufferevent *)arg;
    Head head;
    Json::Value login;
    // login["client_sock"] = 178;
    if (msg[0] == '1') {
        login["type"] = EN_INQUIRY;
        // std::cout << "Please enter the your name: ";
        // bzero(msg, 1024);
        // fgets(msg, 1024, stdin);
        // msg[strcspn(msg, "\n")] = 0;
        // login["name"] = msg;
        // std::cout << "Please enter the your password: ";
        // bzero(msg, 1024);
        // fgets(msg, 1024, stdin);
        // msg[strcspn(msg, "\n")] = 0;
        // login["password"] = msg;
        std::cout << "Please enter the name of the task to query: ";
        bzero(msg, 1024);
        fgets(msg, 1024, stdin);
        msg[strcspn(msg, "\n")] = 0;
        login["task"] = msg;
        // std::cout << login.toStyledString().c_str() << std::endl;
        // for (auto x:buf_vec)
        // {
        //     // login["name"] = ("client" + std::to_string(i)).c_str();
        //     // login["password"] = std::to_string(i).c_str();
        //     head.length = strlen(login.toStyledString().c_str());
        //     //std::cout << login.toStyledString().c_str() << std::endl;
        //     bufferevent_write(x, (char *)&head, sizeof(Head)); //消息的长度
        //     bufferevent_write(x, login.toStyledString().c_str(), head.length);
        // }
        for (int i = 0; i < COUNT; i++) {
            std::string name = "client";
            name += std::to_string(i+1);
            login["name"] = name.c_str();
            login["password"] = std::to_string(i+1).c_str();
            head.length = strlen(login.toStyledString().c_str());
            // std::cout << login.toStyledString().c_str() << std::endl;
            bufferevent_write(buf_vec[i], (char *)&head, sizeof(Head));  //消息的长度
            bufferevent_write(buf_vec[i], login.toStyledString().c_str(), head.length);
        }
    } else if (msg[0] == '2') {
        login["type"] = EN_SCRAMBLE;  //抢票
        // std::cout << "Please enter the your name: ";
        // bzero(msg, 1024);
        // fgets(msg, 1024, stdin);
        // msg[strcspn(msg, "\n")] = 0;
        // login["name"] = msg;
        // std::cout << "Please enter the your password: ";
        // bzero(msg, 1024);
        // fgets(msg, 1024, stdin);
        // msg[strcspn(msg, "\n")] = 0;
        // login["password"] = msg;
        std::cout << "Please enter the name of the task to scramble: ";
        bzero(msg, 1024);
        fgets(msg, 1024, stdin);
        msg[strcspn(msg, "\n")] = 0;
        login["task"] = msg;
        // std::cout << login.toStyledString().c_str() << std::endl;
        // for (auto x:buf_vec)
        // {
        //     // login["name"] = ("client" + std::to_string(i)).c_str();
        //     // login["password"] = std::to_string(i).c_str();
        //     head.length = strlen(login.toStyledString().c_str());
        //     //std::cout << login.toStyledString().c_str() << std::endl;
        //     bufferevent_write(x, (char *)&head, sizeof(Head)); //消息的长度
        //     bufferevent_write(x, login.toStyledString().c_str(), head.length);
        // }
        for (int i = 0; i < COUNT; i++) {
            std::string name = "client";
            name += std::to_string(i+1);
            login["name"] = name.c_str();
            login["password"] = std::to_string(i+1).c_str();
            head.length = strlen(login.toStyledString().c_str());
            // std::cout << login.toStyledString().c_str() << std::endl;
            bufferevent_write(buf_vec[i], (char *)&head, sizeof(Head));  //消息的长度
            bufferevent_write(buf_vec[i], login.toStyledString().c_str(), head.length);
        }
    }
    // else if (msg[0] == '3')
    // {
    //     login["type"] = EN_CREATE; //抢票
    //     std::cout << "Please enter the your name: ";
    //     bzero(msg, 1024);
    //     fgets(msg, 1024, stdin);
    //     msg[strcspn(msg, "\n")] = 0;
    //     login["name"] = msg;
    //     std::cout << "Please enter the your password: ";
    //     bzero(msg, 1024);
    //     fgets(msg, 1024, stdin);
    //     msg[strcspn(msg, "\n")] = 0;
    //     login["password"] = msg;
    //     std::cout << "Please enter the name of the task to creat: ";
    //     bzero(msg, 1024);
    //     fgets(msg, 1024, stdin);
    //     msg[strcspn(msg, "\n")] = 0;
    //     login["task_name"] = msg;
    //     std::cout << "Please enter the start time of task(such as 2020-06-01 00:00:00): ";
    //     bzero(msg, 1024);
    //     fgets(msg, 1024, stdin);
    //     msg[strcspn(msg, "\n")] = 0;
    //     login["start_time"] = msg;
    //     std::cout << "Please enter the total number of task: ";
    //     bzero(msg, 1024);
    //     fgets(msg, 1024, stdin);
    //     msg[strcspn(msg, "\n")] = 0;
    //     login["Total_number"] = std::stoi(msg);
    // }
    else {
        setbuf(stdin, NULL);
        return;
    }
    // head.length = strlen(login.toStyledString().c_str());
    // std::cout << login.toStyledString().c_str() << std::endl;
    // bufferevent *bev = (bufferevent *)arg;
    // bufferevent_write(bev, (char *)&head, sizeof(Head)); //消息的长度
    // bufferevent_write(bev, login.toStyledString().c_str(), head.length);
    setbuf(stdin, NULL);
    LogInfo("客户端发送消息成功");
    start_time = clock();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "参数不全：port \n";
        return 1;
    }
    int port = std::stoi(argv[1]);
    // int count = std::stoi(argv[2]);
    event_base *m_base = event_base_new();
    start_time = clock();
    for (int i = 0; i < COUNT; i++) {
        evutil_socket_t sockfd;
        struct sockaddr_in server_address;
        bzero(&server_address, sizeof(server_address));  //初始化结构体
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(port);
        server_address.sin_addr.s_addr = inet_addr("9.135.152.3");
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        int ret = connect(sockfd, (sockaddr *)&server_address, sizeof(server_address));
        assert(ret != -1);
        bufferevent *bev = bufferevent_socket_new(m_base, sockfd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, server_socket_read_cb, NULL, server_event_cb, m_base);
        bufferevent_enable(bev, EV_READ | EV_PERSIST);
        buf_vec.push_back(bev);
    }

    //监听终端输入事件
    struct event *ev_cmd = event_new(m_base, STDIN_FILENO, EV_READ | EV_PERSIST, cmd_msg_cb, NULL);
    event_add(ev_cmd, NULL);

    //输出提示
    printf("Please enter you order\n1:quiry\n2:scramble for tickets\n3:create new task\n");

    event_base_dispatch(m_base);
    //释放资源
    event_base_free(m_base);
    return 1;
}
