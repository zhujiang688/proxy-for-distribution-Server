#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>
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
#include <string>
#include <json/json.h>
#include <fstream>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <vector>
#include <map>
#include <set>

#include "server_monitor.h"
#include "memorypool.h"
#include "http_helper.h"
#include "mini_log.h"

using namespace std;

//进程池类
class processpool {
   private:
    processpool(int client_listen, int server_listen,
                int process_number);  //单例模式

   public:
    static processpool *creat(int client_listen, int server_listen, int process_number) {
        if (!m_instance) {
            m_instance = new processpool(client_listen, server_listen, process_number);
        }
        return m_instance;
    };
    static void run();  //启动进程池
    ~processpool() { delete[] m_sub_process; };

    //回调函数
    static void ListenerClientCallBack(int fd, short events, void *arg);
    static void ListenerServerCallBack(int fd, short events, void *arg);
    static void server_socket_read_cb(bufferevent *bev, void *arg);
    static void server_event_cb(struct bufferevent *bev, short event, void *arg);
    static void signal_cb(evutil_socket_t, short, void *);

    static void pipe_event_CallBack(int fd, short events, void *arg);
    static void client_socket_read_cb(bufferevent *bev, void *arg);
    static void client_event_cb(struct bufferevent *bev, short event, void *arg);
    static void work_server_socket_read_cb(bufferevent *bev, void *arg);
    static void work_server_event_cb(struct bufferevent *bev, short event, void *arg);
    static void empty_heimingdan_cb(int fd, short events, void *arg);
    static void empty_count_cb(int fd, short events, void *arg);
    //超时回调函数
    static void check_time_out_cb(int fd, short events, void *arg);
    //限流函数
    static bool current_limit(unsigned int sockfd);
    static bool verify_client_ip(std::string client_ip);

   private:
    static void run_parent();
    static void run_child();

   private:
    static const int USER_PER_PROCESS = 65536;  //每个工作进程最多能处理的客户数量
    static const int MAX_EVENT_NUMBER = 10000;  // epoll最多能处理的事件数量
    static int m_idx;                           //工作进程在进程池中的编号
    static event_base *m_base;                  //每个进程的event_base
    static int m_clistenfd;                     //客户端监听描述符，用于接收客户端连接
    static int m_slistenfd;                     //服务端监听描述符，用于接收服务端连接

    static processpool *m_instance;

    static std::map<unsigned int, client_information *> client_map;  //记录客户连接
    static unsigned int client_id;
    static int shm_id;         //共享内存文件描述符
    static void *shm_address;  //共享内存首地址
    static int msgid;          //消息队列描述符
    // static std::set<std::string> hei_ming_dan;                      //用户黑名单
    static std::vector<std::pair<int, bufferevent *>> m_Serverlist;  //与主进程连接的服务器
    static std::vector<pair<bufferevent *, string>> m_server_info;   //记录与socket对应的服务器ip和port
    static cli_event *msg_queue[MSG_QUEUE_LEN];
    static int msg_queue_front;
    static int msg_queue_back;
    static cli_event *fin_queue[MSG_QUEUE_LEN];
    static int fin_queue_front;
    static int fin_queue_back;
    static MemoryPool *mp;
    static std::map<unsigned, int> client_http_code;  // set code for client by its http msg

   public:
    static process *m_sub_process;  //工作进程集合
    static int m_process_number;    //当前进程池中的工作进程数量
};

processpool *processpool::m_instance = NULL;
event_base *processpool::m_base = NULL;
std::map<unsigned int, client_information *> processpool::client_map;  //记录客户连接
std::vector<std::pair<int, bufferevent *>> processpool::m_Serverlist;  //与主进程连接的服务器
std::vector<std::pair<bufferevent *, string>> processpool::m_server_info;
int processpool::m_idx = -1;
process *processpool::m_sub_process = NULL;
int processpool::m_clistenfd = -1;
int processpool::m_slistenfd = -1;
int processpool::m_process_number = 8;
unsigned int processpool::client_id = 0;
int processpool::shm_id = 0;                            //共享内存文件描述符
void *processpool::shm_address = 0;                     //共享内存首地址
int processpool::msgid = 0;                             //消息队列描述符
std::map<unsigned, int> processpool::client_http_code;  // set code for client by its http msg

//初始化缓存空间
cli_event *processpool::msg_queue[MSG_QUEUE_LEN];
int processpool::msg_queue_front = 0;
int processpool::msg_queue_back = 0;
cli_event *processpool::fin_queue[MSG_QUEUE_LEN];
int processpool::fin_queue_front = 0;
int processpool::fin_queue_back = 0;
MemoryPool *processpool::mp = MemoryPoolInit(MAX_MEM, MEM_POOL_SIZE);

//进程池的初始化
processpool::processpool(int client_listen, int server_listen, int process_number) {
    m_clistenfd = client_listen;
    m_slistenfd = server_listen;
    m_process_number = process_number;

    //创建共享内存
    key_t key = ftok(HEI_MING_DAN_PATH, 66);
    assert(key >= 0);
    shm_id = shmget(key, sizeof(Hhei_Ming_Dan), IPC_CREAT | IPC_EXCL | 0666);
    assert(shm_id >= 0);
    shm_address = shmat(shm_id, NULL, 0);

    ((Hhei_Ming_Dan *)shm_address)->init_shm();  //初始化共享内存，初始化各种变量，各种锁

    m_sub_process = new process[m_process_number];  //创建process_number个工作进程
    for (int i = 0; i < m_process_number; i++) {
        //建立每个工作进程与主进程的管道
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_piped);
        if (0 != ret) {
            LogWarn("创建通信管道失败");
        }
        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);

        if (m_sub_process[i].m_pid > 0)  //主进程
        {
            close(m_sub_process[i].m_piped[1]);
            continue;
        } else  //工作进程
        {
            close(m_sub_process[i].m_piped[0]);
            m_idx = i;  //工作进程需要标识自己是第几个工作进程
            break;      //工作进程退出循环
        }
    }
};

void processpool::run() {
    if (m_idx == -1) {
        run_parent();
        return;
    }
    run_child();
};

void processpool::run_child()  //工作进程运行函数
{
    sleep(1);
    LogInfo("工作进程 %d 开始运行", m_idx);
    //添加工作进程的各种监听事件，回调函数
    // HashFunction* fun = new MD5HashFunction();
    // m_consistentHashCircle = new ConsistentHashCircle(fun);
    m_base = event_base_new();
    // m_physicalNodeMap = new std::map<int, PhysicalNode*>();

    //工作进程需要监听与主进程的通信端口
    struct event *listenPipeEvent =
        event_new(m_base, m_sub_process[m_idx].m_piped[1], EV_READ | EV_PERSIST, pipe_event_CallBack, m_base);
    if (!listenPipeEvent) {
        // LOG_FUNC_MSG("event_new()", errnoMap[errno]);
        return;
    }

    //创建消息队列描述符
    msgid = msgget(M_MSG_KEY, IPC_CREAT | 0);
    if (msgid == -1) {
        LogWarn("工作进程 %d 获取消息队列失败", m_idx);
        return;
    }

    // 初始化服务器列表
    LogInfo("工作进程 %d 初始化服务器列表中", m_idx);
    Json::Value server_up;
    Json::Value server_down;
    if (ServerMonitor::LoadServerList(server_up, server_down)) {
        LogError("load server_list failed");
        return;
    }
    for (auto it = server_up.begin(); it != server_up.end(); it++) {
        Json::Value server_up_ele = *it;
        char ip[20];
        strcpy(ip, server_up_ele["ip"].asString().c_str());
        int port = server_up_ele["port"].asInt();
        evutil_socket_t sockfd;
        sockaddr_in server_address;
        bzero(&server_address, sizeof(server_address));  //初始化结构体
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(port);
        server_address.sin_addr.s_addr = inet_addr(ip);
        LogInfo("ip: %s, port: %d", ip, port);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        int ret = connect(sockfd, (sockaddr *)&server_address, sizeof(server_address));
        evutil_make_socket_nonblocking(sockfd);
        if (-1 == ret) {
            LogWarn("工作进程 %d 连接服务器失败, ip: %s, port: %d", m_idx, ip, port);
            // 通知网关探测模块服务器故障
            message msg;
            msg.msg_type = 1;
            strcpy(msg.ip, ip);
            msg.port = port;
            int msgid = msgget(M_MSG_KEY, IPC_CREAT | 0);
            if (msgid == -1) {
                LogError("工作进程 %d 获取消息队列失败", m_idx);
                return;
            }
            int ret = msgsnd(msgid, &msg, sizeof(msg), 0);
            if (0 != ret) {
                LogError("工作进程 %d 通知被动检测模块失败", m_idx);
                return;
            } else {
                LogInfo("工作进程 %d 通知被动检测模块成功", m_idx);
            }
            continue;
        }

        bufferevent *bev = bufferevent_socket_new(m_base, sockfd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, work_server_socket_read_cb, NULL, work_server_event_cb, NULL);
        bufferevent_enable(bev, EV_READ | EV_PERSIST);

        m_Serverlist.push_back(std::pair<int, bufferevent *>(sockfd, bev));

        string ipPort = string(ip) + ':' + to_string(port);
        m_server_info.push_back(make_pair(bev, ipPort));
    }
    LogInfo("工作进程 %d 初始化服务器列表完成", m_idx);

    //定时任务,更新客户请求次数
    timeval beat2 = {1, 0};  //每十分钟清空一次
    event *empty_count = event_new(m_base, -1, EV_PERSIST | EV_TIMEOUT, empty_count_cb, NULL);
    event_add(empty_count, &beat2);

    //定时任务,处理超时事件
    timeval beat3 = {CB_TIMEOUT, 0};  //每10秒检查一次
    event *check_time_out = event_new(m_base, -1, EV_PERSIST | EV_TIMEOUT, check_time_out_cb, NULL);
    event_add(check_time_out, &beat3);

    event_add(listenPipeEvent, NULL);
    event_base_dispatch(m_base);  //启动event
    event_free(empty_count);
    event_base_free(m_base);
    shmdt(shm_address);
};

void processpool::empty_count_cb(int fd, short events, void *arg) {
    // std::cout<<"请求次数重置\n";
    for (std::map<unsigned int, client_information *>::iterator it = client_map.begin(); it != client_map.end(); it++) {
        if (it->second->close_flag) {
            bufferevent_free(it->second->bev);
            delete it->second;
            client_map.erase(it);
            LogInfo("断开客户端连接");
            continue;
        }
        it->second->count = 0;
    }
}

bool processpool::verify_client_ip(std::string client_ip) {
    char ip[20];
    bzero(ip, sizeof(ip));
    strcpy(ip, client_ip.c_str());
    char *buffer = ((Hhei_Ming_Dan *)shm_address)->buffer;
    int count = ((Hhei_Ming_Dan *)shm_address)->count;
    //上读锁
    pthread_rwlock_rdlock(&((Hhei_Ming_Dan *)shm_address)->m_lock);
    for (int i = 0; i < count; i++) {
        if (strcmp(&buffer[i * 20], ip) == 0) {
            LogInfo("恶意用户请求连接，拒绝连接");
            pthread_rwlock_unlock(&((Hhei_Ming_Dan *)shm_address)->m_lock);
            return true;
        }
    }
    pthread_rwlock_unlock(&((Hhei_Ming_Dan *)shm_address)->m_lock);
    return false;
}

void processpool::check_time_out_cb(int fd, short events, void *arg) {
    // 检查客户端是否超时连接
    while (true && msg_queue_back != msg_queue_front) {
        if (client_map.count(msg_queue[msg_queue_front]->usr_uuid) <= 0) {
            MemoryPoolFree(mp, msg_queue[msg_queue_front]);
            msg_queue_front = (msg_queue_front + 1) % MSG_QUEUE_LEN;
            continue;
        }
        time_t cur_time = time(NULL);
        time_t diff_time = cur_time - msg_queue[msg_queue_front]->arr_time;
        if (diff_time > MSG_TIMEOUT) {
            if (!msg_queue[msg_queue_front]->msg_flag) {
                // 关闭客户端连接
                bufferevent *bev = msg_queue[msg_queue_front]->bev;
                // int clifd = bufferevent_getfd(bev);
                // struct linger lger;
                // lger.l_onoff = 1;
                // lger.l_linger = 0;
                // if (clifd)
                // {
                //     setsockopt(clifd, SOL_SOCKET, SO_LINGER, &lger, sizeof(lger));
                //     close(clifd);
                // }
                bufferevent_free(bev);
                int c_id = msg_queue[msg_queue_front]->usr_uuid;
                delete client_map[c_id];
                client_map.erase(client_map.find(c_id));
                LogInfo("已关闭超时客户端");
            }
            MemoryPoolFree(mp, msg_queue[msg_queue_front]);
            msg_queue_front = (msg_queue_front + 1) % MSG_QUEUE_LEN;
        } else {
            break;
        }
    }

    // 检查客户端请求是否已回复
    while (true && fin_queue_back != fin_queue_front) {
        if (client_map.count(fin_queue[fin_queue_front]->usr_uuid) <= 0) {
            MemoryPoolFree(mp, fin_queue[fin_queue_front]);
            fin_queue_front = (fin_queue_front + 1) % MSG_QUEUE_LEN;
            continue;
        }
        time_t cur_time = time(NULL);
        time_t diff_time = cur_time - fin_queue[fin_queue_front]->arr_time;
        if (diff_time > FIN_TIMEOUT) {
            if (!fin_queue[fin_queue_front]->finish_flag) {
                // 向服务器重发客户端请求
                static int count = 0;
                count = count % m_Serverlist.size();
                bufferevent_write(m_Serverlist[count].second, (char *)&fin_queue[fin_queue_front]->head, sizeof(Head));
                bufferevent_write(m_Serverlist[count].second, fin_queue[fin_queue_front]->c_info,
                                  fin_queue[fin_queue_front]->head.length);
                count++;

                fin_queue[fin_queue_front]->arr_time = time(NULL);
                fin_queue[fin_queue_back] = fin_queue[fin_queue_front];
                fin_queue_back = (fin_queue_back + 1) % MSG_QUEUE_LEN;
                if (fin_queue_back == fin_queue_front) {
                    LogWarn("缓存队列已满");
                    exit(0);
                }
                LogInfo("已重发客户端请求");
            } else {
                MemoryPoolFree(mp, fin_queue[fin_queue_front]);
            }
            fin_queue_front = (fin_queue_front + 1) % MSG_QUEUE_LEN;
        } else {
            break;
        }
    }
}

void processpool::pipe_event_CallBack(int fd, short event, void *arg) {
    int message;
    int ret = recv(fd, (char *)&message, sizeof(message), 0);  //接收主线程传来的命令
    if ((ret < 0 && (errno != EAGAIN)) || ret == 0) {
        return;
    } else {
        switch (message) {
            case 1:  //新客户端加入
            {
                evutil_socket_t sockfd;
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                sockfd = ::accept(m_clistenfd, (struct sockaddr *)&client, &len);
                pthread_mutex_lock(&((Hhei_Ming_Dan *)shm_address)->accpet_mutex);
                ((Hhei_Ming_Dan *)shm_address)->accept_flag = true;
                pthread_mutex_unlock(&((Hhei_Ming_Dan *)shm_address)->accpet_mutex);
                if (sockfd == -1) {
                    return;
                }
                //检查是否在黑名单内
                std::string ip = inet_ntoa(client.sin_addr);
                if (verify_client_ip(ip)) {
                    //断开链接
                    close(sockfd);
                    return;
                }
                LogInfo("客户端：%s 通过黑名单检测", ip.c_str());
                evutil_make_socket_nonblocking(sockfd);
                struct event_base *base = (event_base *)arg;
                bufferevent *bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
                bufferevent_setcb(bev, client_socket_read_cb, NULL, client_event_cb, (void *)client_id);
                bufferevent_enable(bev, EV_READ | EV_PERSIST);
                client_information *new_client = new client_information;
                new_client->bev = bev;
                new_client->ip = ip;
                new_client->cli_event_map.insert(make_pair(0, (cli_event *)MemoryPoolAlloc(mp, sizeof(cli_event))));
                new_client->cli_event_map[0]->arr_time = time(NULL);
                new_client->cli_event_map[0]->bev = bev;
                new_client->cli_event_map[0]->usr_uuid = client_id;
                msg_queue[msg_queue_back] = new_client->cli_event_map[0];
                msg_queue_back = (msg_queue_back + 1) % MSG_QUEUE_LEN;
                if (msg_queue_back == msg_queue_front) {
                    LogWarn("缓存队列已满");
                    exit(0);
                }
                client_map.insert(std::map<unsigned int, client_information *>::value_type(client_id++, new_client));
                LogInfo("工作进程 %d 收到主进程信息：新客户端连接", m_idx);
                break;
            }
            case 2:  //新服务端加入
            {
                int length;
                const char *ip;
                int port;
                ret = recv(fd, (char *)&length, sizeof(length),
                           0);  //接收主线程传来的地址
                assert(ret == sizeof(length));
                address server_add;
                ret = recv(fd, (char *)&server_add, length, 0);
                ip = server_add.ip;
                port = server_add.port;

                LogInfo("工作进程 %d 收到主进程信息：新服务端可用, ip: %s, port: %d.", m_idx, ip, port);

                evutil_socket_t sockfd;
                sockaddr_in server_address;
                bzero(&server_address, sizeof(server_address));  //初始化结构体
                server_address.sin_family = AF_INET;
                server_address.sin_port = htons(port);
                server_address.sin_addr.s_addr = inet_addr(ip);
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                ret = connect(sockfd, (sockaddr *)&server_address, sizeof(server_address));
                if (-1 == ret) {
                    LogWarn("工作进程 %d 连接服务器失败, ip: %s, port: %d.", m_idx, ip, port);
                    return;
                } else {
                    LogInfo("工作进程 %d 连接服务器成功, ip: %s, port: %d.", m_idx, ip, port);
                }
                evutil_make_socket_nonblocking(sockfd);
                struct event_base *base = (event_base *)arg;
                bufferevent *bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
                bufferevent_setcb(bev, work_server_socket_read_cb, NULL, work_server_event_cb, NULL);
                bufferevent_enable(bev, EV_READ | EV_PERSIST);
                m_Serverlist.push_back(std::pair<int, bufferevent *>(sockfd, bev));
                std::string ipPort;
                ipPort = string(ip) + ':' + to_string(port);
                m_server_info.push_back(make_pair(bev, ipPort));
                break;
            }
            case 3: {  //某服务端不可用

                int length;
                const char *ip;
                int port;
                ret = recv(fd, (char *)&length, sizeof(length),
                           0);  //接收主线程传来的地址
                assert(ret == sizeof(length));
                address server_add;
                ret = recv(fd, (char *)&server_add, length, 0);
                ip = server_add.ip;
                port = server_add.port;
                LogInfo("工作进程 %d 收到主进程信息: 服务端不可用, ip: %s, port: %d", m_idx, ip, port);
                std::string ipPort;
                ipPort = string(ip) + ':' + to_string(port);

                size_t ind = 0;
                bufferevent *bev = NULL;

                for (; ind < m_server_info.size(); ind++) {
                    if (m_server_info[ind].second == ipPort) {
                        bev = m_server_info[ind].first;
                        break;
                    }
                }

                if (bev) {
                    swap(m_Serverlist[ind], m_Serverlist.back());
                    m_Serverlist.pop_back();
                    bufferevent_free(bev);
                } else {
                    LogInfo("宕机服务器 %s:%d 已不在工作进程 %d 的服务器列表中.", ip, port, m_idx);
                }

                break;
            }

            case 4:  //终止
            {
                LogInfo("工作进程 %d 收到主进程信息：网关服务器关闭", m_idx);
                //释放资源
                for (auto x : client_map) {
                    client_information *client_inf = x.second;
                    bufferevent_free(client_inf->bev);
                    delete client_inf;
                }
                for (auto x : m_Serverlist) bufferevent_free(x.second);
                event_base_loopbreak(m_base);
                MemoryPoolClear(mp);
                MemoryPoolDestroy(mp);
                break;
            }
            default:
                break;
        }
    }
}

bool processpool::current_limit(unsigned int c_id) {
    client_information *client_inf = client_map[c_id];
    if (client_inf->count++ < 10) return true;
    //断开该客户连接并记录黑名单
    LogWarn("客户请求速率过快");
    client_inf->close_flag = true;
    //通知主进程

    message msg;
    msg.msg_type = 2;
    msg.process_id = m_idx;
    strcpy(msg.ip, client_inf->ip.c_str());
    LogInfo("工作进程 %d 发现恶意用户: ip: %s", m_idx, msg.ip);

    int ret = msgsnd(msgid, &msg, sizeof(msg), 0);
    if (0 != ret) {
        LogWarn("工作进程 %d 通知主进程恶意用户失败", m_idx);
        return false;
    } else {
        LogInfo("工作进程 %d 通知主进程恶意用户成功", m_idx);
    }

    // hei_ming_dan.insert(client_inf->ip);
    return false;
}

void processpool::client_socket_read_cb(bufferevent *bev, void *arg) {
    unsigned int c_id = (unsigned int)((((long)arg) << 32) >> 32);
    if (client_map[c_id]->close_flag) {
        return;
    }
    //接收消息
    while (true) {
        //检查是否够一个头部的长度
        if (evbuffer_get_length(bufferevent_get_input(bev)) < sizeof(Head)) {
            LogWarn("不足头部长度");
            return;
        }
        Head head;
        //复制出头部
        assert(evbuffer_copyout(bufferevent_get_input(bev), (void *)&head, sizeof(head)) != -1);
        // std::cout << head.length << std::endl;
        //检查该包是否完整到达
        if (evbuffer_get_length(bufferevent_get_input(bev)) < sizeof(Head) + head.length) {
            LogWarn("不足包体长度");
            return;
        }
        //取出一个包
        bufferevent_read(bev, (char *)&head, sizeof(head));
        char buffer[1024] = {0};
        bufferevent_read(bev, buffer, head.length);
        std::string msg(buffer);
        // 解析 HTTP 报文
        // 在这里限制报文的形式:
        // 1. 只能使用 POST, 否则返回 400 (Bad Request)
        // 2. 只能访问 /, 否则返回 403 (Forbidden)
        // 4. 不 keep-alive (HTTP 1.1 默认 keep-alive), 即 Connection: close
        // struct evbuffer *buffer_ = bufferevent_get_input(bev);
        // // if (evbuffer_get_length(buffer_) == 0) {
        // //   return;
        // // }

        // auto ptr_request_ = evbuffer_search(buffer_, "\r\n", 2, nullptr);
        // char c_buffer_[1024];  // In general, a POST msg only with usrname & passwd should not be longer than 256
        // chars memset(c_buffer_, 0, 1024); evbuffer_remove(buffer_, c_buffer_, ptr_request_.pos + 2);
        // LogDebug("request: %s", c_buffer_);
        // int code_ = HttpHelper::ParseRequest(std::string(c_buffer_));

        // // TODO: when code_ != 200, parser should not work
        // memset(c_buffer_, 0, 1024);
        // auto ptr_header_ = evbuffer_search(buffer_, "\r\n\r\n", 4, nullptr);
        // evbuffer_remove(buffer_, c_buffer_, ptr_header_.pos + 4);
        // LogDebug("header: %s", c_buffer_);
        // int content_length_ = HttpHelper::ParseContentLength(c_buffer_, ptr_header_.pos + 2);

        Json::Value client_rep;

        // if (content_length_ > 512) {
        //     LogWarn("HTTP msg is too large, reject this post.");
        //     code_ = 400;

        //     client_rep["username"] = "";
        //     client_rep["password"] = "";
        //     client_rep["task"] = "apple";
        //     client_rep["type"] = 4;
        // } else {
        //     memset(c_buffer_, 0, 1024);
        // }
        // evbuffer_remove(buffer_, c_buffer_, content_length_);
        // LogDebug("body: %s", c_buffer_);

        // std::string msg(c_buffer_);

        Json::Reader reader;                   //用于将字字符串转换为value对象
        if (!reader.parse(msg, client_rep)) {  //解码
            LogWarn("json reader parse error!");
            return;
        } else {
            LogInfo("parse success.");
        }
        //可以对客户端的请求做一些处理
        //选择服务器
        static int count = 0;
        count = count % m_Serverlist.size();
        // int socketfd = bufferevent_getfd(bev);
        // client_http_code[c_id] = code_;

        //限流
        if (!current_limit(c_id)) {
            return;
        }

        client_rep["client_sock"] = c_id;
        //第几个包
        client_rep["pack_id"] = ++client_map[c_id]->pack_id;
        // Head head;
        // const char *json_client_rep = client_rep.toStyledString().c_str();
        // head.length = strlen(json_client_rep);
        head.length = strlen(client_rep.toStyledString().c_str());
        // char msgmsg[head.length];
        // strcpy(msgmsg, json_client_rep);
        // msgmsg[head.length - 1] = 0;
        LogInfo("head.length: %d, msg: %s", head.length, client_rep.toStyledString().c_str());
        bufferevent_write(m_Serverlist[count].second, (char *)&head, sizeof(Head));
        // bufferevent_write(m_Serverlist[count].second, msgmsg, head.length);

        bufferevent_write(m_Serverlist[count].second, client_rep.toStyledString().c_str(), head.length);
        count++;

        // 客户端请求时间记录
        cli_event *cli_event_msg = client_map[c_id]->cli_event_map[0];
        cli_event_msg->msg_flag = true;

        cli_event *pclievent = (cli_event *)MemoryPoolAlloc(mp, sizeof(cli_event));
        pclievent->arr_time = time(NULL);
        pclievent->usr_uuid = c_id;
        pclievent->bev = cli_event_msg->bev;
        client_map[c_id]->cli_event_map[0] = pclievent;

        msg_queue[msg_queue_back] = pclievent;
        msg_queue_back = (msg_queue_back + 1) % MSG_QUEUE_LEN;
        if (msg_queue_back == msg_queue_front) {
            LogWarn("缓存队列已满");
            exit(0);
        }

        cli_event *pcli_event = (cli_event *)MemoryPoolAlloc(mp, sizeof(cli_event));
        pcli_event->head = head;
        pcli_event->usr_uuid = c_id;
        strcpy(pcli_event->c_info, client_rep.toStyledString().c_str());
        pcli_event->arr_time = time(NULL);
        fin_queue[fin_queue_back] = pcli_event;
        client_map[c_id]->cli_event_map.insert(make_pair(client_map[c_id]->pack_id, pcli_event));
        fin_queue_back = (fin_queue_back + 1) % MSG_QUEUE_LEN;
        if (fin_queue_back == fin_queue_front) {
            LogWarn("缓存队列已满");
            exit(0);
        }
        LogInfo("工作进程%d收到客户端%ld传来的消息", m_idx, c_id);
    }
}

void processpool::client_event_cb(struct bufferevent *bev, short event, void *arg) {
    unsigned int c_id = (unsigned int)((((long)arg) << 32) >> 32);
    LogInfo("工作进程 %d 收到客户端信息：客户退出", m_idx);
    if (event & BEV_EVENT_EOF) {
        LogInfo("connection closed");
    } else if (event & BEV_EVENT_ERROR) {
        LogWarn("some other error");
    }
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
    delete client_map[c_id];
    client_map.erase(client_map.find(c_id));
}

void processpool::work_server_socket_read_cb(bufferevent *bev, void *arg) {
    //接收消息
    while (true) {
        //检查是否够一个头部的长度
        if (evbuffer_get_length(bufferevent_get_input(bev)) < sizeof(Head)) {
            LogWarn("不足头部长度");
            break;
        }
        Head head;
        //复制出头部
        assert(evbuffer_copyout(bufferevent_get_input(bev), (void *)&head, sizeof(head)) != -1);
        //检查该包是否完整到达
        if (evbuffer_get_length(bufferevent_get_input(bev)) < sizeof(Head) + head.length) {
            LogWarn("不足包体长度");
            break;
        }
        //取出一个包
        bufferevent_read(bev, (char *)&head, sizeof(head));
        char buffer[1024] = {0};
        bufferevent_read(bev, buffer, head.length);

        std::string msg(buffer);
        Json::Value server_resp;
        Json::Reader reader;                    //用于将字字符串转换为value对象
        if (!reader.parse(msg, server_resp)) {  //解码
            LogError("json reader parse error!");
            return;
        }
        unsigned int c_id = (unsigned int)server_resp["client_sock"].asInt();
        int pack_id = server_resp["pack_id"].asInt();  //包的编号
        server_resp.removeMember("client_sock");
        server_resp.removeMember("pack_id");
        // std::string msg_http_;
        // HttpHelper::AddState(msg_http_, client_http_code[c_id]);
        // HttpHelper::AddHeader(msg_http_);
        // if (client_http_code[c_id] != 200) {
        //     HttpHelper::AddContent(msg_http_, "Error POST", 11);
        // } else {
        //     int length = strlen(server_resp.toStyledString().c_str());
        //     HttpHelper::AddContent(msg_http_, server_resp.toStyledString().c_str(), length);
        // }

        // char ret_msg_[msg_http_.length() + 1];
        // strcpy(ret_msg_, msg_http_.c_str());

        head.length = strlen(server_resp.toStyledString().c_str());
        //检查客户端还在不在
        if (client_map.count(c_id) <= 0) continue;
        if (client_map[c_id]->cli_event_map.count(pack_id) <= 0) continue;
        //回传消息
        bufferevent_write(client_map[c_id]->bev, (char *)&head, sizeof(Head));
        bufferevent_write(client_map[c_id]->bev, server_resp.toStyledString().c_str(), head.length);
        // bufferevent_write(client_map[c_id]->bev, ret_msg_, sizeof(ret_msg_));
        // 客户端服务完成时间记录
        auto it = client_map[c_id]->cli_event_map.find(pack_id);
        it->second->finish_flag = true;
        //从map中删除该消息记录
        client_map[c_id]->cli_event_map.erase(it);
    }
    LogInfo("工作进程 %d 收到服务端信息", m_idx);
}

void processpool::work_server_event_cb(struct bufferevent *bev, short event, void *arg) {
    //这将自动close套接字和free读写缓冲区
    size_t ind = 0;
    for (; ind < m_Serverlist.size(); ind++) {
        if (m_Serverlist[ind].first == bufferevent_getfd(bev)) {
            break;
        }
    }
    swap(m_Serverlist[ind], m_Serverlist.back());
    m_Serverlist.pop_back();

    ind = 0;
    std::string ipPort;
    for (; ind < m_server_info.size(); ind++) {
        if (m_server_info[ind].first == bev) {
            ipPort = m_server_info[ind].second;
            break;
        }
    }
    swap(m_server_info[ind], m_server_info.back());
    m_server_info.pop_back();

    bufferevent_free(bev);

    // 通知网关探测模块服务器故障
    message msg;
    msg.msg_type = 1;
    msg.process_id = m_idx;
    strcpy(msg.ip, ipPort.substr(0, ipPort.find(':')).c_str());
    msg.port = atoi(ipPort.substr(ipPort.find(':') + 1, ipPort.length()).c_str());

    LogWarn("工作进程 %d 与服务端通信故障, ip: %s, port: %d.", m_idx, msg.ip, msg.port);

    if (event & BEV_EVENT_EOF) {
        LogWarn(" error:server connection closed");
    } else if (event & BEV_EVENT_ERROR) {
        LogWarn(" error:some other errors");
    }

    int ret = msgsnd(msgid, &msg, sizeof(msg), 0);
    if (0 != ret) {
        LogError("工作进程 %d 通知被动检测模块失败.", m_idx);
        return;
    } else {
        LogInfo("工作进程 %d 通知被动检测模块成功.", m_idx)
    }
}

void processpool::empty_heimingdan_cb(int fd, short events, void *arg) {
    LogInfo("黑名单重置");
    //上锁
    pthread_rwlock_wrlock(&((Hhei_Ming_Dan *)shm_address)->m_lock);
    //清空黑名单
    bzero(((Hhei_Ming_Dan *)shm_address)->buffer, HEI_MING_DAN_BUFFER_SIZE);  //初始化黑名单
    ((Hhei_Ming_Dan *)shm_address)->count = 0;
    pthread_rwlock_unlock(&((Hhei_Ming_Dan *)shm_address)->m_lock);
}

void processpool::run_parent()  //主线程的工作函数
{
    LogInfo("主进程开始运行");

    // 服务器健康检查模块初始化
    ifstream ifs("./proxy_configurations.json", ios::binary);
    if (!ifs.is_open()) {
        LogError("detect module:open json file failed.");
        return;
    }
    Json::Reader reader;
    Json::Value root;
    Json::Value check_params;
    int interval;
    int maxfails;
    int active;
    int passive;
    string type;
    if (reader.parse(ifs, root)) {
        check_params = root["check_params"];
    }
    interval = check_params["interval"].asInt();
    maxfails = check_params["maxfails"].asInt();
    type = check_params["type"].asString();
    active = check_params["active"].asInt();
    passive = check_params["passive"].asInt();

    ifs.close();
    if (!((bool)interval && (bool)maxfails && (bool)type.c_str())) {
        LogError("配置文件中相关参数缺失或格式错误");
        return;
    }
    ServerMonitor SM(interval, maxfails, type, m_process_number, processpool::m_sub_process, shm_address);
    //开启主动探测
    if (active) {
        SM.StartActiveDetect();
    }
    //开启被动探测
    if (passive) {
        SM.StartPassiveDetect();
    }

    //资源初始化
    m_base = event_base_new();
    evutil_make_socket_nonblocking(m_clistenfd);
    //添加监听客户端请求连接事件
    struct event *c_listen = event_new(m_base, m_clistenfd, EV_READ | EV_PERSIST, ListenerClientCallBack, m_base);
    event_add(c_listen, NULL);

    evutil_make_socket_nonblocking(m_slistenfd);
    //添加监听服务端请求连接事件
    struct event *s_listen = event_new(m_base, m_slistenfd, EV_READ | EV_PERSIST, ListenerServerCallBack, m_base);
    event_add(s_listen, NULL);

    //主线程中的信号处理
    struct event *signal_SIGINT = event_new(m_base, SIGINT, EV_SIGNAL | EV_PERSIST, signal_cb, m_base);
    assert(event_add(signal_SIGINT, NULL) == 0);
    struct event *signal_SIGTERM = event_new(m_base, SIGTERM, EV_SIGNAL | EV_PERSIST, signal_cb, m_base);
    assert(event_add(signal_SIGTERM, NULL) == 0);

    //添加主进程黑名单刷新事件
    //定时任务,清空黑名单
    timeval beat1 = {600, 0};  //每十分钟清空一次
    event *empty_heimingdan = event_new(m_base, -1, EV_PERSIST | EV_TIMEOUT, empty_heimingdan_cb, NULL);
    event_add(empty_heimingdan, &beat1);

    //开启监听事件循环
    LogInfo("start event loop");
    event_base_dispatch(m_base);
    // 网关停止服务器探测，准备下线
    SM.StopServerMonitor();
    event_free(c_listen);
    event_free(s_listen);
    event_free(signal_SIGINT);
    event_free(empty_heimingdan);
    event_free(signal_SIGTERM);
    //释放资源
    event_base_free(m_base);
    pthread_rwlock_destroy(&((Hhei_Ming_Dan *)shm_address)->m_lock);
    shmdt(shm_address);
    shmctl(shm_id, IPC_RMID, NULL);
};

void processpool::ListenerClientCallBack(int fd, short events, void *arg) {
    if (pthread_mutex_trylock(&((Hhei_Ming_Dan *)shm_address)->accpet_mutex) == 0)  //加锁成功
    {
        if (((Hhei_Ming_Dan *)shm_address)->accept_flag) {
            //轮询通知的方法，选择一个工作进程通知接收新客户端
            static int count = 0;
            count = count % m_process_number;
            int message = 1;
            send(m_sub_process[count].m_piped[0], (char *)&message, sizeof(message), 0);
            LogInfo("主进程向工作进程 %d 发送新客户端达到通知", count);
            count++;
            ((Hhei_Ming_Dan *)shm_address)->accept_flag = false;
        }
        pthread_mutex_unlock(&((Hhei_Ming_Dan *)shm_address)->accpet_mutex);
    }
}

void processpool::ListenerServerCallBack(int fd, short events, void *arg) {
    LogInfo("主进程监听到新服务端注册连接");
    //建立与服务端的连接
    evutil_socket_t sockfd;
    struct sockaddr_in server;
    socklen_t len = sizeof(server);
    sockfd = accept(fd, (struct sockaddr *)&server, &len);
    evutil_make_socket_nonblocking(sockfd);
    printf("accept a server %d\n", sockfd);
    bufferevent *bev = bufferevent_socket_new(m_base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, server_socket_read_cb, NULL, server_event_cb, NULL);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);
    m_Serverlist.push_back(std::pair<int, bufferevent *>(sockfd, bev));
}

void processpool::server_socket_read_cb(bufferevent *bev, void *arg) {
    //收取服务端传来的监听地址
    int length;
    int len = bufferevent_read(bev, (char *)&length, sizeof(length));
    assert(len == sizeof(length));
    address server_add;
    len = bufferevent_read(bev, (char *)&server_add, length);

    // assert(len == length);
    // 此处主进程因错误退出后，工作进程未退出
    int ret = 0;
    char msg[1024];
    if (string(server_add.token) == "login") {
        LogInfo("服务器向网关注册, ip: %s, port: %d.", server_add.ip, server_add.port);
        ret = ServerMonitor::LoginServer(server_add.ip, server_add.port);
        if (0 != ret) {
            LogWarn("服务器注册失败！");
            strcpy(msg, "注册失败！");
            int msg_length = sizeof(msg);
            bufferevent_write(bev, (char *)&msg_length, sizeof(msg_length));
            bufferevent_write(bev, msg, msg_length);
            return;
        } else {
            LogInfo("服务器注册成功！");
            strcpy(msg, "注册成功！");
            int msg_length = sizeof(msg);
            bufferevent_write(bev, (char *)&msg_length, sizeof(msg_length));
            bufferevent_write(bev, msg, msg_length);
        }

        //通知工作进程有新服务器注册
        int message = 2;
        for (int i = 0; i < m_process_number; i++) {
            send(m_sub_process[i].m_piped[0], (char *)&message, sizeof(message), 0);
            send(m_sub_process[i].m_piped[0], (char *)&length, sizeof(length), 0);
            send(m_sub_process[i].m_piped[0], (char *)&server_add, length, 0);
        }
        LogInfo("通知工作进程有新服务器注册");
    } else if (string(server_add.token) == "logout") {
        LogInfo("服务器向网关注销, ip: %s, port: %d.", server_add.ip, server_add.port);
        ret = ServerMonitor::LogoutServer(server_add.ip, server_add.port);
        if (0 != ret) {
            LogWarn("服务器注销失败！");
            strcpy(msg, "注销失败！");
            int msg_length = sizeof(msg);
            bufferevent_write(bev, (char *)&msg_length, sizeof(msg_length));
            bufferevent_write(bev, msg, msg_length);
            return;
        } else {
            LogInfo("服务器注销成功！") strcpy(msg, "注销成功！");
            strcpy(msg, "注销成功！");
            int msg_length = sizeof(msg);
            bufferevent_write(bev, (char *)&msg_length, sizeof(msg_length));
            bufferevent_write(bev, msg, msg_length);
        }
        //通知工作进程有服务器注销
        int message = 3;
        for (int i = 0; i < m_process_number; i++) {
            send(m_sub_process[i].m_piped[0], (char *)&message, sizeof(message), 0);
            send(m_sub_process[i].m_piped[0], (char *)&length, sizeof(length), 0);
            send(m_sub_process[i].m_piped[0], (char *)&server_add, length, 0);
        }
        LogInfo("通知工作进程有服务器注销");
    } else {
        LogWarn("服务器token有误，无法识别信息");
        return;
    }
}

//回调函数
//服务器断开连接的时候调用该函数
void processpool::server_event_cb(struct bufferevent *bev, short event, void *arg) {
    LogInfo("服务端注册连接断开");
    if (event & BEV_EVENT_EOF) {
        LogWarn("server connection closed");
    } else if (event & BEV_EVENT_ERROR) {
        LogWarn("some other error in server");
    }
    size_t ind = 0;
    for (; ind < m_Serverlist.size(); ind++) {
        if (m_Serverlist[ind].first == bufferevent_getfd(bev)) {
            break;
        }
    }
    swap(m_Serverlist[ind], m_Serverlist.back());
    m_Serverlist.pop_back();
    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
}

void processpool::signal_cb(evutil_socket_t sig, short events, void *arg) {
    LogInfo("主进程收到信号");
    //通知工作进程
    int message = 4;
    for (int i = 0; i < m_process_number; i++) {
        send(m_sub_process[i].m_piped[0], (char *)&message, sizeof(message), 0);
    }
    //释放资源
    for (auto x : m_Serverlist) bufferevent_free(x.second);
    close(m_slistenfd);
    close(m_clistenfd);
    //关闭
    event_base_loopexit(m_base, NULL);
}

#endif  // PROCESSPOOL_H
