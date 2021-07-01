#ifndef _SERVER_H_
#define _SERVER_H_

#include <string.h>
#include "threadPool.h"
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <assert.h>
#include <json/json.h>
#include <signal.h>


//业务服务器的主线程
class Server {
   public:
    Server(unsigned short port, int threadCnt);
    ~Server();

   private:
    struct event_base* m_base;  //主线程的event_base
    ThreadPool* m_pool;         //线程池
    int m_listenFd;             //服务器的监听套接字

    friend void listenClientConnectEventCallBack(int fd, short event, void *arg);
    friend void signal_cb(evutil_socket_t sig, short events, void *arg);
    // friend void listenSubThreadSocketPairFirstIOEventCallBack(int fd, short event, void *arg);
};

#endif