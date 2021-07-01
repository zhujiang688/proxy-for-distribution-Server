#include "pool.h"

int main() {
    // 加载proxy配置文件
    std::ifstream ifs("./proxy_configurations.json", ios::binary);
    if (!ifs.is_open()) {
        LogError("open json file failed.");
        return -1;
    }

    Json::Reader reader;
    Json::Value root;
    int client_port;
    int server_port;
    int process_number;

    // 解析json文件
    if (reader.parse(ifs, root)) {
        client_port = root["client_port"].asInt();
        server_port = root["server_port"].asInt();
        process_number = root["process_number"].asInt();
    }

    ifs.close();
    if (!((bool)client_port && (bool)server_port && (bool)process_number)) {
        LogError("配置文件中相关参数缺失或格式错误");
        return -1;
    }

    LogInfo("网关服务启动中, 服务器注册端口：%d, 客户端连接端口：%d", server_port, client_port);

    //建立客户端监听事件
    int m_clistenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_clistenfd != -1);
    struct sockaddr_in c_sin;
    c_sin.sin_family = AF_INET;
    c_sin.sin_addr.s_addr = 0;
    c_sin.sin_port = htons(client_port);
    int ret = bind(m_clistenfd, (sockaddr*)&c_sin, sizeof(c_sin));
    assert(ret != -1);
    ret = listen(m_clistenfd, 1000);
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

    processpool* pool =
        processpool::creat(m_clistenfd, m_slistenfd, process_number);
    if (pool) {
        pool->run();
        delete pool;
    }
    LogInfo("网关服务器下线中...");
    pthread_exit(NULL);
    return 0;
}