#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);
    ~Epoller();

    bool AddFd(int fd, uint32_t events);        // 添加监听事件
    bool DelFd(int fd);                         // 移除监听
    bool ModFd(int fd, uint32_t events);        // 修改监听事件

    int Wait(int timeoutMs = -1);
    int GetEventFd(size_t i) const;             // 获取第i个就绪事件的fd
    uint32_t GetEvents(size_t i) const;         // 获取第i个就绪事件的事件类型

private: 
    int epollFd_;
    std::vector<struct epoll_event> events_;    //存储就绪事件
};

#endif