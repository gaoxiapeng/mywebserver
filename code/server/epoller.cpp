#include "epoller.h"

/*
int epoll_create(int size);         // 成功返回文件描述符，失败返回-1且设置errno
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);     // 成功返回0， 失败返回-1
op: EPOLL_CTL_ADD   EPOLL_CTL_DEL  EPOLL_CTL_MOD

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

struct epoll_event {
    uint32_t     events;  // 监听的事件类型（EPOLLIN：可读  EPOLLOUT：可写  EPOLLET：边缘触发）
    epoll_data_t data;    // 用户数据（通常存 fd）
};
*/

/*
size_t​​      std::vector::size()     表示内存/容器大小的无符号大整数
​​uint32_t​​    epoll_event.events      内核二进制接口的固定位数需求
​​int​​         epoll_wait()            系统调用的错误码兼容性
*/

Epoller::Epoller(int maxEvent) : epollFd_(epoll_create(512)), events_(maxEvent) {
    assert(epollFd_ >= 0 && events_.size() >= 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

// 注册监听事件,传进来的这个events是指监听事件类型
bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    struct epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    struct epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events =events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    struct epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

// 返回就绪文件描述符个数
int Epoller::Wait(int timeoutMs) {
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
} 

int Epoller::GetEventFd(size_t i) const {
    assert(i >= 0 && i < events_.size());
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i >= 0 && i < events_.size());
    return events_[i].events;
}


