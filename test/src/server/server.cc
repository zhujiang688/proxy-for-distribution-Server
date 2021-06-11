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
#include<event2/bufferevent.h>

#include "struture.h"

#define LOAD_SERVER_IP "9.135.152.3"
#define LOAD_SERVER_PORT 20000
#define SERVER_IP "9.135.152.3"
#define SERVER_PORT 15000

void load_socket_read_cb(bufferevent* bev, void* arg)
{
    int length;
    bufferevent_read(bev, (char*)&length, sizeof(length));
    char msg[length];
    bufferevent_read(bev, msg, sizeof(msg));
    std::cout<<"负载均衡服务器主进程返回的信息："<<msg<<std::endl;
}

void load_event_cb(struct bufferevent *bev, short event, void *arg)
{
    if (event & BEV_EVENT_EOF)
        printf("负载均衡服务器主进程连接断开\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error in server\n");
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
    event_base_loopexit((event_base*)arg, NULL);
}

void work_socket_read_cb(bufferevent* bev, void* arg)
{
    int socket;
    bufferevent_read(bev, (char*)&socket, sizeof(socket));
    int length;
    bufferevent_read(bev, (char*)&length, sizeof(length));
    char msg[length];
    bufferevent_read(bev, msg, sizeof(msg));
    bufferevent_write(bev, (char*)&socket, sizeof(socket));
    bufferevent_write(bev, (char*)&length, sizeof(length));
    bufferevent_write(bev, msg, sizeof(msg));
    std::cout<<"工作进程传入信息："<<socket<<"  "<<msg<<std::endl;
}

void work_event_cb(struct bufferevent *bev, short event, void *arg)
{
    if (event & BEV_EVENT_EOF)
        printf("工作进程连接断开\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error in server\n");
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
}

void ListenerServerCallBack(int fd, short events, void* arg)  
{  
    std::cout<<"工作进程接入"<<std::endl;
    event_base* base = (event_base*)arg;
    evutil_socket_t sockfd;
    struct sockaddr_in work;
    socklen_t len = sizeof(work);
    sockfd = ::accept(fd, (struct sockaddr*)&work, &len );
    evutil_make_socket_nonblocking(sockfd);
    bufferevent* bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, work_socket_read_cb, NULL, work_event_cb, &sockfd);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);
}  

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        std::cout<<"请输入监听PORT\n";
        return 1;
    }
    evutil_socket_t sockfd_C;
    evutil_socket_t sockfd_S;
    struct sockaddr_in Loadserver_address;
    bzero(&Loadserver_address,sizeof(Loadserver_address));  //初始化结构体
    Loadserver_address.sin_family = AF_INET;
    Loadserver_address.sin_port = htons(LOAD_SERVER_PORT);
    Loadserver_address.sin_addr.s_addr = inet_addr(LOAD_SERVER_IP);
    sockfd_C = socket(AF_INET, SOCK_STREAM, 0);
    int ret = connect(sockfd_C, (sockaddr*)&Loadserver_address, sizeof(Loadserver_address));//连接负载均衡服务器
    assert(ret != -1);

    //建立服务端监听事件
    sockfd_S = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd_S != -1);
    struct sockaddr_in s_sin;
    s_sin.sin_family = AF_INET;
    s_sin.sin_addr.s_addr = 0;
    //s_sin.sin_port = htons(SERVER_PORT);
    int port = std::stoi(argv[1]);
    s_sin.sin_port = htons(port);
    ret = bind(sockfd_S, (sockaddr*)&s_sin, sizeof(s_sin));
    assert(ret != -1);
    ret = listen(sockfd_S, 100);
    evutil_make_socket_nonblocking(sockfd_S);
    assert(ret != -1);
    event_base* m_base = event_base_new();
    //添加监听客户端请求连接事件
    struct event* s_listen = event_new(m_base, sockfd_S, EV_READ | EV_PERSIST,
                                        ListenerServerCallBack, m_base);
    event_add(s_listen, NULL);

    
    bufferevent* bev = bufferevent_socket_new(m_base, sockfd_C, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, load_socket_read_cb, NULL, load_event_cb, m_base);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);

    //服务器向负载均衡服务器回传监听端口
    address server_add;
    server_add.port = port;
    strcpy(server_add.ip, SERVER_IP);
    int length = sizeof(server_add);
    bufferevent_write(bev, (char*)&length, sizeof(length));
    bufferevent_write(bev, (char*)&server_add, sizeof(server_add));
    
    event_base_dispatch(m_base);
    //释放资源
    event_base_free(m_base);
    return 1;
}

