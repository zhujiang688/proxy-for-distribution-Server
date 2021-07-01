#include "server_monitor.h"

void *ActiveDetect(void *args)
{
    LogInfo("网关服务器主动健康检测开启");
    LogInfo("主动检测间隔: %d 秒", ServerMonitor::m_interval);
    Json::Value server_up;
    Json::Value server_down;

    int ret = 0;
    bool reload_flag = true;
    std::vector<int> fail_num;

    while (ServerMonitor::m_running)
    {
        sleep(ServerMonitor::m_interval);

        if (reload_flag)
        {
            if (ServerMonitor::LoadServerList(server_up, server_down))
            {
                LogError("load server_list failed");
                pthread_exit(NULL);
            }
            fail_num.resize(server_up.size());
            std::fill(fail_num.begin(), fail_num.end(), 0);
            reload_flag = false;
        } // 服务器列表有变动时，触发reload

        int server_id = 0;
        for (auto it = server_up.begin(); it != server_up.end(); it++)
        {
            Json::Value server_up_ele = *it;

            char ip[20];
            strcpy(ip, server_up_ele["ip"].asString().c_str());
            int port = server_up_ele["port"].asInt();

            evutil_socket_t sockfd;
            ret = ServerMonitor::ConnectServer(sockfd, ip, port);
            if (-1 == ret)
            {
                LogWarn("主动检测连接up服务器失败：%s:%d", ip, port);
                fail_num[server_id]++;
                if (ServerMonitor::m_maxfails <= fail_num[server_id])
                {
                    ret = ServerMonitor::SetServerDown(ip, port);
                    if (-1 == ret)
                    {
                        LogWarn("mark server down failed");
                        reload_flag = true;
                        break;
                    }
                    else
                    {
                        reload_flag = true;
                        break;
                    }
                }
            }
            else
            {
                close(sockfd);
            }

            server_id++;
        }

        for (auto it = server_down.begin(); it != server_down.end(); it++)
        {
            Json::Value server_down_ele = *it;
            char ip[20];
            strcpy(ip, server_down_ele["ip"].asString().c_str());
            int port = server_down_ele["port"].asInt();

            evutil_socket_t sockfd;
            ret = ServerMonitor::ConnectServer(sockfd, ip, port);
            if (0 == ret)
            {
                LogInfo("主动检测连接down服务器成功: %s:%d", ip, port);
                ret = ServerMonitor::SetServerUp(ip, port);
                if (-1 == ret)
                {
                    LogWarn("mark server up failed");
                    pthread_exit(NULL);
                }
                else
                {
                    reload_flag = true;
                }

                break;
            }
        }
    }
    LogInfo("网关服务器主动健康检测关闭");
    pthread_exit(NULL);
    return 0;
}

void *PassiveDetect(void *args)
{
    LogInfo("网关服务器被动健康检测开启");
    key_t m_msg_key = M_MSG_KEY;
    if (-1 == m_msg_key)
    {
        LogWarn("消息队列键值生成失败");
        pthread_exit(NULL);
    }
    int msgid = msgget(m_msg_key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        LogWarn("消息队列创建失败");
        pthread_exit(NULL);
    }

    while (ServerMonitor::m_running)
    {
        message msg;
        long msg_type = 0;

        int ret = msgrcv(msgid, &msg, sizeof(msg), msg_type, IPC_NOWAIT);

        if (ret > 0)
        {
            if (msg.msg_type == 1) //服务器宕机通知
            {
                char *ip = msg.ip;
                int port = msg.port;
                LogWarn("工作进程 %d通知被动检测模块服务器连接故障：%s:%d", msg.process_id, ip, port);
                evutil_socket_t sockfd;
                ret = ServerMonitor::ConnectServer(sockfd, ip, port);
                if (-1 == ret)
                {
                    LogWarn("被动检测连接up服务器失败：%s:%d", ip, port);
                    ret = ServerMonitor::SetServerDown(ip, port);
                    if (-1 == ret)
                    {
                        LogWarn("mark server down failed");
                    }
                }
                else
                {
                    close(sockfd);
                    // 通知工作进程重新连接服务器
                    int message = 2;
                    address server_add;
                    server_add.port = port;
                    strcpy(server_add.ip, ip);
                    int length = sizeof(server_add);
                    send(ServerMonitor::m_sub_process[msg.process_id].m_piped[0], (char *)&message, sizeof(message), 0);
                }
            }
            else if (msg.msg_type == 2) //恶意用户通知
            {
                LogInfo("主进程添加黑名单");
                //检查恶意用户数量是否超出界限
                Hhei_Ming_Dan *m_hei_ming_dan = (Hhei_Ming_Dan *)args;
                if (m_hei_ming_dan->count == 1000)
                {
                    LogWarn("黑名单溢出");
                    continue;
                }
                char *ip = msg.ip;
                strcpy(&m_hei_ming_dan->buffer[20 * m_hei_ming_dan->count], ip); //添加到共享内存最后一行
                m_hei_ming_dan->count++;
                LogInfo("添加黑名单成功：%s", ip);
            }
        }
        else if (ret == -1)
        {
            sleep(0.01);
        }
        ret = 0;
    }

    msgctl(msgid, IPC_RMID, NULL);
    LogInfo("网关服务器被动健康检测开启");
    pthread_exit(NULL);
}

ServerMonitor::ServerMonitor(int interval, int maxfails, std::string type, int process_num, process *sub_process, void *shm_address)
{
    m_interval = interval;
    m_maxfails = maxfails;
    m_type = type;
    m_process_num = process_num;
    m_sub_process = sub_process;
    m_shm_address = shm_address;
}

int ServerMonitor::StartActiveDetect()
{
    pthread_t Atid;
    int ret = pthread_create(&Atid, NULL, ActiveDetect, m_shm_address); //传入共享内存地址
    if (ret != 0)
    {
        LogError("ActiveDetect pthread_create failed");
        return -1;
    }
    return 0;
}

int ServerMonitor::StartPassiveDetect()
{
    pthread_t Ptid;
    int ret = pthread_create(&Ptid, NULL, PassiveDetect, m_shm_address);
    if (ret != 0)
    {
        LogError("PassiveDetect pthread_create failed");
        return -1;
    }
    return 0;
}

int ServerMonitor::SetServerDown(const char *ip, const int &port)
{
    Json::Value server_up;
    Json::Value server_down;
    Json::Value server_up_new;
    Json::Value down_info;
    bool set_flag = false;
    // 解析json文件
    int ret = ServerMonitor::LoadServerList(server_up, server_down);
    if (ret)
    {
        LogWarn("load server_list failed");
        return -1;
    }
    for (Json::Value::iterator it = server_up.begin(); it != server_up.end(); it++)
    {
        const Json::Value server_up_ele = *it;
        if (server_up_ele["port"].asInt() == port && server_up_ele["ip"].asString() == std::string(ip))
        {
            set_flag = true;
            continue;
        }
        server_up_new.append(*it);
    }
    if (!set_flag)
    {
        return -1;
    }
    down_info["ip"] = ip;
    down_info["port"] = port;
    server_down.append(down_info);
    if (ServerMonitor::UpdateServerList(server_up_new, server_down))
    {
        LogWarn("update server_list failed");
        return -1;
    }
    LogInfo("服务器 %s:%d 被标志为down", ip, port);

    int message = 3;
    address server_add;
    server_add.port = port;
    strcpy(server_add.ip, ip);
    int length = sizeof(server_add);
    LogInfo("通知工作进程服务器宕机:ip:%s port:%d", ip, port);
    for (int i = 0; i < m_process_num; i++)
    {
        send(m_sub_process[i].m_piped[0], (char *)&message, sizeof(message), 0);
        send(m_sub_process[i].m_piped[0], (char *)&length, sizeof(length), 0);
        send(m_sub_process[i].m_piped[0], (char *)&server_add, length, 0);
    }

    return 0;
}

void ServerMonitor::StopServerMonitor() { m_running = false; }

int ServerMonitor::LoadServerList(Json::Value &server_up, Json::Value &server_down)
{
    std::ifstream if_json;
    if_json.open("./server_list.json", std::ios::binary);
    if (!if_json.is_open())
    {
        LogWarn("open server_list file failed.");
        return -1;
    }
    Json::Reader reader;
    Json::Value jsonin;
    // 解析json文件
    if (reader.parse(if_json, jsonin))
    {
        server_up = jsonin["up"];
        server_down = jsonin["down"];
    }
    if_json.close();
    return 0;
}

int ServerMonitor::UpdateServerList(Json::Value &server_up, Json::Value &server_down)
{
    Json::StyledWriter writer;
    Json::Value jsonout;
    std::ofstream of_json;
    of_json.open("./server_list.json", std::ios::out);
    if (!of_json.is_open())
    {
        LogWarn("open server_list file failed.");
        return -1;
    }
    jsonout["up"] = server_up;
    jsonout["down"] = server_down;
    of_json << writer.write(jsonout);
    of_json.close();
    return 0;
}

int ServerMonitor::SetServerUp(const char *ip, const int &port)
{
    Json::Value server_up;
    Json::Value server_down_new;
    Json::Value server_down;
    Json::Value up_info;
    bool set_flag = false;
    // 解析json文件
    int ret = ServerMonitor::LoadServerList(server_up, server_down);
    if (ret)
    {
        LogWarn("load server_list failed");
        return -1;
    }
    for (Json::Value::iterator it = server_down.begin(); it != server_down.end(); it++)
    {
        const Json::Value &server_down_ele = *it;
        if (server_down_ele["port"].asInt() == port && server_down_ele["ip"].asString() == std::string(ip))
        {
            set_flag = true;
            continue;
        }
        server_down_new.append(*it);
    }
    if (!set_flag)
    {
        return -1;
    }

    up_info["ip"] = ip;
    up_info["port"] = port;
    server_up.append(up_info);
    if (ServerMonitor::UpdateServerList(server_up, server_down_new))
    {
        LogWarn("update server_list failed");
        return -1;
    }

    LogInfo("服务器 %s:%d 被标志为up", ip, port);

    int message = 2;
    address server_add;
    server_add.port = port;
    strcpy(server_add.ip, ip);
    int length = sizeof(server_add);

    for (int i = 0; i < m_process_num; i++)
    {
        send(m_sub_process[i].m_piped[0], (char *)&message, sizeof(message), 0);
        send(m_sub_process[i].m_piped[0], (char *)&length, sizeof(length), 0);
        send(m_sub_process[i].m_piped[0], (char *)&server_add, length, 0);
    }
    LogInfo("通知工作进程服务器宕机恢复:ip%s port:%d", ip, port);
    return 0;
}

int ServerMonitor::LoginServer(const char *ip, const int &port)
{
    //收到服务器注册/注销信息，更新server_list(需要锁表)

    Json::Value server_up;
    Json::Value server_down;
    // 解析json文件
    if (ServerMonitor::LoadServerList(server_up, server_down))
    {
        LogWarn("load server_list failed");
        return -1;
    }
    bool login_flag = true;
    Json::Value login_info;
    for (Json::Value::iterator it = server_up.begin(); it != server_up.end(); it++)
    {
        const Json::Value &server_up_ele = *it;

        if (server_up_ele["port"].asInt() == port && server_up_ele["ip"].asString() == std::string(ip))
        {
            login_flag = false;
            break;
        }
    }
    for (Json::Value::iterator it = server_down.begin(); it != server_down.end(); it++)
    {
        const Json::Value &server_down_ele = *it;
        if (server_down_ele["port"].asInt() == port && server_down_ele["ip"].asString() == std::string(ip))
        {
            login_flag = false;
            break;
        } // strcmp 0值填充的字符串无法比较
    }
    if (!login_flag)
    {
        LogWarn("注册信息有误！服务器 %s:%d 已存在", ip, port);
        return -1;
    }
    login_info["port"] = port;
    login_info["ip"] = ip;
    server_up.append(login_info);
    if (ServerMonitor::UpdateServerList(server_up, server_down))
    {
        LogWarn("load server_list failed");
        return -1;
    }
    return 0;
}

int ServerMonitor::LogoutServer(const char *ip, const int &port)
{
    //收到服务器注册/注销信息，更新server_list(需要锁表)
    Json::Value server_up;
    Json::Value server_down;
    Json::Value server_up_new;
    Json::Value server_down_new;
    // 解析json文件
    if (ServerMonitor::LoadServerList(server_up, server_down))
    {
        LogWarn("load server_list failed");
        return -1;
    }
    bool logout_flag = true;
    for (Json::Value::iterator it = server_up.begin(); it != server_up.end(); it++)
    {
        const Json::Value &server_up_ele = *it;
        if (server_up_ele["port"].asInt() == port && server_up_ele["ip"].asString() == std::string(ip))
        {
            logout_flag = false;
            continue;
        }
        server_up_new.append(*it);
    }
    for (Json::Value::iterator it = server_down.begin(); it != server_down.end(); it++)
    {
        const Json::Value &server_down_ele = *it;
        if (server_down_ele["port"].asInt() == port && server_down_ele["ip"].asString() == std::string(ip))
        {
            logout_flag = false;
            continue;
        }
        server_down_new.append(*it);
    }
    if (logout_flag)
    {
        LogInfo("注销信息有误！服务器 %s:%d 不存在", ip, port);
        return -1;
    }
    if (ServerMonitor::UpdateServerList(server_up_new, server_down_new))
    {
        LogWarn("update server_list failed");
        return -1;
    }

    return 0;
}

int ServerMonitor::ConnectServer(evutil_socket_t &sockfd, char *ip, int port)
{
    sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int ret = connect(sockfd, (sockaddr *)&server_address, sizeof(server_address));
    return ret;
}

ServerMonitor::~ServerMonitor() {}

int ServerMonitor::m_maxfails = 1;
int ServerMonitor::m_interval = 1;
std::string ServerMonitor::m_type = "";
bool ServerMonitor::m_running = true;
int ServerMonitor::m_process_num = 0;
process *ServerMonitor::m_sub_process = NULL;
void *ServerMonitor::m_shm_address = NULL;
