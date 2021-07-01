#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include "thread.h"
//#include "include/dbConnectionPool/connectionPool.h"
#include <vector>

class ThreadPool {
public:
  ThreadPool(int threadCnt);
  ~ThreadPool();
  int getSubThreadSocketPairFd();//从工作线程中选择一个出来，取出其信号管道的文件描述符
  void notice_all_thread(int sig);
private:
  int m_threadCnt;
  std::vector<Thread*> m_pool;
};



#endif