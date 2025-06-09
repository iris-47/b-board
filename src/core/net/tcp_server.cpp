#include "core/net/tcp_server.h"

#include <stdio.h>
#include <cstring>
#include "core/net/acceptor.h"
#include "core/net/inet_address.h"
#include "core/net/socket.h"
#include "core/thread/eventloop_thread_pool.h"
#include "core/reactor/event_loop.h"
#include "core/utils/logger.h"

namespace core {

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr,
                   const std::string& name, Option option)
    : loop_(loop),
      ip_port_(listenAddr.IP_Port()),
      name_(name),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      thread_pool_(new EventLoopThreadPool(loop, name)),
      connection_change_callback_(),
      message_callback_(),
      write_complete_callback_(),
      thread_init_callback_(),
      started_(0),
      next_conn_id_(1) {
    
    // 设置Acceptor的新连接回调，区别于TcpServer的新连接回调，该回调主要
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer() {
    loop_->assertInLoopThread();
    
    LOG_TRACE("TcpServer::~TcpServer [{}] destructing", name_);
    
    for (auto& item : connections_) {
        TcpConnection::TcpConnectionPtr conn(item.second);
        item.second.reset();
        
        // 在连接所属的线程中销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads) {
    assert(numThreads >= 0);
    thread_pool_->setThreadNum(numThreads);
}

void TcpServer::start() {
    if (started_.fetch_add(1) == 0) {
        thread_pool_->start(thread_init_callback_);
        
        // 在事件循环中启动acceptor
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 用于Acceptor的连接回调，为新连接创建TcpConnection对象并设置关闭回调
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    loop_->assertInLoopThread();
    
    // 获取一个IO线程来管理这个连接
    EventLoop* ioLoop = thread_pool_->getNextLoop();
    
    // 创建连接名
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ip_port_.c_str(), next_conn_id_);
    ++next_conn_id_;
    std::string connName = name_ + buf;
    
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from {%s}", 
             name_, connName, peerAddr.IP_Port());
    
    // 获取本地地址
    InetAddress localAddr;
    socklen_t addrlen = static_cast<socklen_t>(sizeof(struct sockaddr_in));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    // getsockname:获取一个已建立连接的套接字的本地地址信息
    // getpeername:获取一个已建立连接的套接字的对端地址信息
    if (::getsockname(sockfd, (struct sockaddr*)&addr, &addrlen) < 0) {
        LOG_ERROR("TcpServer::newConnection - getsockname failed: {%s}", strerror(errno));
    }
    localAddr.setSockAddr(addr);
    
    // 创建TcpConnection对象
    TcpConnection::TcpConnectionPtr conn = std::make_shared<TcpConnection>(
        ioLoop, connName, sockfd, localAddr, peerAddr);
    
    // 添加到连接映射
    connections_[connName] = conn;
    
    // 设置回调函数
    conn->setConnectionChangeCallback(connection_change_callback_);
    conn->setMessageCallback(message_callback_);
    conn->setWriteCompleteCallback(write_complete_callback_);
    
    // 设置关闭回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    
    // 在IO线程中建立连接
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnection::TcpConnectionPtr& conn) {
    // 调用removeConnectionInLoop
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnection::TcpConnectionPtr& conn) {
    loop_->assertInLoopThread();
    
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s", 
             name_, conn->name());
    
    // 从TcpServer的连接映射中移除
    size_t n = connections_.erase(conn->name());
    assert(n == 1);
    
    // 在连接所属的线程中销毁连接
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}

} // namespace core
