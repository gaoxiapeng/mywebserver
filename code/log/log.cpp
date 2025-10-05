#include "log.h"

Log::Log() : lineCount_(0), toDay_(0), isAsync_(nullptr), deque_(nullptr), writeThread_(nullptr), fp_(nullptr) {}

Log::~Log() {
    // 由于unique_ptr，deque_和writeThread都是指针，因此用->
    // 处理写线程
    if(writeThread_ && writeThread_->joinable()) {
        while(!deque_->empty()) {
            deque_->flush();
        }
        deque_->Close();
        writeThread_->join(); // 阻塞调用日志的进程，等待写进程结束
    }
    // 处理文件资源
    if(fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();        // 强制写入FILE*缓冲区数据
        fclose(fp_);    // 关闭文件
    }
}


void Log::init(int level = 1, const char* path, const char* suffix, int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) {    // 异步模式
        isAsync_ = true;
        // 只有当deque_为空时才继续操作，这样是为了防止重复初始化队列和线程
        if(!deque_) {   
            std::unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>(maxQueueSize));
            deque_ = std::move(newDeque);  // newDeque是unique_ptr指针，不能复制，只能移动

            // 动态创建一个线程，并指定其入口函数为 FlushLogThread。
            std::unique_ptr<std::thread> NewThread(new std::thread(FlushLogThread));
            writeThread_ = std::move(NewThread);
        } 
    } else {
            isAsync_ = false;
    }

    lineCount_ = 0;

    // 生成日志文件名
    time_t timer = time(nullptr);         // 获取当前时间戳
    struct tm *sysTime = localtime(&timer);  // 转换为本地时间结构体
    struct tm t = *sysTime;               // 拷贝一份（避免后续被修改）

    path_ = path;    // 存储日志目录（如 "./logs"）
    suffix_ = suffix; // 存储文件后缀（如 ".log"）

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
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
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
            buff_.Append("[info]: ", 9);
            break;
    }
}

// 查看通知级别
int Log::GetLevel() {
    std::lock_guard<std::mutex> locker(mtx_);
    return level_;
}
// 调整通知级别
void Log::SetLevel(int level) {
    std::lock_guard<std::mutex> locker(mtx_);
    level_ = level;
}



// 异步写日志：将deque_队列中的日志先存放到FILE*缓冲区中，而后再调用flush()进行刷盘(由缓冲区写到磁盘中)
void Log::AsyncWrite_() {
    std::string str = "";
    while(deque_->pop(str)) {     // str接收pop出的item
        std::lock_guard<std::mutex> locker(mtx_);
        // 将日志写入FILE*的内核缓冲区
        fputs(str.c_str(), fp_);  // fputs()需要接收const char*类型数据，c_str()将string转为C风格的const char*
    }
}

// 单例模式核心
Log* Log::Instance() {
    static Log inst;    // 静态局部变量
    return &inst;
}

void Log::FlushLogThread() {
    // inst是函数内部的静态变量，它的作用域仅限于Instance()函数内部，​​外部无法直接访问inst​​。
    // 因此，​​外部代码只能通过Log::Instance()获取单例对象的指针​​，而不能直接使用inst。
    Log::Instance()->AsyncWrite_();
}

void Log::flush() {
    if(isAsync_) {    // 先确保所有日志都被写
        deque_->flush();
    }
    fflush(fp_);    // 刷盘
}