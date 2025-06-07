#pragma once

#include <map>
#include <string>
#include <string_view>
#include <chrono>

namespace core {

// HTTP请求:METHOD PATH HTTP/VERSION (GET /index.html HTTP/1.1)
class HttpRequest {
public:
    enum Method {
        INVALID,
        GET,
        POST,
        HEAD,
        PUT,
        DELETE
    };
    
    enum Version {
        UNKNOWN,
        HTTP10,
        HTTP11
    };
    
    HttpRequest()
        : method_(INVALID),
          version_(UNKNOWN) {
    }
    
    // 设置/获取方法
    void setMethod(Method method) { method_ = method; }
    Method method() const { return method_; }
    
    // 设置/获取路径
    void setPath(const std::string& path) { path_ = path; }
    const std::string& path() const { return path_; }
    
    // 设置/获取版本
    void setVersion(Version version) { version_ = version; }
    Version version() const { return version_; }
    
    // 设置/获取HTTP头部
    void addHeader(const std::string& key, const std::string& value) { headers_[key] = value; }
    std::string getHeader(const std::string& key) const {
        auto it = headers_.find(key);
        return it == headers_.end() ? "" : it->second;
    }
    
    // 设置/获取正文
    void setBody(const std::string& body) { body_ = body; }
    const std::string& body() const { return body_; }
    
    // 重置请求
    void reset() {
        method_ = INVALID;
        version_ = UNKNOWN;
        path_.clear();
        headers_.clear();
        body_.clear();
    }
    
    // 将字符串转为方法
    static Method stringToMethod(const std::string& str) {
        if (str == "GET") return GET;
        else if (str == "POST") return POST;
        else if (str == "HEAD") return HEAD;
        else if (str == "PUT") return PUT;
        else if (str == "DELETE") return DELETE;
        else return INVALID;
    }
    
private:
    Method method_;                    // 请求方法
    std::string path_;                 // 请求路径
    Version version_;                  // HTTP版本
    std::map<std::string, std::string> headers_; // 请求头
    std::string body_;                 // 请求体
};

} // namespace core
