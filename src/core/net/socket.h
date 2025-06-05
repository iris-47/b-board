#pragma once

#include <netinet/in.h>
#include <unistd.h>

namespace core{
class InetAddress;

// Socket包装，提供RAII
class Socket{
private:
    const int sockfd_;
public:
    explicit Socket(int sockfd):sockfd_(sockfd){}
    ~Socket(){::close(sockfd_);}
    
    // 禁止拷贝
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int fd() const { return sockfd_; }
    
    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);
    
    // 关闭写端
    void shutdownWrite();
    
    
    void setTcpNoDelay(bool on); // 设置TCP无延迟
    void setReuseAddr(bool on); // 设置地址重用
    void setReusePort(bool on); // 设置端口重用
    void setKeepAlive(bool on); // 设置保活
};

} // namespace core