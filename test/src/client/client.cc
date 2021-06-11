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

void server_socket_read_cb(bufferevent* bev, void* arg)
{
    int length;
    bufferevent_read(bev, (char*)&length, sizeof(length));
    char msg[length];
    bufferevent_read(bev, msg, length);
    std::cout<<"服务器返回信息："<<msg<<std::endl;
}

void server_event_cb(struct bufferevent *bev, short event, void *arg)
{
    if (event & BEV_EVENT_EOF)
        printf("server connection closed\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error in server\n");
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
    event_base_loopexit((event_base*)arg, NULL);
}


void cmd_msg_cb(int fd, short events, void* arg)  
{  
    char msg[1024];  
  
    int ret = read(fd, msg, sizeof(msg));  
    if( ret < 0 )  
    {  
        perror("read fail ");  
        exit(1);  
    }  
  
    struct bufferevent* bev = (struct bufferevent*)arg;  
  
    //把终端的消息发送给服务器端
    bufferevent_write(bev, (char*)&ret, sizeof(ret));//消息的长度  
    bufferevent_write(bev, msg, ret);  
}  


int main()
{
   
    evutil_socket_t sockfd;
    struct sockaddr_in server_address;
    bzero(&server_address,sizeof(server_address));  //初始化结构体
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(10000);
    server_address.sin_addr.s_addr = inet_addr("9.135.152.3");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int ret = connect(sockfd, (sockaddr*)&server_address, sizeof(server_address));
    assert(ret != -1);

    event_base* m_base = event_base_new();
    bufferevent* bev = bufferevent_socket_new(m_base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, server_socket_read_cb, NULL, server_event_cb, m_base);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);
    //监听终端输入事件  
    struct event* ev_cmd = event_new(m_base, STDIN_FILENO,  
                                      EV_READ | EV_PERSIST, cmd_msg_cb,  
                                      (void*)bev);  
    event_add(ev_cmd, NULL);  
    event_base_dispatch(m_base);
    //释放资源
    event_base_free(m_base);
    return 1;
}

