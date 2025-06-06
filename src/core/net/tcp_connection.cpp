#include "core/net/tcp_connection.h"

#include <errno.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include "core/net/channel.h"
#include "core/net/socket.h"
#include "core/reactor/event_loop.h"
#include "core/utils/logger.h"

namespace core {

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name, int sockfd,
                           const InetAddress& local_addr, const InetAddress& peer_addr)
    : loop_(loop),
      name_(name),
      state_(kConnecting),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      local_addr_(local_addr),
      peer_addr_(peer_addr),
      high_water_mark_(64 * 1024 * 1024) {  // 64MB
    
    // 设置各种回调函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));
    
    LOG_DEBUG("TcpConnection::TcpConnection [%s] fd=%d", name_, sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_DEBUG("TcpConnection::~TcpConnection [%s] fd=%d state=%d", 
              name_, channel_->fd(), state_);
    assert(state_ == kDisconnected);
}

void TcpConnection::send(const std::string& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);
        } else {
            // loop_->runInLoop(
            //     std::bind(&TcpConnection::sendInLoop, shared_from_this(), message));
            loop_->runInLoop([self = shared_from_this(), message]() {
                self->sendInLoop(message);
            });
        }
    }
}

void TcpConnection::send(const void* message, size_t len) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message, len);
        } else {
            // 复制数据，避免在发送前数据被释放
            std::string str(static_cast<const char*>(message), len);
            // loop_->runInLoop(
            //     std::bind(&TcpConnection::sendInLoop, shared_from_this(), str));
            loop_->runInLoop([self = shared_from_this(), str]() {
                self->sendInLoop(str);
            });
        }
    }
}

void TcpConnection::send(Buffer* message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message->peek(), message->readableBytes());
            message->retrieveAll();
        } else {
            std::string str = message->retrieveAllAsString();
            // loop_->runInLoop(
            //     std::bind(&TcpConnection::sendInLoop, shared_from_this(), str));
            loop_->runInLoop([self = shared_from_this(), str]() {
                self->sendInLoop(str);
            });
        }
    }
}

void TcpConnection::sendInLoop(const std::string& message) {
    sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void* message, size_t len) {
    loop_->assertInLoopThread();
    
    if (state_ == kDisconnected) {
        LOG_WARN("TcpConnection::sendInLoop [%s] disconnected, give up writing", name_);
        return;
    }
    
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    
    // 如果没有待写数据，尝试直接发送
    if (!channel_->isWriting() && output_buffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), message, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            // 如果一次发送完毕，回调写完成回调
            if (remaining == 0 && write_complete_callback_) {
                loop_->queueInLoop(
                    std::bind(write_complete_callback_, shared_from_this()));
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::sendInLoop [%s]] error: %s", name_, strerror(errno));
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultError = true;
                }
            }
        }
    }
    
    // 没有错误，且还有数据没发送完，则将数据添加到输出缓冲区，并关注可写事件
    if (!faultError && remaining > 0) {
        size_t oldLen = output_buffer_.readableBytes();
        if (oldLen + remaining >= high_water_mark_
            && oldLen < high_water_mark_
            && high_water_mark_callback_) {
            loop_->queueInLoop(
                std::bind(high_water_mark_callback_, shared_from_this(), oldLen + remaining));
        }
        output_buffer_.append(static_cast<const char*>(message) + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

// 关闭write
void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
    }
}

void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::setTcpNoDelay(bool on) {
    socket_->setTcpNoDelay(on);
}

// 建立连接时调用，用于绑定Channle和连接，并调用回调函数
void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    
    // 绑定Channel和TcpConnection的生命周期
    channel_->tie(shared_from_this());
    channel_->enableReading();
    
    // 回调连接建立回调
    if (connection_callback_) {
        connection_callback_(shared_from_this());
    }
}

void TcpConnection::connectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();
        
        // 回调连接关闭回调
        if (connection_callback_) {
            connection_callback_(shared_from_this());
        }
    }
    
    channel_->remove();
}

void TcpConnection::handleRead() {
    loop_->assertInLoopThread();
    
    int savedErrno = 0;
    ssize_t n = input_buffer_.readFd(channel_->fd(), &savedErrno);
    
    if (n > 0) {
        // 回调消息到达回调
        if (message_callback_) {
            message_callback_(shared_from_this(), &input_buffer_, n);
        }
    } else if (n == 0) {
        // 对端关闭连接
        handleClose();
    } else {
        // 出错
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead [%s] error: %s", name_, strerror(errno));
        handleError();
    }
}

void TcpConnection::handleWrite() {
    loop_->assertInLoopThread();
    
    if (channel_->isWriting()) {
        ssize_t n = ::write(channel_->fd(),
                           output_buffer_.peek(),
                           output_buffer_.readableBytes());
        
        if (n > 0) {
            output_buffer_.retrieve(n);
            
            // 如果已经发送完毕，取消关注可写事件
            if (output_buffer_.readableBytes() == 0) {
                channel_->disableWriting();
                
                // 回调写完成回调
                if (write_complete_callback_) {
                    loop_->queueInLoop(
                        std::bind(write_complete_callback_, shared_from_this()));
                }
                
                // 如果正在关闭，则关闭写端
                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handleWrite [%s] error: %s", name_, strerror(errno));
        }
    } else {
        LOG_TRACE("TcpConnection::handleWrite [%s] is down, no more writing", name_);
    }
}

void TcpConnection::handleClose() {
    loop_->assertInLoopThread();
    
    LOG_TRACE("TcpConnection::handleClose [%s] state = %d", name_, state_);
    
    assert(state_ == kConnected || state_ == kDisconnecting);
    
    // 关闭所有事件
    channel_->disableAll();
    
    // 回调关闭回调
    if (close_callback_) {
        close_callback_(shared_from_this());
    }
}

// 用LOG_ERROR输出错误信息
void TcpConnection::handleError() {
    int err = 0;
    socklen_t len = sizeof err;
    // 获取 socket 的待处理错误
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
        err = errno;
    }
    LOG_ERROR("TcpConnection::handleError [%s] - SO_ERROR = %s", name_, strerror(err));
}

} // namespace core
