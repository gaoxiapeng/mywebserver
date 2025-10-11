#include "httprequest.h"

// 默认HTTP页面集合，用于路径补全
const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/index", "/welcome", "/video", "/picture",
    "/register", "/login",
};
// /register和/login需要区分注册和登记的业务逻辑，其余只需要补全路径
const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register", 0}, {"/login", 1},
};

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        // 1、要求显示表达HTTP版本为1.1  2、Connection模式必须是keep-alive
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

/* 一个POST方法的请求报文
POST /api/user HTTP/1.1         // 请求行
Host: example.com               // 请求头
Connection: keep-alive
Content-Type: application/x-www-form-urlencoded
Content-Length: 123

username=test%20user&password=123%40abc           // 请求体
*/
bool HttpRequest::parse(Buffer& buff) {
    // 请求报文中，每一行以CRLF-回车换行符为结尾
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    // 逐行解析报文
    while(buff.ReadableBytes() && state_ != FINISH) {
        // 提取一行报文
        /*search()：在[buff.Peak, buff.BeginWriteConst)中查找子序列[CRLF, CRLF+2)的第一次出现位置*/
        /*C风格中，char数组末尾以\0为结尾，因此CRLF在buff中表示为\r\n\0，因此lineEnd='\r'*/
        const char* lineEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd);
        switch(state_)
        {
        /*
            有限状态机，从请求行开始，每处理完后会自动转入到下一个状态    
        */
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)) {   // 解析失败
                return false;
            }
            // 若解析成功，ParseRequsetLine_()将状态转换为HEADERS
            ParsePath_();
            break;
        case HEADERS:
            ParseHeader_(line);
            // 只有CRLF或无数据(代表ParseHeader_()部分也不会跳转到BODY部分)
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        // 未找到CRLF，跳出循环，等待下次数据到来
        if(lineEnd == buff.BeginWrite()) {
            break;
        }
        buff.RetrieveUntil(lineEnd + 2);
    }
    // LOG是C风格函数，无法识别string，只能用c_str()将其转换为const char*
    LOG_ERROR("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

/*
http://www.bilibili.com/，识别到根路径"/"，最终路径变为/index.html
http://www.bilibili.com/index，识别到预定义路径"/index"，最终路径为/index.html
*/
void HttpRequest::ParsePath_() {
    // 预处理根路径
    if(path_ == "/") {
        path_ = "/index.html";
    }
    // 处理预定义路径
    else {
        for(auto &item : DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const std::string& line) {
    // 编译正则表达式模式
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    // 存储正则表达式匹配结果
    std::smatch subMatch;
    if(std::regex_match(line, subMatch, patten)) {
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    else {
        LOG_ERROR("RequestLine Error");
        return false;
    }
}

void HttpRequest::ParseHeader_(const std::string& line) {
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if(std::regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    // 结合parse来看，
    else {
        state_ = BODY;     // 遇到空行，代表HEADER部分结束，跳转到BODY
    }
}

void HttpRequest::ParseBody_(const std::string& line) {
    body_ = line;
    // 解析POST表单并进行用户验证
    ParsePost_();   
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%s", line.c_str(), line.size());
}

// 16进制转10进制
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    if(ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return ch;
}

// 处理POST请求——解析表单数据并进行用户验证(如果是POST方法的话)
void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Connect-Type"] == "application/x-www-form-urlencoded") {
        // 解析表单数据，映射到post_里
        ParseFromUrlencoded_();  
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) { 
                bool isLogin = (tag == 1);
                // 用户验证
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                }
                else {
                    path_ = "/error.html";
                }
            }
        }
    }
}

// 解析的是POST请求体中的数据(且这些数据用的是URL编码格式)，存放到post_里
void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) {
        return;
    }

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch(ch)
        {
        // '='的前面为key，后面为value
        case '=':
            key = body_.substr(j, i-j); // substr(pos, len)
            j = i + 1;
            break;
        // '&'代表一个键值对的结束
        case '&':
            value = body_.substr(j, i-j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("key:%s, value:%s", key.c_str(), value.c_str());
            break;
        // '+'解码为空格
        case '+':
            body_[i] = ' ';
            break;
        // '%HH'
        case '%':
            num = ConverHex(body_[i+1]) * 16 + ConverHex(body_[i+2]);
            // 先由16进制数转为10进制，再转为ASCII码重新存入
            // 这地方似乎有问题？不应该是将num转为ASCII码后存入吗？为什么要分开个位和十位呢？
            body_[i+1] = num / 10 + '0';
            body_[i+2] = num % 10 + '0';
            break;
        default:
            break;
        }
    }
    // 此时i=n-1，j=最后一个'&'后一个
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i-j);
         post_[key] = value;
    }
}

// 注册成功、登录成功都返回true，除此之外都返回false
bool HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool isLogin) {
    if(name == "" || pwd == "") {
        return false;
    }
    LOG_INFO("Verify name:%s, pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    // RAII技术，且此时的sql是从连接池中get的
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql);

    /*登录时，flag=false，默认登录失败； 注册时，flag=true，默认注册成功*/
    bool flag = false;
    MYSQL_RES* res = nullptr;   // 结果集，即整个TABLE内容
    MYSQL_FIELD* fields = nullptr;   // 字段，即TABLE中的表头
    unsigned int j = 0;   // 有多少字段，即表头有多少列
    char order[256] = {0};      // 放MYSQL命令的缓冲区

    if(!isLogin) {   // 注册
        flag = true;
    }
    // 该命令相当于查找TABLE-user中username对应的那一行
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("SELCET the USER'S info:%s", order);

    // mysql_query()：执行MYSQL命令成功(这里的执行成功不一定代表TABLE中有对应username)返回0，不进入for
    if(mysql_query(sql, order)) {
        mysql_free_result(res);  // 释放结果集
        return false;
    }

    // 这里的东西都是查询username成功后的信息了
    res = mysql_store_result(sql);  // 结果集
    j = mysql_num_fields(res);      // 字段长
    fields = mysql_fetch_fields(res);  // 字段

    // 进入while，说明row != NULL
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("username is %s, pwd is %s", row[0], row[1]);
        std::string password(row[1]);      // 数据库中对应的真实密码
        // 登录验证
        if(isLogin) {
            if(pwd == password) {
                flag = true;   // 密码验证成功
            }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        else {   // 能进入这个while循环且是注册，证明之前row不为空，代表已经有这个用户名了
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /*注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("register!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("Insert command is:%s", order);
        // 注册命令失败
        if(mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        // 注册成功
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!")
    /*这里包括的isLogin=true 且 username不存在的情况，直接返回flag=false*/
    return flag;
}


std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}