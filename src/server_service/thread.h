/*
 * @Description:
 * @Language:
 * @Author:
 * @Date: 2021-03-26 01:43:34
 */
#ifndef _THREAD_H_
#define _THREAD_H_

#include "controller.h"
#include <unistd.h>
#include <json/json.h>
#include <pthread.h>
#include <sys/types.h>
#include <map>
#include <event.h>
#include <iostream>
#include <assert.h>

#include "structure.h"
#include "mini_log.h"


//每个线程具体可以实现的功能是，接收主线程传来的新客户端的套接字，然后该线程将该客户端的IO事件注册到该线程自己的
// event_base上，进行监听
class Thread {
   private:
    pthread_t m_id;             //线程编号
    int m_socketpairFd[2];      //管道，接收主线程信号
    struct event_base* m_base;  //每个线程的event_base
    Controller* m_controller;   //工作线程的核心，决定怎么处理客户端发来的消息
    // friend void clientIOEventCallBack(int fd, short event, void *arg);//客户端IO事件的回调函数
    friend void socketPairIOEventCallBack(int fd, short event, void* arg);  //管道事件的IO函数
    friend void* threadTaskFunction(void* arg);
    friend void work_socket_read_cb(bufferevent* bev, void* arg);                //工作进程IO事件的回调函数
    friend void work_event_cb(struct bufferevent* bev, short event, void* arg);  //工作进程退出事件

   public:
    Thread();
    ~Thread();
    int getEventMapSize() const;
    int getSocketPairFdFirst();
    int getSocketPairFdSecond();
    pthread_t getId();
};

#endif