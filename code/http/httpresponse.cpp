#include "httpresponse.h"

/*
HTTP/1.1 200 OK                             // 状态行
Date: Fri, 22 May 2009 06:07:21 GMT         // 响应头
Connection: keep-alive
keep-alive: max=6, timeout=120
Content-Type: text/html
Content-Length: 10
\r\n
<html>                                      // 响应正文
      <head></head>
      <body>
            <!--body goes here-->
      </body>
</html>
*/

// 文件后缀到MIME(用于描述消息内容类型)类型映射 —— 根据文件拓展名确定Content-Type响应头
const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

// 状态码到状态文本的映射 —— 用于构建状态行
const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

// 状态码到错误页面路径的映射 —— 提供错误提示页面
const std::unordered_map<int, std::string> HttpResponse:: CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = "";
    srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

HttpResponse::~HttpResponse() {
    UnmapFile();    // 释放内存映射文件
}

// 这里的path和ErrorHtml中的path不一样
void HttpResponse::Init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if(mmFile_) {
        UnmapFile();
    }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

// 此处的path还是request传进来的值，即想要访问的页面
void HttpResponse::MakeResponse(Buffer& buff) {
    // stat(需要查看数据的文件路径的指针， stat结构体的指针)，文件属性就记录在结构体(mmFileStat_)中，成功返回0，失败返回1
    // mmFileStat_.st_mode：文件对应的模式(文件类型、文件权限)
    // S_ISDIR(st_mode)：判断是不是目录
    /*文件不存在或无法访问  或者  路径指向的是目录而非文件*/
    if(stat((srcDir_ + path_).data(), &mmFileStat_ ) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    }
    // S_IROTH：其他人可读
    // mmFileStat_.st_mode & S_IROTH = true ：判断所有者对该文件有可读权限
    else if(!(mmFileStat_.st_mode & S_IROTH)) {     // 无可读权限
        code_ = 403;
    }
    else if(code_ == -1) {
        code_ = 200;
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

/*如果错误码code_ = 200，那么path_不变，还是原来申请的文件地址*/
void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

// HTTP版本 + 状态码 + 状态文本
void HttpResponse::AddStateLine_(Buffer& buff) {
    std::string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        code_ = 400;
        status = CODE_STATUS.find(code_)->second;
    }
    buff.Append("HTTP/1.1" + std::to_string(code_) + " " + status + "\r\n");
}

// Connection + Content-Type
void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: ");
    if(isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.Append("close\r\n");
    }
    buff.Append("Content-Type: " + GetFileType_() + "\r\n");
}

// 文件映射 + 结束响应体头部(Cotent-Length)
void HttpResponse::AddContent_(Buffer& buff) {
    // 以只读模式(O_RDONLY)打开文件，打开失败则返回负值
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) {
        ErrorContent(buff, "File NotFound!");
        return;
    }

    /* 将文件映射到内存提高文件的访问速度 
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    LOG_DEBUG("file path: %s", (srcDir_ + path_).data());
    // mmRet：内存映射地址
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {      // 内存映射失败
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char*) mmRet;
    close(srcFd);

    buff.Append("Content-Length: " + std::to_string(mmFileStat_.st_size) + "\r\n");
    buff.Append("\r\n");   // 结束头部
}

// 安全释放内存映射资源
void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);     // 地址，长度
    }
    mmFile_ = nullptr;
}

std::string HttpResponse::GetFileType_() {
    // size_type：无符号整型，用来表示字符串长度或索引位置
    std::string::size_type idx = path_.find_last_of('.');
    if(idx == std::string::npos) {
        return "text/plain";   // 文件无后缀则默认纯文本形式
    }
    std::string suffix = path_.substr(idx); // 直接从idx截到最后
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";    // 未知扩展名的默认处理
}

void HttpResponse::ErrorContent(Buffer& buff, std::string message) {
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += std::to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-Length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}