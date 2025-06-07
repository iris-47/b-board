#pragma once

#include <memory>
#include <string>
#include <functional>
#include <boost/any.hpp>
#include "core/net/buffer.h"
#include "core/net/inet_address.h"

namespace core {

class Channel;
class EventLoop;
class Socket;

// 笔记：std::enable_shared_from_this允许一个对象在成员函数中安全地获得指向自己（this）的 std::shared_ptr<T>，前提是它本身已被 shared_ptr 管理

class TcpConnection : public std::enable_shared_from_this<TcpConnection>{
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, size_t)>;
    using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;
    
    TcpConnection(EventLoop* loop, const std::string& name, int sockfd,
                 const InetAddress& local_addr, const InetAddress& peer_addr);
    ~TcpConnection();
    
    // 禁止拷贝
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    
    // 获取连接信息
    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return local_addr_; }
    const InetAddress& peerAddress() const { return peer_addr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }
    
    // 发送数据
    void send(const std::string& message);
    void send(const void* message, size_t len);
    void send(Buffer* message);

    // contex_相关
    void setContext(const boost::any& context) { context_ = context; }
    const boost::any& getContext() const { return context_; }
    boost::any* getMutableContext(){ return &context_; }

    // 关闭连接
    void shutdown();
    
    // 设置TCP选项
    void setTcpNoDelay(bool on);
    
    // 设置回调函数
    void setConnectionCallback(const ConnectionCallback& cb) { connection_callback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { message_callback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { write_complete_callback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { close_callback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t high_water_mark) {
        high_water_mark_callback_ = cb;
        high_water_mark_ = high_water_mark;
    }
    
    // 连接建立
    void connectEstablished();
    
    // 连接销毁
    void connectDestroyed();

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    
    void setState(StateE s) { state_ = s; }
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const std::string& message);
    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();
    
    EventLoop* loop_;   // 所属的事件循环
    const std::string name_;   // 连接名
    StateE state_;      // 连接状态
    
    // 笔记：用unique_ptr管理对象子对象，其生命周期随本类终结
    std::unique_ptr<Socket> socket_;   // Socket对象
    std::unique_ptr<Channel> channel_; // Channel对象
    const InetAddress local_addr_;     // 本地地址
    const InetAddress peer_addr_;      // 对端地址
    
    ConnectionCallback connection_callback_;         // 连接状态改变回调
    MessageCallback message_callback_;               // 消息处理回调
    WriteCompleteCallback write_complete_callback_;  // 写完成回调
    CloseCallback close_callback_;                   // 对端关闭回调
    HighWaterMarkCallback high_water_mark_callback_; // 高水位回调
    size_t high_water_mark_;                         // 高水位标记
    
    Buffer input_buffer_;   // 输入缓冲区
    Buffer output_buffer_;  // 输出缓冲区

    boost::any context_; // 用于http
};

} // namespace core