#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>      // 文件控制相关函数
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>     // 套接字相关函数和数据结构
#include <netinet/in.h>     // 网络地址结构定义(sockaddr_in)
#include <arpa/inet.h>      // 网络地址转换函数(htons  htonl)

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../http/httpconn.h"

class WebServer {
public:
// 监听来自内核的epoll事件，将写任务放入写任务队列，读任务放入读任务队列，与httpconn交互
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);
    
    ~WebServer();
    // 服务器主循环
    void Start();

private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);      // 根据参数设置监听和连接的事件模式

    // 连接管理函数
    void AddClient_(int fd, sockaddr_in addr);
    void CloseConn_(HttpConn* client);
    void ExtentTime_(HttpConn* client);

    // 事件处理函数
    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    // 请求处理函数
    void OnProcess_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnRead_(HttpConn* client);

    void SendError_(int fd, const char* info);
    static int SetFdNonblock(int fd);       // 设置文件描述符为非阻塞模式

    static const int MAX_FD = 65536;
    
    int port_;
    bool openLinger_;   // 优雅关闭
    int timeoutMS_;
    bool isClose_;
    int listenFd_;
    char* srcDir_;

    uint32_t listenEvent_;  // 监听套接字的事件模式
    uint32_t connEvent_;     // 连接套接字的事件模式

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;   // fd ——> Http客户端连接
};


#endif