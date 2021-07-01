#include "threadPool.h"


//初始化工作线程
ThreadPool::ThreadPool(int threadCnt) : m_threadCnt(threadCnt) {
  for (int i=0; i<m_threadCnt; ++i) {
    m_pool.push_back(new Thread());
  }
}

ThreadPool::~ThreadPool() {
  for (int i=m_pool.size()-1; i>=0; i--) {
    delete m_pool[i];
    m_pool.pop_back();
  }
}

//从所有工作线程中轮询获取一个工作线程用于接收新的客户端
int ThreadPool::getSubThreadSocketPairFd() {
  static int robin = 0;//该进程池当前已经连接了多少客户端
  //robin ++ % m_threadCnt是通过轮询的方式决定由哪个工作线程接收新的客户端
  int x = m_pool[robin ++ % m_threadCnt]->getSocketPairFdFirst();//获取到某个工作线程的信号描述符
  return x;
}

void ThreadPool::notice_all_thread(int sig)
{
  LogInfo("线程池分发结束信号");
  for(int i =0;i<m_threadCnt;i++)
  {
    write(m_pool[i]->getSocketPairFdFirst(), (char*)&sig, sizeof(int));
  }
}