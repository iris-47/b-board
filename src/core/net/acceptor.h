#pragma once

#include <functional>
#include <memory>
#include "core/net/channel.h"

namespace core {

class EventLoop;
class InetAddress;
class Socket;

// 接受新连接的类
class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    
    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();
    
    // 禁止拷贝
    Acceptor(const Acceptor&) = delete;
    Acceptor& operator=(const Acceptor&) = delete;
    
    // 设置新连接回调
    void setNewConnectionCallback(const NewConnectionCallback& cb) { new_connection_callback_ = cb; }
    
    // 是否在监听
    bool listening() const { return listening_; }
    
    // 开始监听
    void listen();
    
private:
    // 处理可读事件
    void handleRead();
    
    EventLoop* loop_;           // 所属的事件循环
    std::unique_ptr<Socket> accept_socket_;  // 监听socket
    std::unique_ptr<Channel> accept_channel_;  // 监听channel
    NewConnectionCallback new_connection_callback_;  // 新连接回调
    bool listening_;            // 是否监听
    int idle_fd_;               // 空闲的文件描述符，用于应对文件描述符耗尽的情况
};

} // namespace core
