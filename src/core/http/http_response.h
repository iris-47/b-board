#pragma once

#include <map>
#include <string>
#include "core/net/buffer.h"

namespace core {

// HTTP响应
class HttpResponse {
public:
    enum StatusCode {
        kUnknown,
        k200Ok = 200,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k403Forbidden = 403,
        k404NotFound = 404,
        k500InternalServerError = 500,
    };
    
    HttpResponse()
        : status_code_(kUnknown),
          close_connection_(false) {
    }
    
    // 设置状态码
    void setStatusCode(StatusCode code) { status_code_ = code; }
    
    // 设置状态消息
    void setStatusMessage(const std::string& message) { status_message_ = message; }
    
    // 设置关闭连接
    void setCloseConnection(bool on) { close_connection_ = on; }
    
    // 是否关闭连接
    bool closeConnection() const { return close_connection_; }
    
    // 设置内容类型
    void setContentType(const std::string& type) { addHeader("Content-Type", type); }
    
    // 添加头部
    void addHeader(const std::string& key, const std::string& value) { headers_[key] = value; }
    
    // 设置主体
    void setBody(const std::string& body) { body_ = body; }
    
    // 将HttpResponse转化为实际回复的http包，并添加到output Buffer
    void appendToBuffer(Buffer* output) const;
    
private:
    StatusCode status_code_;           // 状态码
    std::string status_message_;       // 状态消息
    std::map<std::string, std::string> headers_; // 响应头
    std::string body_;                 // 响应体
    bool close_connection_;            // 是否关闭连接
};

} // namespace core
