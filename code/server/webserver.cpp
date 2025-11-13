#include "webserver.h"

using namespace std;

/*
1、初始化
    - 创建监听socket，设置端口复用、优雅关闭等选项
    - 初始化线程池、数据库连接池、日志系统
    - 配置epoll事件模式
2、事件循环核心
    - epoll_wait监听所有文件描述符(监听socket+客户端连接)
3、连接管理
    - 接受连接：accept新客户端，创建HttpConn对象，加入epoll监控
    - 关闭连接：处理异常事件或超时连接
4、请求处理
    - 读事件：投递到线程池读取并解析HTTP请求
    - 写事件：投递到线程池生成并发送HTTP响应
    - 过程控制：根据处理结果动态修改epoll监听事件（EPOLLIN/EPOLLOUT）
*/

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false), 
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
        srcDir_ = getcwd(nullptr, 256);     // 获取当前工作目录
        assert(srcDir_);
        strncat(srcDir_, "/resources/", 16);    // 字符串拼接函数
        HttpConn::userCount = 0;
        HttpConn::srcDir = srcDir_;
        // 单例模式初始化连接池
        SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
        // 事件模式初始化 — 根据trigMode参数设置epoll的触发模式（ET/LT）
        InitEventMode_(trigMode);
        // 初始化监听套接字
        if(!InitSocket_()) {
            isClose_ = true;
        }
        // 日志系统初始化
        if(openLog) {
            Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
            if(isClose_) {
                LOG_ERROR("========== Server init error! ==========");
            } else {
                LOG_INFO("========== Server init ==========");
                LOG_INFO("Port:%d, OptLinger:%s", port_, OptLinger? "true":"false");
                LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                                (listenEvent_ & EPOLLET ? "ET" : "LT"),
                                (connEvent_ & EPOLLET ? "ET" : "LT"));
                LOG_INFO("LogSys level: %d", logLevel);
                LOG_INFO("srcDir:%s", HttpConn::srcDir);
                LOG_INFO("SqlConnPool num:%d, ThreadPool num:%d", connPoolNum, threadNum);
            }
        }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

/*    
struct sockaddr_in {	   // IPv4 地址结构体
    short int sin_family;      // 地址家族，通常为 AF_INET
    unsigned short sin_port;   // 端口号
    struct in_addr sin_addr;   // IP 地址
    unsigned char sin_zero [8]; // 填充字段，通常置 0
};
struct in_addr {
    in_addr_t s_addr;  // in_addr_t 本质是 32 位 IPv4 地址（网络字节序）

struct linger {
    int l_onoff;  // 0=关闭, 1=启用
    int l_linger; // 超时时间（秒）  close()会阻塞​​，直到数据发送完成 或 超过l_linger指定时间
};

};*/

// 初始化监听套接字：负责创建、配置和启动TCP服务器的监听功能
bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    // TCP端口号最大为65535 || 1024以下端口需要root权限
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error", port_);
        return false;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // INADDR_ANY：监听所有可用网卡
    addr.sin_port = htons(port_);

    // 创建套接字——>AF_INET：IPv4协议， SOCK_STREAM：TCP协议  0：默认TCP协议
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    // 设置优雅关闭:在网络通信或服务终止时，​​以一种有序、安全的方式关闭连接或进程​​，确保数据不丢失。
    struct linger optLinger = {0};
    if(openLinger_) {
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    // 设置套接字选项:将优雅关闭配置应用到套接字  SOL_SOCKET：操作套接字层。  SO_LINGER：设置优雅关闭选项。
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    // 端口复用配置  SO_REUSEADDR：允许新的socket绑定处于TIME_WAIT状态的端口
    // TIME_WAIT状态：当TCP连接的一端关闭连接时，进入TIME_WAIT
    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error!");
        close(listenFd_);
        return false;
    }

    // 绑定端口
    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 开始监听   6：表示最多允许 ​​6 个已完成三次握手的连接​​ 在 ACCEPT 队列中等待 accept()。
    // TCP连接请求时，会先将只完成两次握手的连接放入“未完成队列”，一旦三次握手成功，就移到ACCEPT队列
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 注册epoll事件
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    // 设置非阻塞模式
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

// 设置监听套接字和连接套接字(客户端fd)的epoll事件模式
void WebServer::InitEventMode_(int trigMode) {
    // 用于监听 服务器监听socket 的事件，当客户端在握手过程中关闭连接时，服务器立即感知
    listenEvent_ = EPOLLRDHUP;      // 表示对端(客户端)已关闭连接
    // 用于监听 客户端连接socket 的事件，确保一个客户端 fd 的事件只由一个线程处理
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;     // 事件被触发后，该fd会被epoll自动禁用，防止多线程竞争

    switch(trigMode) 
    {
    case 0:
        break;   // 全部LT
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

// 实现主事件循环，调用epoll_wait阻塞等待事件发生，循环处理所有就绪的I/O事件（新连接、读、写、错误）
void WebServer::Start() {
    int timeMS = -1;    // 无就绪线程将一一直阻塞
    if(!isClose_) {
        LOG_INFO("========== Server Start! ==========");
    }
    while(!isClose_) {
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();     // 获取最近的超时时间
        }
        // 超时事件内无I/O事件，则返回0；有I/O事件，则立即返回就绪I/O的数量
        int eventCnt = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_) {
                DealListen_();
            }
            // EPOLLRDHUP：客户端关闭连接  EPOLLHUP：连接完全断开  EPOLLERR：连接发生错误
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected events!");
            }
        }
    }
}


/*连接生命周期管理*/
// 处理新连接：接受（accept）新的客户端连接，进行连接数检查，并为新连接创建管理对象
void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        // fd：专门用于与当前连接的客户端通信
        // 要将IPv4地址结构sockaddr_in强转为通用地址结构sockaddr
        int fd = accept(listenFd_, (struct sockaddr*)&addr, &len);
        if(fd <= 0) {
            return;     // accept失败
        }
        // 连接数检查
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Client is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);    // ET模式循环接收
}

// 注册新客户端：将新连接的套接字加入epoll监控，设置非阻塞模式，并添加超时管理定时器。
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        // 在 timeoutMS_后，调用当前WebServer对象的 CloseConn_方法，并传入 &users_[fd]作为参数，作为回调函数
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

// 向客户端发送错误信息并立即关闭连接
void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);  // 发送失败<0； 0：默认阻塞发送
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Clinet[%d] quit", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}


/*事件处理分发*/
// I/O事件分发：将读/写任务作为异步任务添加到线程池的工作队列中
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}
void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

// I/O实际处理：在工作线程中执行HTTP请求的读取或响应数据的发送
void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);     // 返回的是读取的长度
    // 读取失败    EAGAIN：非阻塞模式下的正常状态​​：发送缓冲区已满，下次再试
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess_(client);
}
void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    // 传输完成
    if(client->ToWriteBytes() == 0) {
        // 如果是持续连接就继续处理下个请求
        if(client->IsKeepAlive()) {
            OnProcess_(client);     
            return;
        }
    }
    // 发送失败
    else if(ret < 0) {
        // 发送缓冲区满了，此时需要重新注册EPOLLOUT(ET模式下的特殊处理，否则无法发送数据)
        if(writeErrno == EAGAIN) {
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
        }
    }
    CloseConn_(client);
}

// 解析HTTP请求并生成响应
void WebServer::OnProcess_(HttpConn* client) {
    // true：生成完整HTTP响应 ——> 需要注册EPOLLOUT准备发送
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } 
    // false：请求不完整/解析失败 ——> 保持EPOLLIN继续接收数据
    else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}


void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) {
        timer_->adjust(client->GetFd(), timeoutMS_);
    }
}

// 将文件描述符设置为非阻塞模式,read()/write()等系统调用在无法立即完成时返回 EAGAIN而非阻塞
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    /*fcntl(fd, F_GETFD, 0)：获取fd当前状态标志
    O_NONBLOCK：非阻塞标志
    fcntl(fd, F_SETFL, new_flags)：设置新的fd标志*/
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}