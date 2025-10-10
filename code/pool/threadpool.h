#ifndef THREADPOOL.H
#define THREADPOOL.J

#include <cassert>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <queue>

class ThreadPool {
public:
    // 初始化pool_指针
    explicit ThreadPool(size_t threadCount = 8) : pool_(std::make_shared<Pool>()) {
        assert(threadCount > 0);
        // 创建工作线程
        for(size_t i = 0; i < threadCount; i++) { 
            // thread的构造函数接受可调用对象(此处为Lambda)作为线程的入口函数
            std::thread([pool = pool_] {
                std::unique_lock<std::mutex> locker(pool->mtx);
                while(true) {
                    if(!pool->tasks.empty()) {
                        auto task = pool->tasks.front();
                        pool->tasks.pop();
                        locker.unlock();
                        // 此时释放锁，这样其他线程也可操作队列，否则只能串行执行任务
                        task();
                        locker.lock();
                    }
                    else if(pool->isClosed) {
                        break;
                    }
                    // 任务队列为空
                    else pool->cond.wait(locker);   // 释放锁并阻塞当前线程(直到被notify唤醒)
                }
            }).detach();    // 工作子线程独立运行，和ThreadPool这一主线程独立开来
        }
    }
    
    ThreadPool() = default;   // 运行创建对象时不传参数

    /*
    默认移动构造函数
    ThreadPool pool1(4);  // pool1是智能指针，只能移动不能复制
    ThreadPool pool2 = std::move(pool1);  // 调用移动构造函数
    */
    ThreadPool(ThreadPool&&) = default;   

    // 析构的流程是：关闭pool_后，改变标识符，唤醒所阻塞线程(else pool->cond.wait(locker))，再检查else函数后break
    ~ThreadPool() {
        if(pool_) {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->isClosed = false;
        }
        pool_->cond.notify_all();
    }



    /*
    F&& task是一个通用引用(C++11特性)，用于实现完美转发
    传入左值，自动推导为左值引用(F&)
    传入右值(临时对象-返回值；字面量-int x = 1, 这里的1；move转换结果； Lambda表达式)，自动推导为右值引用(F&&)

    move：移动语义，高效转移资源，避免拷贝   int a = 1;  int b = move(a) ——> a本是左值，但被move转为右值了
    forward：完美转发，保持参数的左值/右值的原有属性
    */
    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();    // 唤醒一个等待的工作线程
    }

private:
    // 封装线程池的共享状态
    struct Pool {
        bool isClosed;
        std::mutex mtx;
        std::condition_variable cond;
        std::queue<std::function<void()>> tasks;   // 任务是无参数、无返回的可调用对象
    };
    /*
    管理线程池的共享状态（Pool结构体）
        1、所有线程（工作线程、主线程）安全访问同一 Pool实例（如任务队列、关闭标志）
        2、当最后一个引用 pool_的线程结束时，自动释放 Pool对象
        3、通过 shared_ptr的原子引用计数，保证 Pool的析构时机正确
    */
    std::shared_ptr<Pool> pool_;


};

#endif