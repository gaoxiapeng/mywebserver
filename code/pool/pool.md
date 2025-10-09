### shared_ptr<Pool> pool_ 智能指针：将线程池中的共享资源(如isClosed等)封装起来，该智能指针的作用就是，只要还有一个工作线程没有工作完毕，那么pool_就不会被销毁，当最后一个工作线程工作结束后，自动调用delete
```
// 注意：不要和裸指针混用，始终用shared_ptr传递
// 正确做法（推荐）
std::shared_ptr<Pool> pool_ = std::make_shared<Pool>();

// 错误做法（不推荐）
Pool* raw_pool = new Pool;
std::shared_ptr<Pool> pool_(raw_pool);  
```
> shared_ptr通过**引用计数(安全的，使用原子操作)**管理Pool对象的生命周期，每实例化一个对象，自动计数加一，当自动计数值为0时，Pool自动销毁


### join()  vs  detach()
std::thread必须调用join()或者detach()
- join()代表主线程不需要等待子线程、子线程始终独立运行，直到任务结束
- detach()代表主线程需要等待，子线程结束后主线程才能继续(比如Log析构时，必须写线程结束了再进行析构主进程)