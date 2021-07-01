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
#include <json/json.h>
#include <fstream>
#include "structure.h"
#include "mini_log.h"

using namespace std;

void load_socket_read_cb(bufferevent* bev, void* arg) {
    int length;
    bufferevent_read(bev, (char*)&length, sizeof(length));
    char msg[length];
    bufferevent_read(bev, msg, sizeof(msg));
    LogInfo("网关服务器主进程返回的信息：%s", msg);
    bufferevent_free(bev);
}

void load_event_cb(struct bufferevent* bev, short event, void* arg) {
    if (event & BEV_EVENT_EOF) {
      LogInfo("网关服务器主进程连接断开");
    }
    else if (event & BEV_EVENT_ERROR) {
      LogWarn("some other error in server");
    }
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
    event_base_loopexit((event_base*)arg, NULL);
}

int main(int argc, char* argv[]) {
    // 加载proxy配置文件
    ifstream ifs("./server_configurations.json", ios::binary);
    if (!ifs.is_open()) {
        LogError("open json file failed.");
        return -1;
    }

    Json::Reader reader;
    Json::Value root;
    int proxy_port;
    string proxy_ip;
    string local_ip;

    // 解析json文件
    if (reader.parse(ifs, root)) {
        proxy_ip = root["proxy_ip"].asString();
        local_ip = root["local_ip_for_service"].asString();
        proxy_port = root["proxy_port"].asInt();
    }
    ifs.close();
    if (!((bool)proxy_ip.c_str() && (bool)local_ip.c_str() && (bool)proxy_port)) {
        LogError("配置文件中相关参数缺失或格式错误");
        return -1;
    }

    event_base* m_base = event_base_new();
    evutil_socket_t sockfd_C;
    struct sockaddr_in Loadserver_address;
    bzero(&Loadserver_address, sizeof(Loadserver_address));  //初始化结构体
    Loadserver_address.sin_family = AF_INET;
    Loadserver_address.sin_port = htons(proxy_port);
    Loadserver_address.sin_addr.s_addr = inet_addr(proxy_ip.c_str());
    sockfd_C = socket(AF_INET, SOCK_STREAM, 0);
    int ret = connect(sockfd_C, (sockaddr*)&Loadserver_address,
                      sizeof(Loadserver_address));  //连接网关服务器
    if (-1 != ret) {
        LogInfo("已连接网关服务器，开始注册/注销");
    }

    bufferevent* bev = bufferevent_socket_new(m_base, sockfd_C, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, load_socket_read_cb, NULL, load_event_cb, m_base);
    bufferevent_enable(bev, EV_READ);

    int select = 0;
    int port = 0;

    cout << "请选择向网关服务器注册或注销:" << endl;
    cout << "1、注册" << endl;
    cout << "2、注销" << endl;
    cout << "3、关闭服务器" << endl;
    cin >> select;
    if (select == 1) {
        //向网关服务器注册
        cout << "请输入注册ip：" << endl;
        cin >> local_ip;
        cout << "请输入注册port：" << endl;
        cin >> port;
        address server_add;
        strcpy(server_add.token, "login");
        server_add.port = port;
        int length = sizeof(server_add);
        strncpy(server_add.ip, local_ip.c_str(), strlen(local_ip.c_str()));
        // strcpy 可能存在被拷贝对象内存不足的问题
        bufferevent_write(bev, (char*)&length, sizeof(length));
        bufferevent_write(bev, (char*)&server_add, sizeof(server_add));
    } else if (select == 2) {
        //向网关服务器注销
        cout << "请输入注销ip：" << endl;
        cin >> local_ip;
        cout << "请输入注销port：" << endl;
        cin >> port;
        address server_add;
        strcpy(server_add.token, "logout");
        server_add.port = port;
        strcpy(server_add.ip, local_ip.c_str());
        int length = sizeof(server_add);
        bufferevent_write(bev, (char*)&length, sizeof(length));
        bufferevent_write(bev, (char*)&server_add, sizeof(server_add));
    } else {
        cout << "输入有误，请重新输入" << endl;
    }

    //开始监听事件
    event_base_dispatch(m_base);
    //释放资源
    event_base_free(m_base);
    return 0;
}
