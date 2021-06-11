#include <iostream>
#include "pool.h"
using namespace std;

static const int client_port = 10000;
static const int server_port = 20000;

int main() {
  //建立客户端监听事件
  int m_clistenfd = socket(AF_INET, SOCK_STREAM, 0);
  assert(m_clistenfd != -1);

  struct sockaddr_in c_sin;
  c_sin.sin_family = AF_INET;
  c_sin.sin_addr.s_addr = 0;
  c_sin.sin_port = htons(client_port);


  int ret = bind(m_clistenfd, (sockaddr*)&c_sin, sizeof(c_sin));
  assert(ret != -1);
  ret = listen(m_clistenfd, 100);
  assert(ret != -1);

  //建立服务端监听事件
  int m_slistenfd = socket(AF_INET, SOCK_STREAM, 0);
  assert(m_slistenfd != -1);

  struct sockaddr_in s_sin;
  s_sin.sin_family = AF_INET;
  s_sin.sin_addr.s_addr = 0;
  s_sin.sin_port = htons(server_port);


  ret = bind(m_slistenfd, (sockaddr*)&s_sin, sizeof(s_sin));
  assert(ret != -1);
  ret = listen(m_slistenfd, 100);
  assert(ret != -1);

  
  processpool* pool = processpool::creat(m_clistenfd, m_slistenfd);
  if(pool)
  {
      pool->run();
      delete pool;
  }
  return 0;
}