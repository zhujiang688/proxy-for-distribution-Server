# proxy-for-distribution-Server
单个网关加多个服务器的分布式服务器系统，数据库采用的是单个mysql
整体基于libevent搭建
网关具有客户端限流、服务器水平扩展与容灾、客户端消息缓存与超时重发等功能
网关具有负载均衡的能力，能够将客户请求轮询转发给不同的服务器
网关采用多进程的方式实现，服务端采用多线程的方式实现
基于该系统实现了一个抢票业务。有新建抢票活动，查询是否有票，抢票三个接口

[引力计划项目总结.docx](https://github.com/zhujiang688/proxy-for-distribution-Server/files/6764911/default.docx)


