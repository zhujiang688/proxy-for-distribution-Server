#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//主线程的监听端口由新客户加入时，执行该函数
void listenClientConnectEventCallBack(int fd, short event, void *arg)
{ //arg是一个serve指针
  //LOG_FUNC_TRACE();
  Server *ser = static_cast<Server *>(arg);
  struct sockaddr_in cli;
  socklen_t len;
  int cfd = accept(fd, (struct sockaddr *)&cli, &len);
  assert(cfd != -1);
  //将刚接入的客户端套接字转发给工作线程
  write(ser->m_pool->getSubThreadSocketPairFd(), (char *)&cfd, sizeof(cfd));
  LogInfo("主线程转发套接字端口");
}

void signal_cb(evutil_socket_t sig, short events, void *arg)
{
    LogInfo("主线程收到信号");
    Server* ser = (Server*)arg;
    //通知工作进程
    ser->m_pool->notice_all_thread(-1);
    //关闭
    event_base_loopexit(ser->m_base, NULL);
}

Server::Server(unsigned short port, int threadCnt)
{
  //LOG_FUNC_TRACE();
  //初始化业务服务器的地址与端口，监听描述符
  m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
  assert(m_listenFd != -1);
  struct sockaddr_in ser;
  ser.sin_family = AF_INET;
  ser.sin_addr.s_addr = 0;
  ser.sin_port = htons(port);
  int res = bind(m_listenFd, (struct sockaddr *)&ser, sizeof(ser));
  assert(res != -1);
  res = listen(m_listenFd, 100);
  assert(res != -1);
  evutil_make_socket_nonblocking(m_listenFd);

  //建立主线程的event_base
  m_base = event_base_new();
  m_pool = new ThreadPool(threadCnt);

  //添加主线程的端口监听事件
  struct event *listenClinetConnectEvent = event_new(m_base, m_listenFd, EV_READ | EV_PERSIST, listenClientConnectEventCallBack, this);
  assert(listenClinetConnectEvent);

  event_add(listenClinetConnectEvent, nullptr);

  //主线程中的信号处理
  struct event *signal_SIGINT = event_new(m_base, SIGINT, EV_SIGNAL | EV_PERSIST , signal_cb, this);
  assert(event_add(signal_SIGINT, NULL) == 0);
  struct event *signal_SIGTERM = event_new(m_base, SIGTERM, EV_SIGNAL | EV_PERSIST , signal_cb, this);
  assert(event_add(signal_SIGTERM, NULL) == 0);

  event_base_dispatch(m_base);
  sleep(1);
  event_base_free(m_base);
  close(m_listenFd);
}

Server::~Server()
{
  delete m_pool;
}

