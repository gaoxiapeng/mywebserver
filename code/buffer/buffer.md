# `Buffer` 类详解笔记

---

## 构造函数
### `Buffer(int initBuffSize)`
- **功能**：构造一个初始容量为 `initBuffSize` 的缓冲区，并将 `readPos_` 和 `writePos_` 置零。
- **用到的函数**：
  - `std::vector<char>(initBuffSize)`  
    创建一个长度为 `initBuffSize` 的 `vector<char>`，每个元素初始化为 `'\0'`。  
    参数：`size_type count` → 容量大小。  
    返回：一个新的 `vector`。  
- **在 Buffer 中的作用**：保证有一块连续内存作为数据存储区。

---

# 读部分

### `WritableBytes()`
- **功能**：返回缓冲区尾部还能写多少字节。
- **内部调用**：仅用到 `std::vector::size()`。  
  - **作用**：返回当前 `vector` 的元素数量（即容量大小）。  
- **Buffer 作用**：判断是否有足够空间进行写入。

---

### `ReadableBytes()`
- **功能**：返回可读数据的字节数（`writePos_ - readPos_`）。
- **内部调用**：无。  
- **Buffer 作用**：确定能读多少数据。

---

### `PrependableBytes()`
- **功能**：返回前面已被读走、可复用的空间大小。  
- **内部调用**：无。  
- **Buffer 作用**：在扩容前先判断能否通过数据搬移复用空间。

---

### `Peek()`
- **功能**：返回当前可读数据的起始指针。  
- **内部调用**：`BeginPtr_()` → `&*buffer_.begin()`。  
  - `std::vector::begin()`：返回指向首元素的迭代器。  
  - `operator*` 解引用迭代器 → 得到首元素引用。  
  - `&` 取地址 → 得到底层数组首地址。  
- **Buffer 作用**：获取一块连续内存，用于直接传递给系统调用或字符串构造函数。

---

### `Retrieve(len)`
- **功能**：将 `readPos_` 前移 `len`，表示这些数据被消费。  
- **内部调用**：`assert(len <= ReadableBytes())`  
  - `assert`：运行时检查条件是否成立，不成立时终止程序。  
- **Buffer 作用**：安全地消费数据，不会越界。

---

### `RetrieveUntil(end)`
- **功能**：将 `readPos_` 移动到 `end` 所指位置。  
- **内部调用**：  
  - `Peek()`：找到当前可读区起点。  
  - `Retrieve(end - Peek())`：通过指针减法计算要前移的长度。  
- **Buffer 作用**：适用于协议解析，按分隔符或结束符消费数据。

---

### `RetrieveAll()`
- **功能**：清空缓冲区。  
- **内部调用**：  
  - `std::fill(buffer_.begin(), buffer_.end(), '\0')`  
    将容器内所有元素置为 `'\0'`。  
    参数：起始迭代器、结束迭代器、填充值。  
- **Buffer 作用**：快速恢复初始状态。

---

### `RetrieveAllToStr()`
- **功能**：返回可读数据的字符串，并清空缓冲区。  
- **内部调用**：  
  - `std::string(Peek(), ReadableBytes())`  
    构造字符串：从 `Peek()` 开始拷贝 `ReadableBytes()` 个字符。  
  - `RetrieveAll()`：重置缓冲区。  
- **Buffer 作用**：把所有内容一次性取走，常用于日志或完整请求读取。

---

# 写部分

### `EnsureWriteable(len)`
- **功能**：保证至少有 `len` 空间可写，否则扩容或搬移。  
- **内部调用**：  
  - `WritableBytes()` 判断剩余空间。  
  - `MakeSpace_(len)` 处理不足情况。  
  - `assert(WritableBytes() >= len)` 确保最终空间足够。  
- **Buffer 作用**：写入前的空间管理。

---

### `HasWritten(len)`
- **功能**：写入后更新 `writePos_`。  
- **内部调用**：无。  
- **Buffer 作用**：标记可读数据范围扩展。

---

### `BeginWriteConst()` / `BeginWrite()`
- **功能**：返回当前可写位置的指针（一个是 const，一个可写）。  
- **内部调用**：`BeginPtr_()`。  
- **Buffer 作用**：提供数据写入入口。

---

# 数据追加部分

### `Append(const char* str, size_t len)`
- **功能**：向缓冲区追加一段数据。  
- **内部调用**：  
  - `assert(str)` 保证传入指针有效。  
  - `EnsureWriteable(len)` 保证空间。  
  - `std::copy(str, str + len, BeginWrite())`  
    将 `[str, str+len)` 区间拷贝到缓冲区写入位置。  
  - `HasWritten(len)` 更新写指针。  
- **Buffer 作用**：核心写入接口。

---

### `Append(const std::string& str)`
- **功能**：写入字符串。  
- **内部调用**：  
  - `str.data()`：返回底层字符数组指针。  
  - `str.length()`：返回字符串长度。  
  - 调用 `Append(const char*, size_t)`。  
- **Buffer 作用**：支持 C++ 字符串写入。

---

### `Append(const void* data, size_t len)`
- **功能**：写入任意二进制数据。  
- **内部调用**：  
  - `static_cast<const char*>(data)` → 转换为字节指针。  
  - 调用 `Append(const char*, size_t)`。  
- **Buffer 作用**：适合存储网络包、图像等原始数据。

---

### `Append(const Buffer& buff)`
- **功能**：将另一个 `Buffer` 的内容写入本缓冲区。  
- **内部调用**：  
  - `buff.Peek()` → 数据起点。  
  - `buff.ReadableBytes()`（应为这个，你代码里写成了 `WritableBytes()`）。  
  - 调用 `Append(const char*, size_t)`。  
- **Buffer 作用**：合并两个缓冲区数据。

---

# I/O 操作

### `ReadFd(int fd, int* saveErrno)`
- **功能**：从文件描述符读取数据到缓冲区。  
- **内部调用**：  
  - `struct iovec`：分散读结构体，`iov_base` 指针，`iov_len` 长度。  
  - `readv(fd, iov, 2)`：系统调用，一次性读到多个缓冲区。  
    - 参数：文件描述符、`iovec` 数组、数组长度。  
    - 返回：实际读取字节数，错误时返回 -1 并置 `errno`。  
  - `Append(buff, len - writeSize)`：当缓冲区不够时，把额外数据写入。  
- **Buffer 作用**：高效读入数据，避免多次系统调用。

---

### `WriteFd(int fd, int* saveErrno)`
- **功能**：把缓冲区可读数据写入文件描述符。  
- **内部调用**：  
  - `write(fd, Peek(), ReadableBytes())`：系统调用，写入数据。  
    - 参数：文件描述符、缓冲区指针、长度。  
    - 返回：写入的字节数，错误返回 -1 并置 `errno`。  
- **Buffer 作用**：输出数据，常用于网络发送。

---

# 内部辅助函数

### `BeginPtr_()`
- **功能**：返回底层数组首地址。  
- **内部调用**：  
  - `buffer_.begin()`：返回迭代器。  
  - `&*it`：取首元素地址。  
- **Buffer 作用**：作为所有指针计算的基准。

---

### `MakeSpace_(len)`
- **功能**：保证至少有 `len` 空间可写。  
- **内部调用**：  
  - `WritableBytes()`、`PrependableBytes()` 判断是否足够。  
  - `buffer_.resize(writePos_ + len + 1)` 扩容。  
  - `std::copy(...)` 将未读数据搬移到前端复用空间。  
- **Buffer 作用**：扩容或数据整理，保证写操作安全。  

---

# 总结
- 读函数：主要依赖 `assert`、`std::fill`、`std::string` 构造。  
- 写函数：主要依赖 `std::copy`、`std::vector::resize`。  
- I/O：主要依赖 **系统调用** `readv`、`write`。  
- 内部函数：基于 `std::vector` 提供指针运算能力。  
