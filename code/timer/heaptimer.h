#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include "../log/log.h"

// typedef：为现有数据类型创建别名
typedef std::function<void()> TimeoutCallBack;     // 回调函数类型
typedef std::chrono::high_resolution_clock Clock;  // 返回按秒为单位的时间戳
typedef std::chrono::milliseconds MS;              // 毫秒时间单位
typedef Clock::time_point TimeStamp;               // 时间点类型，表示任务的过期时间

// 一个需要定时触发的任务
struct TimerNode {
    int id;         // 任务的唯一标识符
    TimeStamp expires;  
    TimeoutCallBack cb;   
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
};

class HeapTimer {
public:
    HeapTimer() {
        heap_.reserve(64);
    }
    ~HeapTimer() {
        clear();
    }
    // 调整指定任务的超时时间
    void adjust(int id, int newExpires);
    // 添加或更新定时任务
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    // 立即执行指定任务的回调并删除任务
    void doWork(int id);
    // 清空所有定时任务
    void clear();
    // 检查并执行所有已超时的任务
    void tick();
    // 删除堆顶任务
    void pop();
    // 返回距离下一个任务超时的剩余时间(毫秒)
    int GetNextTick();

private:
    std::vector<TimerNode> heap_;               // 存储定时任务的小根堆
    std::unordered_map<int, size_t> ref_;       // 记录任务id到索引的映射

    void del_(size_t i);                        // 删除指定节点
    void siftup_(size_t i);                     // 插入新节点时向上调整堆
    bool siftdown_(size_t index, size_t n);     // 删除或更新堆顶元素时，向下调整堆
    void SwapNode_(size_t i, size_t j);         // 交换节点并更新ref_
};

#endif