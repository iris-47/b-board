#include "core/net/socket.h"

#include <errno.h>
#include <string.h>
#include <netinet/tcp.h>

#include "core/net/inet_address.h"
#include "core/utils/logger.h"
namespace core{
void Socket::bindAddress(const InetAddress& localaddr){
    int ret = ::bind(sockfd_, localaddr.getSockAddr(), static_cast<socklen_t>(sizeof(struct sockaddr_in)));
    if(ret < 0){
        LOG_FATAL("bind [%s] failed: %s", localaddr.IP_Port(), strerror(errno));
    }
}

void Socket::listen(){
    int ret = ::listen(sockfd_, SOMAXCONN);
    if (ret < 0) {
        LOG_FATAL("listen failed: %s", strerror(errno));
    }
}

// 返回connfd
int Socket::accept(InetAddress* peeraddr){
    struct sockaddr_in addr;
    socklen_t addr_len = static_cast<socklen_t>(sizeof addr);

    int connfd = ::accept(sockfd_, (struct sockaddr*)&addr, &addr_len);

    if(connfd > 0){
        peeraddr->setSockAddr(addr);
    }else{
        LOG_ERROR("Socket::accept failed: %s", strerror(errno));
    }

    return connfd;
}

void Socket::shutdownWrite() {
    if (::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("Socket::shutdownWrite failed: %s", strerror(errno));
    }
}

// 启用或禁用 TCP-Nagle 算法(将多个小的数据包合并成一个更大的数据包发送，减少网络拥塞)
void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    if (::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
                   &optval, static_cast<socklen_t>(sizeof optval)) < 0) {
        LOG_ERROR("Socket::setTcpNoDelay failed: %s", strerror(errno));
    }
}

// 允许立即重用处于 TIME_WAIT（2*MSL时间）状态的地址, 用于服务器崩溃后快速重启（避免"Address already in use"错误）
void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    if (::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
                   &optval, static_cast<socklen_t>(sizeof optval)) < 0) {
        LOG_ERROR("Socket::setReuseAddr failed: %s", strerror(errno));
    }
}

// 允许多个套接字绑定完全相同的IP和端口组合,核会通过哈希算法将传入的连接请求均匀分配到这些套接字上，避免传统的单监听套接字+accept() 竞争问题
void Socket::setReusePort(bool on) {
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    if (::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
                   &optval, static_cast<socklen_t>(sizeof optval)) < 0) {
        LOG_ERROR("Socket::setReusePort failed: %s", strerror(errno));
    }
#else
    if (on) {
        LOG_ERROR("Socket::setReusePort is not supported");
    }
#endif
}

// 启用TCP保活机制
void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    if (::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
                   &optval, static_cast<socklen_t>(sizeof optval)) < 0) {
        LOG_ERROR("Socket::setKeepAlive failed: %s", strerror(errno));
    }
}
} // namespace core