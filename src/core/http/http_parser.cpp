#include "core/http/http_parser.h"

#include <algorithm>
#include <cstring>
#include "core/net/buffer.h"
#include "core/utils/logger.h"

namespace core {

HttpParser::HttpParser()
    : state_(kExpectRequestLine),
      content_length_(0) {
}

void HttpParser::reset() {
    state_ = kExpectRequestLine;
    request_.reset();
    content_length_ = 0;
}

bool HttpParser::parseRequest(Buffer* buf, std::chrono::steady_clock::time_point receiveTime) {
    bool ok = true;
    bool hasMore = true;
    
    while (hasMore) {
        if (state_ == kExpectRequestLine) {
            // 查找请求行结束符
            const char* crlf = buf->findCRLF();
            if (crlf) {
                // 解析请求行
                ok = parseRequestLine(buf->peek(), crlf);
                if (ok) {
                    // 跳过请求行
                    buf->retrieveUntil(crlf + 2);
                    // 转入解析头部阶段
                    state_ = kExpectHeaders;
                } else {
                    hasMore = false;
                }
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectHeaders) {
            // 查找头部行结束符
            const char* crlf = buf->findCRLF();
            if (crlf) {
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf) {
                    // 解析头部
                    std::string field(buf->peek(), colon);
                    ++colon;
                    // 去除中间空格
                    while (colon < crlf && isspace(*colon)) {
                        ++colon;
                    }
                    std::string value(colon, crlf);
                    // 去除尾部空格
                    while (!value.empty() && isspace(value[value.size() - 1])) {
                        value.resize(value.size() - 1);
                    }
                    request_.addHeader(field, value);
                    
                    // 检查Content-Length
                    if (field == "Content-Length") {
                        content_length_ = std::stoul(value);
                    }
                } else {
                    // 空行，头部结束
                    if (content_length_ > 0) {
                        // 转入解析主体阶段
                        state_ = kExpectBody;
                    } else {
                        // 没有主体，解析完成
                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                // 跳过当前行
                buf->retrieveUntil(crlf + 2);
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectBody) {
            // 检查是否有足够的数据
            if (buf->readableBytes() >= content_length_) {
                // 解析主体
                request_.setBody(buf->retrieveAsString(content_length_));
                // 解析完成
                state_ = kGotAll;
                hasMore = false;
            } else {
                hasMore = false;
            }
        }
    }
    
    return ok;
}

bool HttpParser::parseRequestLine(const char* begin, const char* end) {
    // 查找空格分隔符
    const char* space = std::find(begin, end, ' ');
    if (space == end) {
        return false;
    }
    
    // 解析请求方法
    std::string method_str(begin, space);
    HttpRequest::Method method = HttpRequest::stringToMethod(method_str);
    if (method == HttpRequest::INVALID) {
        return false;
    }
    request_.setMethod(method);
    
    // 跳过空格
    begin = space + 1;
    
    // 查找下一个空格
    space = std::find(begin, end, ' ');
    if (space == end) {
        return false;
    }
    
    // 解析请求路径
    request_.setPath(std::string(begin, space));
    
    // 跳过空格
    begin = space + 1;
    
    // 解析HTTP版本
    if (end - begin != 8) {
        return false;
    }
    if (strncmp(begin, "HTTP/1.", 7) != 0) {
        return false;
    }
    if (*(begin + 7) == '0') {
        request_.setVersion(HttpRequest::HTTP10);
    } else if (*(begin + 7) == '1') {
        request_.setVersion(HttpRequest::HTTP11);
    } else {
        return false;
    }
    
    return true;
}

} // namespace core
