#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

/* read部分 */
// Pos是可变的，因此返回参数不能加const
size_t Buffer::WritableBytes() const {      // 可读长度
    return buffer_.size() - writePos_;
}     
size_t Buffer::ReadableBytes() const {      // 可写长度
    return writePos_ - readPos_;
};       
size_t Buffer::PrependableBytes() const {   // 可复用空间
    return readPos_;
};    

const char* Buffer::Peek() const {          // 返回可读指针
    // C++11允许指针和整数相加
    return BeginPtr_() + readPos_;
}
void Buffer::Retrieve(size_t len) {         // 已读len，移动readPos_
    assert(len <= ReadableBytes());
    readPos_ += len;
}
void Buffer::RetrieveUntil(const char* end) {  // 将readPos_移到指定位置
    assert(Peek() <= end);
    Retrieve(end - Peek());
}
void Buffer::RetrieveAll() {                // 重置缓冲区
    // 现代C++方法,'\0'是char，'0'是int
    std::fill(buffer_.begin(), buffer_.end(), '\0');
    readPos_ = 0;
    writePos_ = 0;
}
std::string Buffer::RetrieveAllToStr() {    // 提取缓冲区可读部分并清空缓冲区
    // C++11中string的构造函数basic_string(const char* s, size_type count);
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

/* write部分 */
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}
// 这里将检查和写入拆开了（和读不一样，读只有一个Retrieve），这是因为写需要扩容
void Buffer::EnsureWriteable(size_t len) {  // 判断是否需要扩容
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
}

/* 数据追加 —— 针对不同输入类型 */
void Buffer::Append(const char* str, size_t len) {   // 核心：char类型追加
    // Debug模式下调试检查str是否为nullptr
    assert(str);
    EnsureWriteable(len);
    // std::copy(源数据起始指针，源数据目标指针，目标位置起始指针)
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}
void Buffer::Append(const std::string& str) {    // 字符串类型追加
    // str.data()返回str的 const char*
    Append(str.data(), str.length());
}
void Buffer::Append(const void* data, size_t len) {  // 任意类型的二进制（图像、网络包、结构体等）
    assert(data);
    Append(static_cast<const char*>(data), len);
}
void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

/* I/O操作 */
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];   // 只是临时栈，最终需要拷贝到buffer_内
    /*
    struct iovec {
        void  *iov_base;  // 缓冲区起始地址
        size_t iov_len;   // 缓冲区长度
    };
    */
    struct iovec iov[2];
    const size_t writeSize = WritableBytes();
    // 第一个buff定为原始可读区域
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writeSize;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    // 分散读（会自动写入，后续只需要改变指针即可）
    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
    } 
    else if(static_cast<size_t>(len) <= writeSize) {
        HasWritten(static_cast<size_t>(len));
    }
    else {
        writePos_ = buffer_.size();
        // Append前，readv已自动将fd内容写入buff中，后续需要手动将buff中的东西再放入buffer_中
        Append(buff, len - writeSize);
    }
    return len;
}
ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    // 将readable部分的内容写入fd中
    const size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
    } 
    else {
        readPos_ += len;
    }
    return len;
}

/* 内部辅助函数 */
char* Buffer::BeginPtr_() {
    // 先解引用迭代器，再取地址
    return &*buffer_.begin();
}
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {  // 需要扩容（要考虑到prepandable的长度）
        buffer_.resize(writePos_ + len + 1);
    }
    else {
        size_t readSize = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readSize;
        assert(readSize == ReadableBytes());
    }
}
