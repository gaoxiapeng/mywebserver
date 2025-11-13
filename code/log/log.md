### 异步写日志的流程
**输入format(也就是写入日志的内容后) ——> 先将这些内容附加上时间、等级等信息，存放到buffer中 ——> 然后将buffer中的内容push_back到日志队列deque中 ——> 接着就是由消费者线程(写线程writeThread)将其写入日志文件中**

---
单例模式：一个类只有一个实例，并提供一个全局访问点。单例模式就是将构造函数和析构函数放到private中（私有化构造函数），使得外部无法访问构造函数，那么自然无法手动实例化对象了。且删除了拷贝构造和幅值操作

## static关键字

**静态成员变量(类内)**
```
class Student {
private:
    static int totalStudents;  // 静态成员变量声明
    string name;
    
public:
    Student(string n) : name(n) {
        totalStudents++;  // 每创建一个学生，总数+1
    }
    
    static int getTotal() {
        return totalStudents;
    }
};

// 静态成员变量必须在类外定义（分配内存）
int Student::totalStudents = 0;  // 初始化
``` 
```
int main() {
    cout << "初始学生数: " << Student::getTotal() << endl;  // 输出: 0
    
    Student s1("Alice");
    Student s2("Bob");
    Student* s3 = new Student("Charlie");
    
    cout << "当前学生数: " << Student::getTotal() << endl;  // 输出: 3
    
    // 所有对象共享同一个totalStudents
    cout << s1.getTotal() << endl;  // 3
    cout << s2.getTotal() << endl;  // 3
    cout << Student::getTotal() << endl;  // 3
    
    delete s3;
    return 0;
}
```

· 属于类，所有对象共享一份数据
· 类外初始化
· 生命周期贯穿整个程序

**静态成员函数(类内)**
```
class MathUtils {
private:
    static const double PI;  // 静态常量
    
public:
    // 静态成员函数 - 工具函数
    static double circleArea(double radius) {
        return PI * radius * radius;
    }
    
    static double degreesToRadians(double degrees) {
        return degrees * PI / 180.0;
    }
    
    // 不能访问非静态成员！
    // static void error() { cout << name; }  // 错误
};

const double MathUtils::PI = 3.1415926;
```
```int main() {
    // 不需要创建对象，直接通过类名调用
    double area = MathUtils::circleArea(5.0);
    double radians = MathUtils::degreesToRadians(90);
    
    cout << "圆面积: " << area << endl;
    cout << "90度=" << radians << "弧度" << endl;
    
    return 0;
}

```
· 没有this指针，且静态成员函数只能访问静态变量
· 不需要创建对象，可以直接通过类名调用

**静态局部变量(函数内)**
```
void counter() {
    static int count = 0;  // 只初始化一次！
    count++;
    cout << "函数被调用了 " << count << " 次" << endl;
}

int main() {
    counter();  // 第1次调用
    counter();  // 第2次调用  
    counter();  // 第3次调用
    // 输出: 1, 2, 3
    return 0;
}
```
· 只初始化一次，保持值的永久性
· 作用域仅局限于函数内，但生命周期是全局的

---
## 实例化一个对象实际上分两步：
    - 内存分配
    - 调用构造函数


## 单例模式：确保一个类只有一个实例，并提供全局访问点
> **单例模式是指在整个系统生命周期内，保证一个类只能产生一个实例，确保该类的唯一性。**

> **单例模式的全局访问点静态，是因为这样就不需要先创建对象，而是直接通过类名调用   `Log* log = Log::Instance();`**

· **构造函数和析构函数为私有类型，目的是禁止外部构造和析构。**
· **拷贝构造函数和赋值构造函数是私有类型，禁止外部拷贝和赋值，确保唯一性。**
· **类中有一个获取实例的静态方法，可以全局访问。(全局静态访问点)**

```
class S
{
public:
    // 全局访问点 - 获取单例实例
    static S& getInstance()
    {
        static S instance;  // 局部静态变量：线程安全，首次调用时创建
        return instance;    // 返回实例的引用
    }

    // C++11 方式：明确删除拷贝操作（推荐）
    S(S const&) = delete;
    void operator=(S const&) = delete;

private:
    S() {}  // 私有构造函数：防止外部创建实例
    
    // 注：Scott Meyers 建议将 deleted 函数放在 public 区域
    // 这样编译器会先检查可访问性，再检查删除状态，产生更好的错误信息
};
```
> **局部作用域 vs 全局作用域**
```
// ❌ 全局静态变量（传统饿汉模式）
class TraditionalSingleton {
private:
    static S instance;  // 全局静态，程序启动就创建
public:
    static S& getInstance() { return instance; }
};
// 必须在类外定义：S TraditionalSingleton::instance;

// ✅ 局部静态变量（Meyers' Singleton）
class MeyersSingleton {
public:
    static S& getInstance() {
        static S instance;  // 局部静态，首次调用时创建
        return instance;
    }
};
```

### 懒汉模式 vs 饿汉模式
> **懒汉模式**
```
class LazySingleton {
public:
    static LazySingleton* getInstance() {
        static LazySingleton instance;  // 首次调用时创建
        return &instance;               // 返回指针
    }
    
    LazySingleton(const LazySingleton&) = delete;
    void operator=(const LazySingleton&) = delete;

private:
    LazySingleton() = default;
};
```

> **饿汉模式**
```
class HungrySingleton {
public:
    static HungrySingleton* getInstance() {
        return instance;  // 直接返回已创建的实例
    }
    
    HungrySingleton(const HungrySingleton&) = delete;
    void operator=(const HungrySingleton&) = delete;

private:
    HungrySingleton() = default;
    static HungrySingleton* instance;  // 声明静态实例
};

// 程序启动前初始化
HungrySingleton* HungrySingleton::instance = new HungrySingleton();
```

---

**同步写日志：边生成边写入 —— 日志产生后立即写入磁盘文件**
**异步写日志：先缓存后批量写入 —— 日志先放入内存队列，后台线程批量写入**


写日志流程：先`fopen`打开日志文件 ——> 接着`fputs`将内容写入FILE*缓冲区(内存) ——> 当FILE*缓冲区满或定时自动处理，将日志内容写入内核缓冲区；或者手动`fflush`强制从FILE缓冲区刷到内核缓冲区 ——> `fsync`强制从内核缓冲区刷到磁盘中 ——> `fclose`关闭文件


---
## 阻塞队列（BlockDeque）代码总结

> 条件变量是多线程编程中的高效同步工具，它解决了线程间"等待-通知"的协作问题。与互斥锁配合使用，条件变量让线程能够在条件不满足时主动休眠（而非忙等待），当其他线程修改条件后通过notify操作唤醒等待线程。这种机制的核心优势在于避免了CPU空转，实现了线程间的精确同步。典型应用包括生产者-消费者模式，其中生产者在线程队列未满时插入数据并通过条件变量通知消费者，消费者在线程队列为空时等待通知，这种"等待条件成立-执行业务-通知他人"的工作模式构成了多线程协作的基础范式。

该阻塞队列基于 `std::deque` 实现，利用互斥锁和条件变量来保证多线程环境下的数据安全访问。其核心逻辑是通过 **生产者-消费者模型** 来协调数据的存取，确保线程间同步与资源互斥。

在设计上，队列提供了固定容量，生产者在队列已满时会被阻塞，消费者在队列为空时同样会被阻塞。条件变量在合适的时机被唤醒，从而让线程继续执行，避免了资源竞争和忙等。  
关闭队列时会清空数据，并唤醒所有等待中的线程，使得生产者和消费者可以感知队列的结束状态，从而安全退出。

整体流程可以总结为：
- **生产者线程** 调用 `push_back` 或 `push_front` 插入数据，若队列已满则等待，直到有消费者取走数据。
- **消费者线程** 调用 `pop` 获取数据，若队列为空则等待，直到有生产者插入数据。
- **互斥锁** 保证队列操作的原子性，**条件变量** 保证线程间的同步。
- **关闭操作** 则负责中止队列运行，唤醒所有阻塞线程，确保程序能够正确收尾。

该实现体现了阻塞队列在多线程环境下的典型应用：在固定容量限制下通过同步机制实现线程安全的生产与消费，保证了数据处理的有序性与一致性。

# C++继承、多态

## 多态（Polymorphism）

在C++中，多态是指通过基类的指针或引用调用虚函数时，能够根据实际对象类型执行不同的函数实现。这是面向对象编程的核心特性之一，主要通过`virtual`关键字实现。

多态分为两种：
- 编译时多态（静态绑定）：通过函数重载和模板实现
- 运行时多态（动态绑定）：通过虚函数实现

## 虚函数（Virtual Function）

虚函数通过在基类中使用`virtual`关键字声明，允许派生类重写该函数：
```
class Base() {
public:
    virtual void show() {
        cout << "Base show" << endl;
    }
    void say() {
        cout << 1 << endl;
    }
};
```
```
class Derived() : public Base{   // 继承
public:
    void show() {
        cout << "Derived show" << endl;
    }
    void say() {
        cout << 2 << endl;
    }
};
```
> **当通过基类指针调用虚函数时，实际调用的是对象实际类型的函数版本：**
```
Base* b = new Derived();   // 基类指针指向派生类对象
b->show(); // 调用Derived::show()
b->say();  // 调用Base::say()
```

## 纯虚函数（Pure Virtual Function）

纯虚函数是在基类中声明但没有实现的虚函数，使用`= 0`语法：
```
class AbstractBase {
public:
virtual void mustImplement() = 0;
};
```
包含纯虚函数的类称为抽象类，不能实例化。派生类必须实现所有纯虚函数才能实例化。

## 虚析构函数（Virtual Destructor）

虚析构函数确保通过基类指针删除派生类对象时，能正确调用派生类的析构函数：
```
class Base {
public:
virtual ~Base() {} // 虚析构函数
};
```
**如果没有虚析构函数，通过基类指针删除派生类对象会导致只调用基类析构函数，而派生类析构函数不被调用，可能造成内存泄漏。**

## 纯虚析构函数（Pure Virtual Destructor）

纯虚析构函数是一种特殊的纯虚函数，它使类成为抽象类，但必须提供实现：

```
class AbstractBase {
public:
virtual ~AbstractBase() = 0;
};
AbstractBase::~AbstractBase() {} // 必须提供实现
```

纯虚析构函数主要用于：
1. 使类成为抽象类
2. 强制派生类实现自己的析构函数
3. 仍然保证析构链的正常执行

## 总结

1. 多态通过虚函数实现运行时动态绑定
2. 纯虚函数定义接口规范，强制派生类实现
3. 虚析构函数确保对象完整销毁
4. 纯虚析构函数使类抽象化同时保证析构安全


