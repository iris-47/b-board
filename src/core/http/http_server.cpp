#include "core/http/http_server.h"

#include "core/http/http_parser.h"
#include "core/http/http_request.h"
#include "core/http/http_response.h"
#include "core/net/tcp_connection.h"
#include "core/utils/logger.h"

namespace core {

// 存储在TcpConnection中的上下文
class HttpContext {
public:
    HttpParser parser;
    
    // 重置解析器
    void reset() {
        parser.reset();
    }
};

HttpServer::HttpServer(EventLoop* loop, const InetAddress& listenAddr,
                     const std::string& name, TcpServer::Option option)
    : server_(loop, listenAddr, name, option),
      http_callback_([this](const HttpRequest& req, HttpResponse* resp) {
          onRequest(req, resp);
      }) {
    
    // 设置回调函数
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void HttpServer::start() {
    LOG_INFO("HttpServer[%s] starts listening on %s", server_.name(), server_.ipPort());
    server_.start();
}

void HttpServer::onConnection(const TcpConnection::TcpConnectionPtr& conn) {
    if (conn->connected()) {
        // 创建上下文
        conn->setContext(std::make_shared<HttpContext>());
    }
}

void HttpServer::onMessage(const TcpConnection::TcpConnectionPtr& conn, Buffer* buf, size_t len) {
    auto context = boost::any_cast<std::shared_ptr<HttpContext>>(conn->getContext());
    
    // 解析请求
    if (!context->parser.parseRequest(buf, std::chrono::steady_clock::now())) {
        // 解析失败
        LOG_ERROR("HttpServer::onMessage - Bad Request");
        
        // 返回400
        HttpResponse response;
        response.setStatusCode(HttpResponse::k400BadRequest);
        response.setStatusMessage("Bad Request");
        response.setCloseConnection(true);
        
        Buffer buf;
        response.appendToBuffer(&buf);
        conn->send(&buf);
        conn->shutdown();
    }
    
    if (context->parser.gotAll()) {
        // 处理请求
        HttpResponse response;
        onRequest(context->parser.request(), &response);
        
        // 发送响应
        Buffer buf;
        response.appendToBuffer(&buf);
        conn->send(&buf);
        
        // 如果是HTTP/1.0或者需要关闭连接，则关闭连接
        if (response.closeConnection()) {
            conn->shutdown();
        }
        
        // 重置解析器，准备解析下一个请求
        context->reset();
    }
}

void HttpServer::onRequest(const HttpRequest& req, HttpResponse* resp) {
    LOG_INFO("HttpServer::onRequest - %s %s", 
            req.method() == HttpRequest::GET ? "GET" : 
            req.method() == HttpRequest::POST ? "POST" : "OTHER",
            req.path());
    
    // 调用用户设置的回调函数
    if (http_callback_) {
        http_callback_(req, resp);
    } else {
        // 默认响应
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setCloseConnection(true);
    }
}

} // namespace core
