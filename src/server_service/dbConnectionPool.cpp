#include "connectionPool.h"

bool ConnectionPool::loadConfigFile()
{
  FILE *pf = fopen(MYSQL_CONFIG_PATH, "r");
  if (nullptr == pf)
  {
    //LOG_MSG("mysql.cfg file is not exist!");
    return false;
  }

  while (!feof(pf))
  {
    char buf[BUF_LEN] = {0};
    fgets(buf, BUF_LEN, pf);
    std::string str = buf;
    int idx = str.find('=', 0);
    if (-1 == idx)
    {
      continue;
    }

    int endIdx = str.find('\n', idx);
    std::string key = str.substr(0, idx);
    std::string value = str.substr(idx + 1, endIdx - idx - 1);
    if (key == "ip")
    {
      m_ip = value;
    }
    else if (key == "port")
    {
      m_port = atoi(value.c_str());
    }
    else if (key == "dbName")
    {
      m_dbName = value;
    }
    else if (key == "password")
    {
      m_password = value;
    }
    else if (key == "userName")
    {
      m_userName = value;
    }
    else if (key == "initSize")
    {
      m_initSize = atoi(value.c_str());
    }
    else if (key == "maxIdleTime")
    {
      m_maxIdleTime = atoi(value.c_str());
    }
    else if (key == "maxSize")
    {
      m_maxSize = atoi(value.c_str());
    }
    else if (key == "connectionTimeOut")
    {
      m_connectionTimeOut = atoi(value.c_str());
    }
  }
  fclose(pf);
  return true;
}

ConnectionPool::ConnectionPool()
{
  if (!loadConfigFile())
  { //加载配置
    LogError("配置加载失败");
    return;
  }

  for (int i = 0; i < m_initSize; ++i)
  { //建立多个数据库连接
    Connection *p = new Connection();
    p->connect(m_ip, m_port, m_userName, m_password, m_dbName);
    if (p == nullptr)
    {
      LogWarn("连接失败");
      continue;
    }
    p->refreshAliveTime();
    m_connectionQueue.push(p);
    m_connectionCnt++;
  }

  //创建按一个线程用于生产连接
  std::thread producer(std::bind(&ConnectionPool::produceConnectionTask, this));
  producer.detach();
  //创建一个线程用于
  std::thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
  scanner.detach();
  LogInfo("数据库连接池启动完毕");
}

//
void ConnectionPool::produceConnectionTask()
{
  for (;;)
  { //无限循环
    std::unique_lock<std::mutex> lock(m_queueMutex);
    while (!m_connectionQueue.empty())
    {                  //如果队列是空的，直接跳过
      m_cv.wait(lock); //队列不为空，一直锁再这里
    }
    //队列为空跳出
    if (m_connectionCnt < m_maxSize)
    { //连接数量没有超过设定的最大数量，可以新增连接
      Connection *p = new Connection();
      p->connect(m_ip, m_port, m_userName, m_password, m_dbName);
      if (p == nullptr)
      {
        LogWarn("连接失败");
        continue;
      }
      p->refreshAliveTime();
      m_connectionQueue.push(p);
      m_connectionCnt++;
    }
    m_cv.notify_all();
  }
}

std::shared_ptr<Connection> ConnectionPool::getConnection()
{
  std::unique_lock<std::mutex> lock(m_queueMutex);
  while (m_connectionQueue.empty())
  {
    if (std::cv_status::timeout == m_cv.wait_for(lock, std::chrono::milliseconds(m_connectionTimeOut)))
    {
      if (m_connectionQueue.empty())
      {
        //LOG_MSG("connection timeout get connection fail!");
        return nullptr;
      }
    }
  }

  std::shared_ptr<Connection> sp(m_connectionQueue.front(),
                                 [&](Connection *pconn) { //后面这个函数应该是释放智能指针的操作
                                   std::unique_lock<std::mutex> lock(m_queueMutex);
                                   pconn->refreshAliveTime();
                                   m_connectionQueue.push(pconn);
                                   m_connectionCnt++;
                                 });
  m_connectionQueue.pop(); //将被分配的连接从队列出取出
  m_connectionCnt--;
  m_cv.notify_all(); //唤醒连接生产线程
  LogInfo("获取数据库连接成功");
  return sp;
}

void ConnectionPool::scannerConnectionTask()
{
  for (;;)
  {
    std::this_thread::sleep_for(std::chrono::seconds(m_maxIdleTime));

    std::unique_lock<std::mutex> lock(m_queueMutex);
    while (m_connectionCnt > m_initSize)
    { //现有连接数量大于初始连接数量
      Connection *p = m_connectionQueue.front();
      if (p->getAliveTime() >= (m_maxIdleTime * 1000))
      { //队列头部的连接的上次活跃时间过大，即很久没有用
        m_connectionQueue.pop();
        m_connectionCnt--;
        delete p;
      }
      else
      {
        break;
      }
    }
  }
}