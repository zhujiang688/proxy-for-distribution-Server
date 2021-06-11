#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <event2/event.h>
#include<event2/bufferevent.h>
#include <vector>
#include <map>

#include "struture.h"

class process//工作进程
{
public:
    process():m_pid(-1){};
    

public:
    pid_t m_pid;
    int m_piped[2];//主进程与工作进程之间通信的管道
};

//进程池类
class processpool
{
private:
    processpool(int client_listen, int server_listen);//单例模式

public:
    static processpool* creat(int client_listen, int server_listen)
    {
        if(!m_instance)
        {
            m_instance = new processpool(client_listen, server_listen);
        }
        return m_instance;
    };
    static void run();//启动进程池
    ~processpool()
    {
        delete[]  m_sub_process;
    };

    //回调函数
    static void ListenerClientCallBack(int fd, short events, void* arg);
    static void ListenerServerCallBack(int fd, short events, void* arg);
    static void server_socket_read_cb(bufferevent* bev, void* arg);
    static void server_event_cb(struct bufferevent *bev, short event, void *arg);
    static void signal_cb(evutil_socket_t, short, void * );

    static void pipe_event_CallBack(int fd, short events, void* arg);
    static void client_socket_read_cb(bufferevent* bev, void* arg);
    static void client_event_cb(struct bufferevent *bev, short event, void *arg);
    static void work_server_socket_read_cb(bufferevent* bev, void* arg);
    static void work_server_event_cb(struct bufferevent *bev, short event, void *arg);

private:
    static void run_parent();
    static void run_child();

private:
    static const int USER_PER_PROCESS = 65536;//每个工作进程最多能处理的客户数量
    static const int MAX_EVENT_NUMBER = 10000;//epoll最多能处理的事件数量
    static const int m_process_number = 2;//当前进程池中的工作进程数量
    static int m_idx;//工作进程在进程池中的编号
    static event_base* m_base;//每个进程的event_base
    static int m_clistenfd;//客户端监听描述符，用于接收客户端连接
    static int m_slistenfd;//服务端监听描述符，用于接收服务端连接
    static process* m_sub_process;//描述所有工作进程的
    static processpool* m_instance;

    
    static std::map<int, bufferevent*>client_map;//记录客户连接
    static std::vector<std::pair<int,bufferevent*> >m_Serverlist;//与主进程连接的服务器
    //static ConsistentHashCircle* m_consistentHashCircle;//工作进程的哈希环
    //static std::map<int, PhysicalNode*>* m_physicalNodeMap;//工作进程连接的服务端，int是业务服务器对应的套接字
};

processpool* processpool::m_instance = NULL;
event_base* processpool::m_base = NULL;
std::map<int, bufferevent*> processpool::client_map;//记录客户连接
std::vector<std::pair<int,bufferevent*> > processpool::m_Serverlist;//与主进程连接的服务器
//ConsistentHashCircle* processpool::m_consistentHashCircle = NULL;//工作进程的哈希环
//std::map<int, PhysicalNode*>* processpool::m_physicalNodeMap = NULL;//工作进程连接的服务端，int是业务服务器对应的套接字
int processpool::m_idx = -1;
process* processpool::m_sub_process = NULL;
int processpool::m_clistenfd = -1;
int processpool::m_slistenfd = -1;


//进程池的初始化
processpool::processpool(int client_listen, int server_listen)
{
    m_clistenfd = client_listen;
    m_slistenfd = server_listen;
    m_sub_process = new process[m_process_number];

    //创建process_number个工作进程
    for(int i = 0;i<m_process_number;i++)
    {
        //建立每个工作进程与主进程的管道
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_piped);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0); 

        if(m_sub_process[i].m_pid > 0)//主进程
        {
            close(m_sub_process[i].m_piped[1]);
            continue;
        }
        else//工作进程
        {
            close(m_sub_process[i].m_piped[0]);
            m_idx = i;//工作进程需要标识自己是第几个工作进程
            break;//工作进程退出循环
        }
    }
};

void processpool::run()
{
    if(m_idx == -1)
    {
        run_parent();
        return;
    }
    run_child();
};

void processpool::run_child()//工作进程运行函数
{
    std::cout<<"工作进程开始运行\n";
    //添加工作进程的各种监听事件，回调函数
    //HashFunction* fun = new MD5HashFunction();
    //m_consistentHashCircle = new ConsistentHashCircle(fun);
    m_base = event_base_new();
    //m_physicalNodeMap = new std::map<int, PhysicalNode*>();

    //工作进程需要监听与主进程的通信端口
    struct event* listenPipeEvent = event_new(m_base, m_sub_process[m_idx].m_piped[1], 
                EV_READ|EV_PERSIST, 
                pipe_event_CallBack, m_base);
    if (!listenPipeEvent) {
        //LOG_FUNC_MSG("event_new()", errnoMap[errno]);
        return ;
    }
    event_add(listenPipeEvent, NULL);
    event_base_dispatch(m_base);//启动event 
    event_free(listenPipeEvent);
    event_base_free(m_base);
};

void processpool::pipe_event_CallBack(int fd, short event, void* arg)
{
    int message;
    int ret = recv(fd, (char*)&message, sizeof(message),0);//接收主线程传来的命令
    if((ret < 0 && (errno != EAGAIN )) || ret == 0)
    {
        return;
    }
    else
    {
        switch (message)
        {
        case 1://新客户端加入
        {
            std::cout<<"工作进程"<<m_idx<<"收到主进程传来的消息：新客户端加入\n";
            evutil_socket_t sockfd;
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            sockfd = ::accept(m_clistenfd, (struct sockaddr*)&client, &len );
            evutil_make_socket_nonblocking(sockfd);
            struct event_base* base = (event_base*)arg;
            bufferevent* bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
            bufferevent_setcb(bev, client_socket_read_cb, NULL, client_event_cb, NULL);
            bufferevent_enable(bev, EV_READ | EV_PERSIST);
            client_map.insert(std::map<int, bufferevent*>::value_type(sockfd, bev));

            break;
        }
        case 2://新服务端加入
        {
            std::cout<<"工作进程"<<m_idx<<"收到主进程传来的消息：新服务端加入\n";
            int length;
            const char* ip;
            int port;
            ret = recv(fd, (char*)&length, sizeof(length),0);//接收主线程传来的地址
            assert(ret == sizeof(length));
            address server_add;
            ret = recv(fd, (char*)&server_add, length,0);
            ip = server_add.ip;
            port = server_add.port;
            evutil_socket_t sockfd;
            sockaddr_in server_address;
            bzero(&server_address,sizeof(server_address));  //初始化结构体
            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(port);
            server_address.sin_addr.s_addr = inet_addr(ip);
            std::cout<<"ip: "<<ip<<"   port: "<<port<<std::endl;
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            ret = connect(sockfd, (sockaddr*)&server_address, sizeof(server_address));
            evutil_make_socket_nonblocking(sockfd);
            assert(ret != -1);
            struct event_base* base = (event_base*)arg;
            bufferevent* bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
            bufferevent_setcb(bev, work_server_socket_read_cb, NULL, work_server_event_cb, NULL);
            bufferevent_enable(bev, EV_READ | EV_PERSIST);
            m_Serverlist.push_back(std::pair<int, bufferevent*>(sockfd, bev));
            break;
        }
        case 3://某服务端不可用
            std::cout<<"工作进程"<<m_idx<<"收到主进程传来的消息：客户端不可用\n";
            break;
        case 4://终止
        {
            std::cout<<"工作进程"<<m_idx<<"收到主进程传来的消息：进程终止\n";
            //释放资源
            for(auto x:client_map) bufferevent_free(x.second);
            for(auto x:m_Serverlist) bufferevent_free(x.second);
            event_base_loopbreak(m_base);
            break;
        }
        default:
            break;
        }
    }
    
}

void processpool::client_socket_read_cb(bufferevent* bev, void* arg)
{
    //接收消息
    int length;
    int ret =  bufferevent_read(bev, (char*)&length, sizeof(length));
    char msg[length];
    ret = bufferevent_read(bev, msg, sizeof(msg));
    //选择服务器
    static int count = 0;
    count = count % m_Serverlist.size();
    int socketfd = bufferevent_getfd(bev);
    bufferevent_write(m_Serverlist[count].second, (char*)&socketfd, sizeof(socketfd));
    bufferevent_write(m_Serverlist[count].second, (char*)&length, sizeof(length));
    bufferevent_write(m_Serverlist[count].second, msg, sizeof(msg));
    count++;
    std::cout<<"工作进程"<<m_idx<<"收到客户端"<<socketfd<<"传来的消息"<<msg<<std::endl;
}

void processpool::client_event_cb(struct bufferevent *bev, short event, void *arg)
{
    std::cout<<"工作进程"<<m_idx<<"收到客户端传来的消息：客户退出\n";
    if (event & BEV_EVENT_EOF)
        printf("connection closed\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error\n");
    //这将自动close套接字和free读写缓冲区
    int sockfd = bufferevent_getfd(bev);
    bufferevent_free(bev);
    client_map.erase(client_map.find(sockfd));
}


void processpool::work_server_socket_read_cb(bufferevent* bev, void* arg)
{
    int sockfd;
    int ret =  bufferevent_read(bev, (char*)&sockfd, sizeof(sockfd));
    assert(ret == sizeof(sockfd));
    std::cout<<sockfd<<std::endl;
    int length;
    ret =  bufferevent_read(bev, (char*)&length, sizeof(length));
    assert(ret == sizeof(length));
    char msg[length];
    ret =  bufferevent_read(bev, msg, length);
    assert(ret == length);
    std::cout<<"read done"<<std::endl;
    assert(client_map.count(sockfd));
    bufferevent_write(client_map[sockfd], (char*)&length, sizeof(length));
    bufferevent_write(client_map[sockfd], msg, length);
    std::cout<<"工作进程"<<m_idx<<"收到服务端传来的消息"<<msg<<std::endl;
}

void processpool::work_server_event_cb(struct bufferevent *bev, short event, void *arg)
{
    std::cout<<"工作进程"<<m_idx<<"收到服务端传来的消息：服务端退出\n";
    if (event & BEV_EVENT_EOF)
        printf("server connection closed\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error in server\n");
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
    int ind = 0;
    for(ind;ind<m_Serverlist.size();ind++)
    {
        if(m_Serverlist[ind].first == bufferevent_getfd(bev))
        {
            break;
        }
    }
    swap(m_Serverlist[ind], m_Serverlist.back());
    m_Serverlist.pop_back();
}


void processpool::run_parent()//主线程的工作函数
{
    std::cout<<"主进程开始运行\n";
    //资源初始化
    m_base = event_base_new();
    evutil_make_socket_nonblocking(m_clistenfd);
    //添加监听客户端请求连接事件
    struct event* c_listen = event_new(m_base, m_clistenfd, EV_READ|EV_PERSIST|EV_ET,
                                        ListenerClientCallBack, m_base);
    event_add(c_listen, NULL);
    
    evutil_make_socket_nonblocking(m_slistenfd);
    //添加监听服务端请求连接事件
    struct event* s_listen = event_new(m_base, m_slistenfd, EV_READ|EV_PERSIST|EV_ET,
                                        ListenerServerCallBack, m_base);
    event_add(s_listen, NULL);


    //主线程中的信号处理 
    struct event* signal_SIGINT = event_new(m_base, SIGINT, EV_SIGNAL|EV_PERSIST|EV_ET, signal_cb, m_base);
    assert(event_add(signal_SIGINT, NULL) == 0);
    struct event* signal_SIGTERM = event_new(m_base, SIGTERM, EV_SIGNAL|EV_PERSIST|EV_ET, signal_cb, m_base);
    assert(event_add(signal_SIGTERM, NULL) == 0);

    //开启监听事件循环
    std::cout << "start event loop" << std::endl;
    event_base_dispatch(m_base);
    
    event_free(c_listen);
    event_free(s_listen);
    event_free(signal_SIGINT);
    event_free(signal_SIGTERM);
    //释放资源
    event_base_free(m_base);
};

void processpool::ListenerClientCallBack(int fd, short events, void* arg)
{
    //轮询通知的方法，选择一个工作进程通知接收新客户端
    static int count = 0;
    count = count % m_process_number;
    int message = 1;
    send(m_sub_process[count].m_piped[0], (char*)&message, sizeof(message),0);
    std::cout<<"主进程向工作进程"<<count<<"发送新客户端达到通知\n";
    count++;
}

void processpool::ListenerServerCallBack(int fd, short events, void* arg)
{
    printf("主进程监听到新服务端到达并连接\n");
    //建立与服务端的连接
    evutil_socket_t sockfd;
    struct sockaddr_in server;
    socklen_t len = sizeof(server);
    sockfd = ::accept(fd, (struct sockaddr*)&server, &len );
    evutil_make_socket_nonblocking(sockfd);
    printf("accept a server %d\n", sockfd);
    bufferevent* bev = bufferevent_socket_new(m_base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, server_socket_read_cb, NULL, server_event_cb, NULL);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);
    m_Serverlist.push_back(std::pair<int, bufferevent*>(sockfd, bev));
}

void processpool::server_socket_read_cb(bufferevent* bev, void* arg)
{
    printf("服务端向主进程传递服务端地址信息\n");
    //收取服务端传来的监听地址
    int length;
    int len = bufferevent_read(bev, (char*)&length, sizeof(length));
    assert(len == sizeof(length));
    char msg[length];
    len = bufferevent_read(bev, msg, length);
    assert(len == length);

    //通知工作进程新的服务端接入
    int message = 2;
    for(int i = 0;i<m_process_number;i++)
    {
        send(m_sub_process[i].m_piped[0], (char*)&message, sizeof(message),0);
        send(m_sub_process[i].m_piped[0], (char*)&length, sizeof(length),0);
        send(m_sub_process[i].m_piped[0], msg, length,0);
    }
    printf("主进程通知工作进程新的服务端地址\n");
}

//回调函数
//服务器断开连接的时候调用该函数
void processpool::server_event_cb(struct bufferevent *bev, short event, void *arg)
{
    printf("主进程服务端断开\n");
    if (event & BEV_EVENT_EOF)
        printf("server connection closed\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error in server\n");
    int ind = 0;
    for(ind;ind<m_Serverlist.size();ind++)
    {
        if(m_Serverlist[ind].first == bufferevent_getfd(bev))
        {
            break;
        }
    }
    swap(m_Serverlist[ind], m_Serverlist.back());
    m_Serverlist.pop_back();
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
}

void processpool::signal_cb(evutil_socket_t sig, short events, void * arg)
{
    printf("主进程收到信号\n");
    //通知工作进程
    int message = 4;
    for(int i = 0;i<m_process_number;i++)
    {
        send(m_sub_process[i].m_piped[0], (char*)&message, sizeof(message),0);
    }
    //释放资源
    for(auto x:m_Serverlist) bufferevent_free(x.second);
    //关闭
    event_base_loopexit(m_base, NULL);
}


#endif

