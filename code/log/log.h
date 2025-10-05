#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <thread>
#include <string>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>  // 提供可变参数
#include <assert.h>
#include <sys/stat.h>  // 文件状态和权限相关
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log{
private:
    Log();   // 私有构造函数，禁止外部实例化对象
    virtual ~Log();   // 虚析构
    void AppendLogLevelTitle_(int level);  // 加日志级别前缀
    void AsyncWrite_();  // 异步写日志
    
private:
// 日志文件配置
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;    // 默认最大长度

    const char* path_;   
    const char* suffix_;   // 日志文件后缀
    int MAX_LINES_;        // 当前最大长度

// 日志状态监控
    int lineCount_;     // 当前写入行数
    int toDay_;         // 日期
    bool isOpen_;       // 日志系统是否开启

// 日志内容处理
    Buffer buff_;    // 日志内容缓冲区
    int level_;      // 当前日志级别（0-3）
    bool isAsync_;   // 是否异步写入标志

// 文件操作
    FILE* fp_;      // 日志文件指针，缓冲多条日志(写入磁盘前都需要先写入FILE*缓冲区)

// 异步日志
// unique_ptr:智能指针，​​独占所有权​​地管理动态分配的对象，确保该对象​​同一时间只能被一个指针拥有
// 会自动释放内存，无需手动delete，确保阻塞队列和写线程的生命周期和日志实例一致
// 这里的成员都是指针类型的
    std::unique_ptr<BlockDeque<std::string>> deque_;  // 阻塞队列(日志内容存放位置)
    std::unique_ptr<std::thread> writeThread_;      // 写日志的线程
    std::mutex mtx_;

public:
    void init(int level, const char* path = "./log",
                const char* suffix = ".log",
                int maxQueueCaoacity = 1024);
    static Log* Instance();   // 单例模式

    // write()：同步模式下写入文件，异步模式下写入阻塞队列
    void write(int level, const char* format,...);
    // 后台线程：从内存队列取出日志，批量写入磁盘（仅异步模式需要）——持续执行
    static void FlushLogThread();   
    // 从队列取日志写入文件(磁盘)——立即执行(由关键日志调用，如错误日志)
    void flush();   

    // 动态调整要记录哪些级别的日志
    int GetLevel();     
    void SetLevel(int level);

    bool IsOpen() {
        return isOpen_;
    }

};

#endif