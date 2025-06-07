#pragma once

#include <string>
#include "core/http/http_request.h"

namespace core {

class Buffer;

// HTTP请求解析器
class HttpParser {
public:
    enum ParseState {
        kExpectRequestLine,    // 期望请求行
        kExpectHeaders,        // 期望头部
        kExpectBody,           // 期望主体
        kGotAll,               // 获取全部
    };
    
    HttpParser();
    
    // 重置解析器
    void reset();
    
    // 解析请求
    bool parseRequest(Buffer* buf, std::chrono::steady_clock::time_point receiveTime);
    
    // 是否获取到完整的请求
    bool gotAll() const { return state_ == kGotAll; }
    
    // 获取解析后的请求
    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }
    
private:
    // 解析请求行
    bool parseRequestLine(const char* begin, const char* end);
    
    ParseState state_;         // 解析状态
    HttpRequest request_;      // 解析到的请求
    size_t content_length_;    // 内容长度
};

} // namespace core
