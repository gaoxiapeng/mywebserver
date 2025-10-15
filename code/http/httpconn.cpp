#include "httpconn.h"

// 静态成员变量需要在头文件中声明，在源文件中定义(分配存储空间)
const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
};

HttpConn::~HttpConn() {
    Close();
};

/* 
IPv4地址：
struct sockaddr_in {
    short int sin_family;      // 地址家族，通常为AF_INET
    unsigned short sin_port;   // 端口号
    struct in_addr sin_addr;   // IP地址
    unsigned char sin_zero[8]; // 填充字段，通常置0
};

struct in_addr {
    unsigned long s_addr;      // IPv4地址（32位）
};

大端序：高位在前，低位在后； 小端序：高位在后，低位在前
网络字节序：互连网通信的字节序，一定是大端序； 主机字节序：电脑的字节序
*/
void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    fd_ = fd;
    addr_ = addr;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();     // 关闭内存映射
    if(isClose_ == false) {
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount)
    } 
}

int HttpConn::GetFd() const {
    return fd_;
}

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    /*inet_ntoa()：将网络字节序的32位整型转换为点分十进制字符串
    inet_addr()：将点分十进制字符串转换为网络字节序的32位整型*/ 
    return inet_ntoa(addr_.sin_addr);
} 

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

/*分散读
ET：只在数据到达时触发一次，且必须循环读完*/ 
ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        // readv是不能保证一次读完的，因此这里要用到循环
        len = readBuff_.ReadFd(fd_, saveErrno);
        if(len < 0) {
            break;
        }
    } while(isET);
    return len;
}

/*分散写*/
ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        // len代表write每次写入的长度; 前面虽然定义了两个iov，但看响应报文大小调整使用几个
        len = writev(fd_, iov_, iovCnt_);
        if(len < 0) {
            *saveErrno = errno;
            break;
        }

        if(iov_[0].iov_len + iov_[1].iov_len == 0) {
            break;      // 传输结束
        }
        else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            // 移动iov_[1].iov_base代表下次从这里开始写
            iov_[1].iov_base = (uint*)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);  
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        } else {
            iov_[0].iov_base = (uint*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);
}

bool HttpConn::process() {
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    // 将请求报文写入readBuff_中
    else if(request_.parse(readBuff_)) {    
        LOG_DEBUG("request path is : %s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
        response_.Init(srcDir, request_.path(), false, 400);
    }
    // 给出对应的响应
    response_.MakeResponse(writeBuff_);

    
    // const_cast：用于移除或添加 const修饰符
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /*
    如果请求的文件​有效​​，File()返回该文件的内存映射。
    如果请求的文件​无效​​（如 404），File()返回的是错误页面（如 404.html）的内存映射。
    先通过 mmap将文件映射到内存（File()），
    再用 iov_[1]指向该区域，
    最后通过 writev将响应头和文件内容一并发送​​
    */
    if(response_.FileLen() > 0 && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}