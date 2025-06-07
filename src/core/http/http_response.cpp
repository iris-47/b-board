#include "core/http/http_response.h"

#include <stdio.h>

namespace core {

// 将HttpResponse转化为实际回复的http包，并添加到output Buffer
void HttpResponse::appendToBuffer(Buffer* output) const {
    // 一个标准Response示例如下：
    // HTTP/1.1 200 OK
    // Content-Type: application/json
    // Content-Length: 190

    // {
    // "code": 200,
    // "msg": "{\"partialKey\":\"...\",\"finalPublicKey\":\"...\"}"
    // }
    // 状态行
    char buf[32];
    snprintf(buf, sizeof buf, "HTTP/1.1 %d ", status_code_);
    output->append(buf);
    output->append(status_message_);
    output->append("\r\n");
    
    // 响应头
    if (close_connection_) {
        output->append("Connection: close\r\n");
    } else {
        snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", body_.size());
        output->append(buf);
        output->append("Connection: Keep-Alive\r\n");
    }
    
    for (const auto& header : headers_) {
        output->append(header.first);
        output->append(": ");
        output->append(header.second);
        output->append("\r\n");
    }
    
    // 空行
    output->append("\r\n");
    
    // 响应体
    output->append(body_);
}

} // namespace core
