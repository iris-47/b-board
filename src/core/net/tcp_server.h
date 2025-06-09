#pragma once

#include <atomic>
#include <map>
#include <string>
#include <memory>
#include "core/net/tcp_connection.h"

namespace core {

class Acceptor;
class EventLoop;
class EventLoopThreadPool;
class InetAddress;

// TCP服务器类
class TcpServer {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    
    enum Option {
        kNoReusePort,
        kReusePort,
    };
    
    TcpServer(EventLoop* loop, const InetAddress& listenAddr,
             const std::string& name, Option option = kNoReusePort);
    ~TcpServer();
    
    // 禁止拷贝
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    
    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback& cb) { thread_init_callback_ = cb; }
    void setConnectionChangeCallback(const TcpConnection::ConnectionCallback& cb) { connection_change_callback_ = cb; }
    void setMessageCallback(const TcpConnection::MessageCallback& cb) { message_callback_ = cb; }
    void setWriteCompleteCallback(const TcpConnection::WriteCompleteCallback& cb) { write_complete_callback_ = cb; }
    
    void start();
    
    const std::string& ipPort() const { return ip_port_; }
    const std::string& name() const { return name_; }
    EventLoop* getLoop() const { return loop_; }
    
private:
    using ConnectionMap = std::map<std::string, TcpConnection::TcpConnectionPtr>;
    
    void newConnection(int sockfd, const InetAddress& peerAddr);
    
    // 连接关闭回调
    void removeConnection(const TcpConnection::TcpConnectionPtr& conn);
    
    // 在IO线程中移除连接
    void removeConnectionInLoop(const TcpConnection::TcpConnectionPtr& conn);
    
    EventLoop* loop_;   // 主循环
    const std::string ip_port_;    // IP和端口
    const std::string name_;       // 服务器名称
    std::unique_ptr<Acceptor> acceptor_;               // 接受器，用于接受新的tcp连接
    std::unique_ptr<EventLoopThreadPool> thread_pool_; // 线程池，管理EventLoop线程。
    
    TcpConnection::ConnectionCallback connection_change_callback_;        // 连接回调, 用于设置TcpConnection类
    TcpConnection::MessageCallback message_callback_;              // 消息回调, 用于设置TcpConnection类
    TcpConnection::WriteCompleteCallback write_complete_callback_; // 写完成回调, 用于设置TcpConnection类
    ThreadInitCallback thread_init_callback_;                      // 线程初始化回调，用于设置EventLoopThreadPool类
    
    std::atomic<int> started_;     // 是否已启动
    int next_conn_id_;             // 下一个连接ID，用于将tcp连接(fd)轮流映射到各个loop，实现负载均衡
    ConnectionMap connections_;    // 连接映射，connName->shared_ptr<*Connection>
};

} // namespace core
