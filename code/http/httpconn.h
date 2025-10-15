#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>      // 定义基本系统数据类型-size_t、ssize_t、pid_t
#include <sys/uio.h>        // readv、writev
#include <arpa/inet.h>      // 互联网地址操作函数
#include <stdlib.h>         // 通用工具函数—atoi()：字符串转为整数
#include <errno.h>

#include "../buffer/buffer.h"
#include "../pool/sqlconnpool.h"
#include "../log/log.h"
#include "httprequest.h"
#include "httpresponse.h"

/*httpconn实现功能
1、读取请求
2、解析请求
3、生成响应
4、发送响应
*/

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);
    void Close();                   // 关闭连接


    sockaddr_in GetAddr() const;    // 获取客户端地址结构体
    int GetFd() const;              // 获取文件描述符
    int GetPort() const;            // 获取客户端端口号
    const char* GetIP() const;      // 获取客户端IP


    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);
    bool process();                 // 处理HTTP请求并生成响应


    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }
    // 计算待写入的总字节数
    int ToWriteBytes() {
        return iov_[0].iov_len + iov_[1].iov_len;
    }


    static bool isET;       // 是否使用ET(边缘触发)模式
    static const char* srcDir;
    static std::atomic<int> userCount;      // 原子计数器，记录当前活跃的用户连接数


private:
    int fd_;
    struct sockaddr_in addr_;      // 客户端地址信息
    

    bool isClose_;
    int iovCnt_;
    struct iovec iov_[2];       // 响应报文内容较多，因此使用分散写


    Buffer readBuff_;   // 读缓冲区——HTTP请求
    Buffer writeBuff_;  // 写缓冲区——HTTP响应


    HttpRequest request_;
    HttpResponse response_;
};

#endif