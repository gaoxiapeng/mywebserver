# I/O 多路复用与 epoll 原理精要

## 一、概念概述
- **I/O（Input/Output）**：文件或套接字的数据读写操作。  
- **I/O 多路复用**：一个线程或进程同时监听多个文件描述符（fd）的输入输出状态，提高并发效率。  
- **epoll**：Linux 下实现 I/O 多路复用的系统调用，通过事件驱动机制高效监控大量 fd。

---

## 二、I/O 模型对比

### 1. 同步阻塞
- 调用 `accept`、`recv` 等函数时若无数据可读，线程会阻塞等待。
- CPU 被挂起，效率低。

### 2. 同步非阻塞
- 调用立即返回，不阻塞。
- 程序需反复轮询检查各 fd 状态，CPU 开销大。

### 3. I/O 多路复用
- 将多个 fd 交由内核统一监控，当任一 fd 就绪时通知用户程序。
- 常见实现：`select`、`poll`、`epoll`。

---

## 三、select 与 poll 简述

### select
- 用户传入 `fd_set` 位图，内核返回就绪的 fd。
- 用户仍需遍历所有 fd 查找就绪项。
- **缺点**：fd 数量有限（通常 1024）、每次调用都需拷贝整个 fd_set。

### poll
- 使用 `pollfd` 数组，不受 fd 数量限制。
- 仍需每次传入整个数组，遍历检查每个 fd 状态。
- **改进不大**：仍是 O(n) 级扫描。

---

## 四、epoll 工作机制

### 1. 内核结构
- **红黑树（RB-tree）**：存储所有被监控的 fd 及其事件。
- **就绪队列（ready queue）**：存放已触发事件的 fd。

### 2. 核心系统调用
| 函数 | 作用 |
|------|------|
| `epoll_create()` | 创建 epoll 实例，返回 epfd |
| `epoll_ctl()` | 注册、修改、删除事件（操作红黑树） |
| `epoll_wait()` | 等待事件发生（监控就绪队列） |

```
#include <sys/epoll.h>
// 创建一个新的epoll实例。在内核中创建了一个数据，这个数据中有两个比较重要的数据，一个是需要检测的文件描述符的信息（红黑树），还有一个是就绪列表，存放检测到数据发送改变的文件描述符信息（双向链表）。
int epoll_create(int size);
	- 参数：
		size : 目前没有意义了。随便写一个数，必须大于0
	- 返回值：
		-1 : 失败
		> 0 : 文件描述符，操作epoll实例的
            
typedef union epoll_data {
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;

struct epoll_event {
	uint32_t events; /* Epoll events */
	epoll_data_t data; /* User data variable */
};
    
// 对epoll实例进行管理：添加文件描述符信息，删除信息，修改信息
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
	- 参数：
		- epfd : epoll实例对应的文件描述符
		- op : 要进行什么操作
			EPOLL_CTL_ADD: 添加
			EPOLL_CTL_MOD: 修改
			EPOLL_CTL_DEL: 删除
		- fd : 要检测的文件描述符
		- event : 检测文件描述符什么事情
            
// 检测函数
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
	- 参数：
		- epfd : epoll实例对应的文件描述符
		- events : 传出参数，保存了发生了变化的文件描述符的信息
		- maxevents : 第二个参数结构体数组的大小
		- timeout : 阻塞时间
			- 0 : 不阻塞
			- -1 : 阻塞，直到检测到fd数据发生变化，解除阻塞
			- > 0 : 阻塞的时长（毫秒）
	- 返回值：
		- 成功，返回发送变化的文件描述符的个数 > 0
		- 失败 -1

```


### 3. 运行流程
1. 用户调用 `epoll_create` 创建实例，获得 epfd。 
2. 使用 `epoll_ctl` 向内核注册要监控的 fd 及事件类型。  
3. 调用 `epoll_wait`：
   - 若就绪队列非空，立即返回事件。
   - 若为空：
     - `timeout = 0`：非阻塞返回；
     - `timeout = -1`：无限阻塞，挂起调用进程；
     - `timeout > 0`：阻塞指定时间。
4. 当内核检测到某 fd 状态变化（如 socket 收到数据），会通过回调函数将其加入就绪队列。
5. 内核唤醒等待的进程，将就绪事件批量拷贝到用户空间。

### 4. 优势
- **O(1) 级性能**：内核直接通知具体就绪 fd，无需线性扫描。  
- **事件驱动机制**：通过回调异步通知，减少 CPU 空转。  
- **高并发支持**：单线程即可处理数十万连接。  
- **持久监听**：fd 注册后持续有效，无需重复传入。

---

## 五、数据拷贝机制
- **recv()**：数据从网卡 → 内核 socket 缓冲区 → 用户空间。
- **send()**：数据从用户空间 → 内核缓冲区 → 网卡发送。

---

## 六、总结
- `select`、`poll`：线性扫描，性能随 fd 数量增加而下降。  
- `epoll`：事件驱动 + 内核维护就绪队列，性能稳定高效。  
- 关键优势：**避免重复拷贝与遍历，实现真正的高并发 I/O 多路复用。**
