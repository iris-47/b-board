#include "core/net/acceptor.h"

#include <cstring>
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "core/net/inet_address.h"
#include "core/net/socket.h"
#include "core/reactor/event_loop.h"
#include "core/utils/logger.h"

namespace core {

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      accept_socket_(new Socket(::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP))),
      accept_channel_(new Channel(loop, accept_socket_->fd())),
      listening_(false),
      // 笔记：/dev/null写入它的数据会被直接丢弃，读取它会立即返回 EOF（文件结束）
      idle_fd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) { 
    
    assert(idle_fd_ >= 0);
    
    // 设置端口复用
    accept_socket_->setReuseAddr(true);
    accept_socket_->setReusePort(reuseport);
    
    // 绑定地址
    accept_socket_->bindAddress(listenAddr);
    
    // 设置可读回调
    accept_channel_->setReadCallback(
        std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    accept_channel_->disableAll();
    accept_channel_->remove();
    ::close(idle_fd_);
}

void Acceptor::listen() {
    loop_->assertInLoopThread();
    listening_ = true;
    accept_socket_->listen();
    accept_channel_->enableReading();
}

void Acceptor::handleRead() {
    loop_->assertInLoopThread();
    
    InetAddress peerAddr;
    
    // 接受新连接
    int connfd = accept_socket_->accept(&peerAddr);
    
    if (connfd >= 0) {
        // 有新连接到来
        if (new_connection_callback_) {
            new_connection_callback_(connfd, peerAddr);
        } else {
            ::close(connfd);
        }
    } else {
        // 出错
        LOG_ERROR("Acceptor::handleRead - accept error: %s", strerror(errno));
        
        // 文件描述符耗尽，先关闭空闲的文件描述符，再接受连接，然后立即关闭
        // 这样做的目的是优雅应对 EMFILE 错误
        // 直接拒绝 accept()：对端客户端会收到 ECONNREFUSED，用户体验差（尤其是重试机制不完善的客户端）。
        // 立即 accept + close：对端会认为连接 短暂建立后正常关闭，行为更符合网络协议标准，客户端更容易优雅处理。
        if (errno == EMFILE) {
            ::close(idle_fd_);
            idle_fd_ = accept_socket_->accept(&peerAddr);
            ::close(idle_fd_);
            idle_fd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}

} // namespace core
