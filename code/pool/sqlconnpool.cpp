#include "sqlconnpool.h"

// 单例模式
SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;
    return &connPool;
}

/*初始化连接池*/
void SqlConnPool::Init(const char* host, int port,
              const char* user, const char* pwd,
              const char* dbName, int connSize = 10) {
    assert(connSize > 0);
    for(int i = 0; i < connSize; i++) {
        // 初始化连接(分配内存，设置默认值)
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if(!sql) {
            LOG_ERROR("MySql Init error!")
            assert(sql);
        }
        // 实际连接数据库(实际建立与数据库的网络连接)
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if(!sql) {
            LOG_ERROR("MySql Connect error!")
        }
        connQue_.push(sql);
    }
    MAX_CONN_ = connSize;
    /*
    初始化信号量——信号量的值表示当前可用的连接数
    &semId_：信号量变量   0：信号量的共享位置-0表示线程间共享   MAX_CONN_：信号量初始值
    */ 
   sem_init(&semId_, 0, MAX_CONN_);
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL* sql = nullptr;
    if(connQue_.empty()) {
        LOG_WARN("SqlConnPool busy!")
        return nullptr;
    }
    // 如果semId_>0，立即减1并继续执行； 如果semId_=0，阻塞线程
    sem_wait(&semId_);
    {
        std::lock_guard<std::mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    std::lock_guard<std::mutex> locker(mtx_);
    connQue_.push(sql);
    // 增加信号量的值，并唤醒等待该信号量的一个线程(sem_wait)
    sem_post(&semId_);
}

void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> locker(mtx_);
    while(!connQue_.empty()) {
        MYSQL* item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> locker(mtx_);
    return connQue_.size();
}
