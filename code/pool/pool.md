### explicit关键字
- **只允许显示调用构造函数**
- **禁止编译器自动进行类型转换**
```
class MyString {
public:
    explicit MyString(const char* str) {  // 有explicit
        cout << "构造函数: " << str << endl;
    }
};

void printString(MyString str) {
    // 处理字符串...
}

int main() {
    // 显式调用（仍然允许）
    MyString s1("Hello");
    
    // 隐式转换（编译错误）
    MyString s2 = "World";  // 错误：不能隐式转换
    printString("Hello");   // 错误：不能隐式转换
    
    // 显式创建临时对象
    printString(MyString("Hello"));  // 正确
    
    return 0;
}
```
---
### 构造方式
> **显示构造 vs 隐式构造**
```
// 直接初始化
MyClass obj1(100);              // ✅ 显式
MyClass obj2{100};              // ✅ 显式（C++11 列表初始化）

// 显式创建临时对象
func(MyClass(100));             // ✅ 显式
func(MyClass{100});             // ✅ 显式

// 拷贝初始化（可能触发隐式转换）
MyClass obj3 = 100;             // ❌ 隐式（如果构造函数非explicit）

// 函数参数隐式转换
void func(MyClass obj);
func(100);                      // ❌ 隐式（如果构造函数非explicit）

// 返回值隐式转换
MyClass createObj() {
    return 100;                 // ❌ 隐式（如果构造函数非explicit）
}
```
> **移动构造：用于将资源从一个对象"移动"到新对象，避免不必要的深拷贝**
```
ThreadPool(ThreadPool&&) = default;  // 编译器生成默认移动构造函数 ThreadPool&&：右值引用

ThreadPool pool1(4);           // 创建线程池
ThreadPool pool2 = std::move(pool1);  // 移动构造：pool1的资源转移给pool2
// 此时pool1为空但有效，pool2拥有所有线程资源
```
> **右值引用**
```
ClassName&      左值引用    持久对象（有名字）
ClassName&&     右值引用    临时对象（将销毁）
```
```
class String {
public:
    // 移动构造函数
    String(String&& other) {  // other是临时对象的引用
        data_ = other.data_;  // 窃取资源
        other.data_ = nullptr; // 原对象置空
    }
    
private:
    char* data_;
};

// 使用示例
String createString() { return String("hello"); }

String s1 = createString();  // ✅ 触发移动构造：createString()返回临时对象（右值）
```
---
### lambda表达式
`[捕获列表](参数列表) -> 返回类型 { 函数体 }`

>**捕获列表**
```
int x = 10;
auto f1 = [x]() { return x; };        // 值捕获
auto f2 = [&x]() { return x++; };      // 引用捕获
auto f3 = [=]() { return x; };          // 隐式值捕获所有变量
auto f4 = [&]() { x++; };               // 隐式引用捕获所有变量
```
>**参数和返回类型**
```
auto add = [](int a, int b) -> int { return a + b; };  // 显式返回类型
auto mul = [](auto a, auto b) { return a * b; };       // 自动推导
```
> **线程池中的使用**
```
// 创建工作线程
std::thread([pool = pool_] {  // 捕获pool副本
    while(true) {
        // 线程执行逻辑
    }
}).detach();
```
---

### 通用引用+完美转发
> **通用引用： 让模板函数能同时接收左值和右值参数，自动推导正确的引用类型**
**完美转发： 依赖于模板参数推导和通用引用，在函数间传递参数时，保持参数的左值/右值属性不变，实现高效转发**

> **通用引用**
```
template<class F>
void AddTask(F&& task)  // F&& 是通用引用，可接受左值或右值
```
```
// 1. 左值（具名对象）
std::function<void()> task1 = []{ cout << "任务1"; };
pool.AddTask(task1);           // F&& 推导为 F&

// 2. 右值（临时对象）
pool.AddTask([]{ cout << "任务2"; });  // F&& 推导为 F&&

// 3. 移动语义
std::function<void()> task3 = []{ cout << "任务3"; };
pool.AddTask(std::move(task3));  // 显式转为右值
```
> **完美转发 : std::forward<F>(task) ——> 将参数以原始的值类别转发给emplace**
```
// 保持参数的值类别（左值/右值）
pool->tasks.emplace(std::forward<F>(task));

// 等价于：
if (task是左值) {
    pool->tasks.emplace(task);  // 拷贝构造，原内容依旧保留
} else {
    pool->tasks.emplace(std::move(task));  // 移动构造，原内容不保留
}
```
> **通用引用 + 完美转发**

```
template<class F>
void AddTask(F&& task) {
    {
        std::lock_guard<std::mutex> locker(pool_->mtx);
        pool_->tasks.emplace(std::forward<F>(task));
    }
    pool_->cond.notify_one();    // 唤醒一个等待的工作线程
}


// 场景1：传递左值（避免不必要的拷贝）
std::function<void()> heavyTask = []{ /* 重任务 */ };
pool.AddTask(heavyTask);  // forward保持左值，触发拷贝构造
// heavyTask 仍然有效，可重复使用

// 场景2：传递右值（高效移动）
pool.AddTask(std::move(heavyTask));  // forward转为右值，触发移动构造
// heavyTask 被移空，不能再使用

// 场景3：传递临时对象（直接构造）
pool.AddTask([]{ return 42; });  // forward保持右值，直接构造在队列中
```

---
### shared_ptr<Pool> pool_ 智能指针
> **将线程池中的共享资源(如isClosed等)封装起来，该智能指针的作用就是，只要还有一个工作线程没有工作完毕，那么pool_就不会被销毁，当最后一个工作线程工作结束后，自动调用delete**
```
// 注意：不要和裸指针混用，始终用shared_ptr传递
// 正确做法（推荐）
std::shared_ptr<Pool> pool_ = std::make_shared<Pool>();

// 错误做法（不推荐）
Pool* raw_pool = new Pool;
std::shared_ptr<Pool> pool_(raw_pool);  
```
> **shared_ptr通过引用计数(安全的，使用原子操作)管理Pool对象的生命周期，每实例化一个对象，自动计数加一，当自动计数值为0时，Pool自动销毁**

---
### join()  vs  detach()
**std::thread必须调用join()或者detach()**
- **detach()代表主线程不需要等待子线程、子线程始终独立运行，直到任务结束**
- **join()代表主线程需要等待，子线程结束后主线程才能继续(比如Log析构时，必须写线程结束了再进行析构主进程)**

```
ThreadPool pool(4);  // 创建包含4个工作线程的池

// 如果没有detach：
// 1. 主线程必须等待所有工作线程结束
// 2. 工作线程是无限循环，永远不会结束  
// 3. 导致主线程卡在构造函数，无法继续执行

// 使用detach后：
// 1. 工作线程在后台独立运行
// 2. 主线程立即返回，程序继续执行
// 3. 线程池可以正常使用
```
---

### RAII机制
> **在构造中获取资源，在析构中释放资源**
```
class GoodExample {
    FILE* file_;
public:
    // 构造时获取资源
    GoodExample(const char* filename) : file_(fopen(filename, "r")) {}
    
    // 析构时自动释放资源
    ~GoodExample() { if(file_) fclose(file_); }
    
    // 不需要手动close()方法！
};
```
```
// 这个不是RAII机制
class BadExample {
    FILE* file_;
public:
    BadExample() {}  // 空构造函数
    ~BadExample() {} // 空析构函数
    void open(const char* filename) { file_ = fopen(filename, "r"); } // 手动获取
    void close() { if(file_) fclose(file_); } // 手动释放 ← 这不是RAII！
};
```

---
### 浅拷贝和深拷贝
> **浅拷贝：只复制指针，共享数据 ——> 性能要求高，且不需要独立修改时** 
```
// 只复制指针，共享数据
vector<int>* a = new vector<int>{1,2,3};
vector<int>* b = a;  // 浅拷贝：b和a指向同一块内存
```
> **深拷贝：完全独立复制所有数据 ——> 需要完全独立的对象，避免意外修改时**
```
// 完全独立复制所有数据
vector<int> a = {1,2,3};
vector<int> b = a;  // 深拷贝：b拥有完全独立的内存副本
```
**默认拷贝构造函数是浅拷贝，要想实现深拷贝需要手动定义拷贝构造函数，且需要const + 按引用传参**
```
// 1、避免无限循环
// 错误：按值传参 → 无限递归！
Exp(const Exp exp) {    // 调用拷贝构造需要拷贝exp参数
    this->data = exp.data;  // 但拷贝exp又要调用拷贝构造...
}

// 正确：按const引用传参
Exp(const Exp& exp) {   // 不触发拷贝，直接传引用
    this->data = exp.data;
}

// 2、避免不必要的开销
// 按值传参：产生临时对象，效率低
Exp(Exp exp) { ... }  // 调用时先拷贝整个对象

// 按引用传参：零拷贝，高效  
Exp(const Exp& exp) { ... }  // 直接操作原对象，无拷贝
```