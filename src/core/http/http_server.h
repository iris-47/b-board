#pragma once

#include <string>
#include <functional>
#include <memory>
#include "core/net/tcp_server.h"

namespace core {

class HttpRequest;
class HttpResponse;

// HTTP服务器
class HttpServer {
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;
    
    HttpServer(EventLoop* loop, const InetAddress& listenAddr,
              const std::string& name, TcpServer::Option option = TcpServer::kNoReusePort);
    
    void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
    void setHttpCallback(const HttpCallback& cb) { http_callback_ = cb; }
    
    // 启动服务器
    void start();
    
private:
    void onConnection(const TcpConnection::TcpConnectionPtr& conn);
    void onMessage(const TcpConnection::TcpConnectionPtr& conn, Buffer* buf, size_t len);
    void onRequest(const HttpRequest& req, HttpResponse* resp);
    
    TcpServer server_;         // TCP服务器
    HttpCallback http_callback_;  // HTTP回调
};

} // namespace core