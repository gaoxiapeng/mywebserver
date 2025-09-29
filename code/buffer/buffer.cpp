#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

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
void Buffer::RetrieveUntil(const char* end) {  // 将readPos_移到末尾
    assert(Peek() <= end);
    Retrieve(end - Peek());
}
void Buffer::RetrieveAll() {                // 充值缓冲区
    // 现代C++方法
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