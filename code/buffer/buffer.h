// 当多个源文件（.cpp）通过 #include "Buffer.h"包含同一个头文件时
// 如果头文件没有保护，其内容会被重复展开到每个源文件中。这会导致重复定义和编译失败
#ifndef BUFFER_H   // 如果没有定义BUFFER_H宏
#define BUFFER_H   // 定义宏
// #pragma once    // 替代 #ifndef/#define（C++17）

#include <cstring>
#include <iostream>
#include <unistd.h>  // write, read
#indlude <sys/uio.h> // readv
#include <vector>
#include <atomic>
#include <assert.h>

class Buffer{
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = defalt;

    // 容量查询
    size_t WritableBytes() const;       // 可写空间大小
    size_t ReadableBytes() const;       // 可读空间大小
    size_t PrependableBytes() const;    // 可复用预留空间大小

    // 数据读取操作 
    const char* Peek() const;            // 获取可读数据起始指针
    void Retrieve(size_t len);           // 标记已读取len字节
    void RetrieveUntil(const char* end); // 标记读取到指定位置
    void RetrieveAll();                  // 重置缓冲区（清空数据）
    std::string RetrieveAllToStr();      // 提取所有数据并转为string

    // 数据写入操作 
    void EnsureWriteable(size_t len);    // 判断是否需要扩容
    void HasWritten(size_t len);         // 标记已写入len字节
    const char* BeginWriteConst() const; // 可写区域起始指针（const）
    char* BeginWrite();                  // 可写区域起始指针

    // 数据追加（Append） 
    void Append(const std::string& str);       // 追加string
    void Append(const char* str, size_t len);  // 追加字符数组
    void Append(const void* data, size_t len); // 追加二进制数据
    void Append(const Buffer& buff);           // 追加另一个Buffer的数据

    // I/O 操作 
    ssize_t ReadFd(int fd, int* Errno);  // 从fd读取数据到Buffer
    ssize_t WriteFd(int fd, int* Errno); // 将Buffer数据写入fd

private:
    // 内部辅助函数 
    char* BeginPtr_();                  // 缓冲区起始地址
    const char* BeginPtr_() const;      // 缓冲区起始地址（const）
    void MakeSpace_(size_t len);        // 扩容或搬移数据以腾出空间

    // 成员变量 
    // 模板参数中，必须使用完全限定名称（即需要std::）; char不是模板参数，而size_t是模板参数
    std::vector<char> buffer_;          // 底层存储
    std::atomic<std::size_t> readPos_;  // 读指针（原子操作）
    std::atomic<std::size_t> writePos_; // 写指针（原子操作）
    
};



#endif             // 结束条件编译
