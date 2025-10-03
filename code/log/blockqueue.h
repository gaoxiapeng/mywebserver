// 模板类的具体实现代码也得放到.h文件中
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <sys/time.h>

// 定义模板类
template<class T>
class BlockDeque {
private:
    std::deque<T> deq_;
    std::mutex mtx_;
    std::condition_variable condConsumer_;   // 消费者条件对象
    std::condition_variable condProducer_;   // 生产者条件对象
    size_t capacity_;
    bool isClose_;       // 用来标记队列是否关闭

public:
    // explicit：只允许显示调用构造函数
    explicit BlockDeque(size_t MaxCapacity);
    ~BlockDeque();

    // 队列控制
    void clear();       // 清空队列
    void Close();       // 关闭队列
    void flush();       // 唤醒消费者线程——手动触发消费者处理数据

    // 基础状态查询
    bool empty();       // 检查队列是否为空
    bool full();        // 检查队列是否已满
    size_t size();      // 检查队列元素个数
    size_t capacity();  // 检查队列最大容量

    // 数据存取操作
    T front();
    T back();
    void push_back(const T& item);
    void push_front(const T& item);
    bool pop(T& item);   // 从队首删除元素（成功返回true），队列空时阻塞
    bool pop(T& item, int timeout);  // 超时或关闭返回false
    //普通deque的pop只是删除元素，而这里的pop需要同时完成两个操作：获取队首元素值（通过参数item返回）和删除该元素。
    //这种设计确保了多线程环境下"取值+删除"是一个不可分割的原子操作，避免了其他线程在中间修改队列导致的数据竞争问题。返回值bool则用来表示操作是否成功（如队列关闭时返回false）
};

template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) : capacity(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;      // 标记打开队列
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();               // 清空队列
}

template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

// 队列控制

template<class T>
void BlockDeque<T>::Close() {    // 关闭队列，需要唤醒所有阻塞线程
    // lock_guard是一个模板类；mutex是lg的模板参数，表示需要管理的锁的类型(互斥锁)
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
    isClose_ = true;
    condProducer_.notify_all();
    condConsumer_.notify_all();
}

template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
}

// 基础状态查询

template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

// 数据存取操作

template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
void BlockDeque<T>::push_back(const T& item) {
    // lock_guard不可以手动解锁，当用到wait()时，需要中途手动解锁，因此用unique_lock
    std::unique_lock<std::mutex> locker(mtx_);
    // 不用if是因为可能存在虚假唤醒
    while(deq_.size() >= capacity_) {
        // wait(locker):释放锁，允许其他线程(如消费者)操作队列； 阻塞生产者进程
        // 传入的是unique_lock<std::mutex>对象
        condProducer_.wait(locker);   // 当生产者线程被唤醒时，从此处代码开始 
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(const T& item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front();
    condConsumer_.notify_one();
}

template<class T>
bool BlockDeque<T>::pop(T& item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()) {
        // 两种唤醒条件：1、生产者调用 condConsumer_.notify_one()   2、队列关闭
        condConsumer_.wait(locker);    // 无限期阻塞
        if(isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T& item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()) {
        // std::chrono::seconds 表示以秒为单位的时间间隔
        // std::chrono::seconds 是专门用于条件变量的带超时等待函数
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        }
        if(isClose_) {
            return false;
        }
    }
    item = deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif