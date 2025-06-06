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
    
    // 设置线程数
    void setThreadNum(int numThreads);
    
    // 设置线程初始化回调
    void setThreadInitCallback(const ThreadInitCallback& cb) { thread_init_callback_ = cb; }
    
    // 设置连接回调
    void setConnectionCallback(const TcpConnection::ConnectionCallback& cb) { connection_callback_ = cb; }
    
    // 设置消息回调
    void setMessageCallback(const TcpConnection::MessageCallback& cb) { message_callback_ = cb; }
    
    // 设置写完成回调
    void setWriteCompleteCallback(const TcpConnection::WriteCompleteCallback& cb) { write_complete_callback_ = cb; }
    
    // 启动服务器
    void start();
    
    // 获取IP和端口
    const std::string& ipPort() const { return ip_port_; }
    
    // 获取名称
    const std::string& name() const { return name_; }
    
    // 获取事件循环
    EventLoop* getLoop() const { return loop_; }
    
private:
    using ConnectionMap = std::map<std::string, TcpConnection::TcpConnectionPtr>;
    
    // 新连接到来回调
    void newConnection(int sockfd, const InetAddress& peerAddr);
    
    // 连接关闭回调
    void removeConnection(const TcpConnection::TcpConnectionPtr& conn);
    
    // 在IO线程中移除连接
    void removeConnectionInLoop(const TcpConnection::TcpConnectionPtr& conn);
    
    EventLoop* loop_;   // 主循环
    const std::string ip_port_;    // IP和端口
    const std::string name_;       // 服务器名称
    std::unique_ptr<Acceptor> acceptor_;   // 接受器
    std::unique_ptr<EventLoopThreadPool> thread_pool_; // 线程池
    
    TcpConnection::ConnectionCallback connection_callback_;       // 连接回调
    TcpConnection::MessageCallback message_callback_;             // 消息回调
    TcpConnection::WriteCompleteCallback write_complete_callback_; // 写完成回调
    ThreadInitCallback thread_init_callback_;      // 线程初始化回调
    
    std::atomic<int> started_;     // 是否已启动
    int next_conn_id_;             // 下一个连接ID
    ConnectionMap connections_;    // 连接映射
};

} // namespace core
