#include "log.h"

/*写线程writeThread_不属于线程池，其生命周期和日志实例绑定了*/

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

/*
1、while(!deque_->empty())确保队列中所有日志被处理
2、writeThread_->join()确保写线程完成当前正在执行的操作后自然退出
3、两者结合保证了：所有已产生的日志都被完整写入文件，且不会出现资源访问冲突
*/
Log::~Log() {
    // 由于unique_ptr，deque_和writeThread都是指针，因此用->
    // 处理写线程
    // joinable():检查一个线程对象是否可以被join()或detach()
    if(writeThread_ && writeThread_->joinable()) {
        while(!deque_->empty()) {
            deque_->flush();  // 唤醒后台刷盘线程writeThread_，也就是blockqueue中的消费者线程
        }
        deque_->Close();      // 关闭队列，阻止新的任务进入队列
        writeThread_->join(); // join —— 让主线程等待写线程工作结束，阻塞主线程(调用析构函数的线程)，再析构 ——> 确保日志优雅关闭，防止日志丢失
    }
    // 处理文件资源
    if(fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();        // 强制写入内核缓冲区
        fclose(fp_);    // 关闭文件
    }
}

/*
- 初始化队列和线程(需要判断是否已经有了，防止多次创建)
- 创建文件：日期、名称
- 清空缓冲区，关闭旧文件，打开新文件
*/ 
void Log::init(int level = 1, const char* path, const char* suffix, int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) {    // 异步模式
        isAsync_ = true;
        // 只有当deque_为空时才继续操作，这样是为了防止重复初始化队列和线程
        if(!deque_) {   
            std::unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>(maxQueueSize));
            deque_ = std::move(newDeque);  // newDeque是unique_ptr指针，unique_ptr禁止拷贝构造

            // 动态创建一个线程，并指定其入口函数为 FlushLogThread(入口函数：线程启动后执行的第一个函数)
            std::unique_ptr<std::thread> NewThread(new std::thread(FlushLogThread));
            writeThread_ = std::move(NewThread);

            // 直接构造
            // writeThread_ = std::unique_ptr<std::thread>(new std::thread(FlushLogThread))
        } 
    } 
    else {
        isAsync_ = false;
    }

    lineCount_ = 0;

    // 生成日志文件
    time_t timer = time(nullptr);         // 获取当前时间戳(秒级)
    struct tm* sysTime = localtime(&timer);  // 转换为本地时间结构体
    struct tm t = *sysTime;               // 拷贝一份（避免后续被修改）

    path_ = path;    // 存储日志目录（./logs）
    suffix_ = suffix; // 存储文件后缀（.log）

    // 生成日志文件名，格式：目录/年_月_日.后缀（如 "./logs/2023_10_05.log"）
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
        path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);

    toDay_ = t.tm_mday;  // 记录当前日期（用于后续判断是否跨天）

    {
        // 缓冲区清空并关闭旧文件
        std::lock_guard<std::mutex> locker(mtx_);
        buff_.RetrieveAll();
        if(fp_) {
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a"); // 以追加模式打开
        if(fp_ == nullptr) {
            mkdir(path_, 0777);     // 0777：所有用户都有读、写、执行的权限
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
}

/*
- 判断是否需要创建新的日志文件
- 写日志
- 将日志推送到队列或内核缓冲区中
*/
void Log::write(int level, const char* format, ...) {
    time_t tSec = time(nullptr);
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list valist;   // 可变参数列表

    // 判断是否需要创建新的日志文件
    {
        // 处理日志名
        if(toDay_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES) == 0)) {
            std::unique_lock<std::mutex> locker(mtx_);
            locker.unlock();   // 日志名操作没涉及到共享资源(fp_等，故解锁)
            
            char newFile[LOG_NAME_LEN];
            char tail[36];   // 日期格式化缓冲区
            // 将格式化字符串写入tail缓冲区，并返回长度
            snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

            if(toDay_ != t.tm_mday) {   // 日期变了
                snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
                toDay_ = t.tm_mday;
                lineCount_ = 0;
            } else {    // 一个文件写满了 —— 文件名中多了一个：当天的第几个日志文件(lineCount_ / MA_LINES)
                snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_ / MAX_LINES), suffix_);
            }

            // 关旧日志，开新日志
            locker.lock();  // 操作fp_，需要上锁
            flush();
            fclose(fp_);
            fopen(newFile, "a");
            assert(fp_ != nullptr);
        }
    }

        // 写日志 —— 时间戳-日志级别-用户消息
    {
        std::unique_lock<std::mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%04d-%02d-%2d %02d:%02d:%02d",
                            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                            t.tm_hour, t.tm_min, t.tm_sec);
        // n：格式化后的字符串长度
        buff_.HasWritten(n);

        // 日志级别
        AppendLogLevelTitle_(level);

        // 用户消息
        /*
        LOG_INFO("用户 %s 登录成功，IP: %s", "Alice", "192.168.1.100");
        format = "用户 %s 登录成功，IP: %s"
        valist = ["Alice", "192.168.1.100"]
        */
        va_start(valist, format);   // 初始化可变参数列表
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, valist);
        va_end(valist);   // 清理可变参数列表
        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);    // 换行


        // 将上述写入内存的日志推送到队列或文件中
        if(isAsync_ && deque_ && !deque_->full()) {  // 异步：推送到deque中
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {                                     // 同步：直接写到FILE*缓冲区中
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}


// 异步写日志：将deque_队列中的内容先写到FILE*缓冲区中，而后由FILE*缓冲区写到内核缓冲区，最后由操作系统写入磁盘
// 无限循环，只要deque没关闭，写线程就会一直执行这个函数
void Log::AsyncWrite_() {
    std::string str = "";
    while(deque_->pop(str)) {     // str接收pop出的item
        std::lock_guard<std::mutex> locker(mtx_);
        // 将日志写入FILE*缓冲区
        fputs(str.c_str(), fp_);  // fputs()需要接收const char*类型数据，c_str()将string转为C风格的const char*
    }
}

// 为什么这里任何操作都只唤醒一个写线程，即deque_->flush()(condConsumer.notify_one())，因为前面(std::unique_ptr<std::thread> writeThread_)只构建了一个写线程
// 且一个写线程足以处理整个任务队列，多个写线程反而会造成竞争
void Log::flush() {
    if(isAsync_) {    
        deque_->flush();   // 唤醒writethread_线程，而后执行AsyncWrite_()
    }
    fflush(fp_);    // 从FILE*缓冲区强制写入内核缓冲区
}

// 单例模式核心
Log* Log::Instance() {
    static Log inst;    // 静态局部变量
    return &inst;
}

// 消费者线程的入口函数
void Log::FlushLogThread() {
    // inst是函数内部的静态变量，它的作用域仅限于Instance()函数内部，​​外部无法直接访问inst​​。
    // 因此，​​外部代码只能通过Log::Instance()获取单例对象的指针​​，而不能直接使用inst。
    Log::Instance()->AsyncWrite_();
}

void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
        case 0:
        // 9:字符串长度（"[debug]: "共8字符 + 结尾的\0）
        // 所有字符串必须以\0结尾，这是区分"字符数组"和"字符串"的关键
            buff_.Append("[debug]:", 9);  
            break;
        case 1:
        // "[info] :" 由const char[9]退化为const char* 
            buff_.Append("[info] :", 9);  // 注意[]后的空格，这是为了对齐。空格也算一个字符
            break;
        case 2:
            buff_.Append("[warn] :", 9);
            break;
        case 3:
            buff_.Append("[error]:", 9);
            break;
        default:
            buff_.Append("[info] :", 9);
            break;
    }
}

// 调整通知级别
void Log::SetLevel(int level) {
    std::lock_guard<std::mutex> locker(mtx_);
    level_ = level;
}
// 查看通知级别
int Log::GetLevel() {
    std::lock_guard<std::mutex> locker(mtx_);
    return level_;
}
