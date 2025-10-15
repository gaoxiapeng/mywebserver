#ifndef HTTP_REQUEST_H
#define HTTP_REQUSET_H

#include <string>
#include <regex>  // 正则表达式
#include <errno.h>  // 错误号定义
#include <mysql/mysql.h>
#include <unordered_map>
#include <unordered_set>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

/*
​1、​初始化​​：创建对象时调用 Init()初始化所有成员变量。
​​2、解析请求​​：调用 parse(Buffer& buff)开始解析，按照状态逐步解析：
    - 首先解析请求行（ParseRequestLine_）
    - 然后解析请求头（ParseHeader_）
    - 最后解析请求体（ParseBody_）
​​3、数据处理​​：解析过程中填充成员变量，如 method_、path_、header_等。
​​4、结果获取​​：通过公共成员函数获取解析结果。
*/

class HttpRequest {
public: 
//枚举，定义了HTTP请求解析的不同阶段
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

// 表示服务器在处理HTTP请求时的不同状态和结果，并非标准状态码
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUSET,
        BAD_REQUSET,
        NO_RESOURSE,
        FORBIDDEND_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest() {Init();}
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);       // 从缓冲区解析HTTP请求

    std::string path() const;       // 获取请求路径
    std::string& path();
    std::string method() const;     // 获取请求方法
    std::string version() const;    // 获取HTTP版本
    std::string GetPost(const std::string& key) const;  // 获取POST参数
    std::string GetPost(const char* key) const;               // C风格
    bool IsKeepAlive() const;       // 检查是否是持久连接


private:
    // 解析相关函数
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line);

    static int ConverHex(char ch);      // 16进制字符转换为10进制
    void ParsePath_();    // 处理请求路径
    void ParsePost_();    // 解析POST表单数据 && 用户验证

    void ParseFromUrlencoded_();    // 解析URL编码的表单数据
    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);      // 用户验证函数

    PARSE_STATE state_;     // 当前解析状态
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string,std::string> header_;    // 请求头键值对：Content-Type和Content-Length
    std::unordered_map<std::string,std::string> post_;      // POST参数键值对

    static const std::unordered_map<std::string,int> DEFAULT_HTML_TAG;  // HTML标签映射
    static const std::unordered_set<std::string> DEFAULT_HTML;  // 默认HTML页面集合
};




#endif