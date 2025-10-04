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

// 单例模式核心
Log* Log::Instance() {
    static Log inst;    // 静态局部变量
    return &inst;
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