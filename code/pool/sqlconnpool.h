#ifndef SQLCONNPOOL.H
#define SQLCONNPOOL.H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>     // 信号量操控
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool* Instance();

    // 初始化：数据库地址、端口、用户名、密码、数据库名、连接数
    void Init(const char* host, int port,
              const char* user, const char* pwd,
              const char* dbName, int connSize);

    // 获取连接
    MYSQL* GetConn();
    // 归还连接
    void FreeConn(MYSQL* conn);
    // 关闭连接池
    void ClosePool();

    int GetFreeConnCount();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;      // 连接池容量
    int useCount_;      // 当前使用的连接数
    int freeCount_;     // 当前空闲的连接数

    std::queue<MYSQL*> connQue_;        // 存储可用连接的队列
    std::mutex mtx_;
    sem_t semId_;       // 信号量——记录可用连接数(阻塞式获取连接)
};

/*资源在对象构造时初始化，在对象析构时释放*/
/*构造时从连接池中获取连接，析构时自动归还连接*/
class SqlConnRAII {
public:
    // 这里用二级指针，考虑传递参数只是变量时，如果我们想修改该变量，那么我们需要传入其指针或引用，这里我们想修改的是一级指针，所以需要传进去二级指针
    SqlConnRAII(MYSQL** sql, SqlConnPool* connpool) {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    ~SqlConnRAII() {
        if(sql_) {
            connpool_->FreeConn(sql_);
        }
    }

private:
    // 从连接池获取的数据库连接指针
    MYSQL* sql_;
    SqlConnPool* connpool_;
};

#endif