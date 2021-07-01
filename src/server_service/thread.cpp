#include "thread.h"

// void clientIOEventCallBack(int fd, short event, void *arg) {
//   Thread* thread = static_cast<Thread*>(arg);
//   char buffer[1024] = {0};
//   if (recv(fd, buffer, 1023, 0) <= 0) {
//     std::cout << "client disconnected!" << std::endl;
//     close(fd);
//     return;
//   }
//   std::cout << "============ client data ============" << std::endl;
//   std::string msg(buffer);
//   Json::Value value;
//   Json::Reader reader;//用于将字字符串转换为value对象

//   if (!reader.parse(msg, value)) {//解码
//     LOG_MSG("json reader parse error!");
//     return;
//   }
//   thread->m_controller->process(fd, value);//处理该客户端传来的任务
// }

void work_socket_read_cb(bufferevent *bev, void *arg)
{
  while (true)
  {
    //检查是否够一个头部的长度
    if (evbuffer_get_length(bufferevent_get_input(bev)) < sizeof(Head))
    {
      LogWarn("不足头部长度");
      return;
    }
    Head head;
    //复制出头部
    assert(evbuffer_copyout(bufferevent_get_input(bev), (void *)&head, sizeof(head)) != -1);
    //std::cout << head.length << std::endl;
    //检查该包是否完整到达
    if (evbuffer_get_length(bufferevent_get_input(bev)) < sizeof(Head) + head.length)
    {
      LogWarn("不足包体长度");
      return;
    }
    //取出一个包
    bufferevent_read(bev, (char *)&head, sizeof(head));
    char buffer[1024] = {0};
    bufferevent_read(bev, buffer, head.length);
    std::string msg(buffer);
    Json::Value client_rep;
    Json::Reader reader; //用于将字字符串转换为value对象
    if (!reader.parse(msg, client_rep))
    { //解码
      LogWarn("json reader parse error!");
      return;
    }
    //可以对客户端的请求做一些处理
    Thread* thread = static_cast<Thread*>(arg);
    thread->m_controller->process(bev, client_rep);//处理该客户端传来的任务
    LogInfo("服务器线程：%ld 收到报文 %s", pthread_self(), msg.c_str());
  }
}

void work_event_cb(struct bufferevent *bev, short event, void *arg)
{
  if (event & BEV_EVENT_EOF) {
    LogWarn("工作进程连接断开");
  }
  else if (event & BEV_EVENT_ERROR) {
    LogWarn("some other error in server");
  }
  //这将自动close套接字和free读写缓冲区
  bufferevent_free(bev);
  //event_base_loopexit((event_base *)arg, NULL);
}

void socketPairIOEventCallBack(int fd, short event, void *arg)
{ //管道IO事件
  Thread *thread = static_cast<Thread *>(arg);
  int cfd = 0; //这应该是主线程写入的新客户端的套接字
  assert(recv(fd, (char *)&cfd, 4, 0) >= 0);
  //准备添加新的客户端
  if(cfd == -1)
  {
    LogInfo("工作线程接入收到结束信号");
    event_base_loopexit(thread->m_base, NULL);
    return;
  }
  evutil_make_socket_nonblocking(cfd);
  bufferevent *bev = bufferevent_socket_new(thread->m_base, cfd, BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(bev, work_socket_read_cb, NULL, work_event_cb, thread);
  bufferevent_enable(bev, EV_READ | EV_PERSIST);
  LogInfo("工作线程接入sock %d", cfd);
}

void *threadTaskFunction(void *arg)
{ //初始化线程的函数
  LogInfo("创建新线程成功");
  Thread *thread = static_cast<Thread *>(arg);
  thread->m_id = pthread_self();
  thread->m_base = event_init(); //新建该线程的event_base
  //监听从主线程传来的信号
  evutil_make_socket_nonblocking(thread->m_socketpairFd[1]);
  struct event *socketPairIOEvent = event_new(thread->m_base, thread->m_socketpairFd[1], EV_PERSIST | EV_READ, socketPairIOEventCallBack, thread);
  assert(socketPairIOEvent);
  event_add(socketPairIOEvent, nullptr);
  event_base_dispatch(thread->m_base);
  event_base_free(thread->m_base);
  LogInfo("工作线程退出");
  pthread_exit(NULL);
}

Thread::Thread()
{
  m_controller = new Controller();
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, m_socketpairFd) != -1);
  int res = pthread_create(&m_id, nullptr, threadTaskFunction, this); //创建新线程
  assert(res == 0);
}

Thread::~Thread()
{
  //event_base_free(m_base);
  //delete m_controller;
}

int Thread::getSocketPairFdFirst()
{
  return m_socketpairFd[0]; //主线程从这里写
}

int Thread::getSocketPairFdSecond()
{
  return m_socketpairFd[1];
}

pthread_t Thread::getId()
{
  return m_id;
}
