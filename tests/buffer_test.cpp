#include "src/buffer.h"
#include <gtest/gtest.h>
#include <thread>

// 基础功能测试
TEST(BufferTest, BasicReadWrite) {
    Buffer buf;
    const char* testStr = "Hello Buffer";
    buf.Append(testStr, strlen(testStr));
    
    EXPECT_EQ(buf.ReadableBytes(), strlen(testStr));
    EXPECT_EQ(std::string(buf.Peek(), buf.ReadableBytes()), testStr);
}

// 扩容测试
TEST(BufferTest, AutoExpand) {
    Buffer buf(10); // 初始大小10
    std::string bigStr(1000, 'x');
    buf.Append(bigStr.data(), bigStr.size());
    
    EXPECT_GE(buf.WritableBytes(), bigStr.size());
}

// 多线程测试
TEST(BufferTest, ThreadSafety) {
    Buffer buf;
    auto writer = [&buf]() {
        for(int i=0; i<1000; ++i) {
            buf.Append("test", 4);
        }
    };

    std::thread t1(writer);
    std::thread t2(writer);
    t1.join();
    t2.join();
    
    EXPECT_EQ(buf.ReadableBytes(), 8000);
}

// IO操作测试
TEST(BufferTest, FileIO) {
    Buffer buf;
    // 创建临时文件
    FILE* tmp = tmpfile();
    int fd = fileno(tmp);
    
    const char* content = "Test content";
    write(fd, content, strlen(content));
    lseek(fd, 0, SEEK_SET);
    
    int err = 0;
    ssize_t n = buf.ReadFd(fd, &err);
    EXPECT_EQ(n, strlen(content));
    EXPECT_EQ(std::string(buf.Peek(), buf.ReadableBytes()), content);
    
    close(fd);
}