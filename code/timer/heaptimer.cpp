#include "heaptimer.h"

// 插入新节点并向上调整堆
void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    /*小根堆：特殊的完全二叉树，父节点为i，左节点为2i+1， 右节点为2i+2*/
    size_t j = (i - 1) / 2;      // j是i的父节点
    while(j > 0) {
        if(heap_[j] < heap_[i]) {
            break;
        }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = j;
    ref_[heap_[j].id] = i;
}

bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = 2 * i + 1;
    while(j < n) {
        if(heap_[j+1] < heap_[j]) j++;      // 父节点要跟较小的那个子节点对比
        if(heap_[j] < heap_[i]) {
            SwapNode_(i, j);
            i = j;
            j = 2 * i + 1;
        } else {
            break;
        }
    }
    return i > index;   // 发生节点移动则为true
}

// 向堆中添加新节点或者更新已有节点的超时时间和回调函数
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;   // 作为索引
    /*新节点，由堆尾插入再调整*/
    if(ref_.count(id) == 0) {    
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);
    /*已有节点，调整堆*/
    } else {
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        if(!(siftdown_(i, heap_.size()))) {
            siftup_(i);
        }
    }
}

// 删除指定任务
void HeapTimer::del_(size_t index) {
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    // 先将要删除的任务移至队尾
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {      // 此时的i是原先的堆尾元素
            siftup_(i);
        }
    }
    // 堆尾元素删除
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

// 执行回调并删除任务
void HeapTimer::doWork(int id) {
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

// 调整指定任务的超时时间 —— 延长时间
void HeapTimer::adjust(int id, int timeout) {
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    siftdown_(ref_[id], heap_.size());
}

// 检查并执行所有已超时任务
void HeapTimer::tick() {
    if(heap_.empty()) {
        return;
    }
    // 堆顶时间最小，循环检查堆顶超时时间
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        // std::chrono::duration_cast<MS>()：将时间差转换为毫秒
        // .count()：提取毫秒数
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) {
            res = 0;
        }
    }
    return res;
}
