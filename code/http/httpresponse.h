#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <cassert>
#include <unordered_map>
#include <fcntl.h>      // 文件控制
#include <unistd.h>     // 系统调用
#include <sys/stat.h>   // 获取文件状态信息
#include <sys/mman.h>   // 内存映射文件

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(Buffer& buff);    // 生成完整HTTP响应
    void UnmapFile();       // 释放内存映射文件
    char* File();
    size_t FileLen() const;
    // 生成错误页面提示
    void ErrorContent(Buffer& buff, std::string message);
    int Code() const {
        return code_;
    }

private:
    void AddStateLine_(Buffer& buff);   // 状态行
    void AddHeader_(Buffer& buff);      // 响应头部
    void AddContent_(Buffer& buff);     // 响应内容

    void ErrorHtml_();                  // 自动选择错误页面
    std::string GetFileType_();         // 获取MIME类型

    int code_;
    bool isKeepAlive_;

    std::string path_;
    std::string srcDir_;

    // 内存映射
    char* mmFile_;
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};

#endif