#include "core/net/inet_address.h"

#include <arpa/inet.h>
#include <string.h>
#include "core/utils/logger.h"

namespace core {

InetAddress::InetAddress(uint16_t port, bool loopback) {
    memset(&addr_, 0, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = loopback ? htonl(INADDR_LOOPBACK) : htonl(INADDR_ANY);
}

InetAddress::InetAddress(const std::string& ip, uint16_t port) {
    memset(&addr_, 0, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    
    if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
        LOG_ERROR("inet_pton failed for %s", ip);
    }
}

InetAddress::InetAddress(const struct sockaddr_in& addr)
    : addr_(addr) {
}

std::string InetAddress::IP() const {
    char buf[64] = "";
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, static_cast<socklen_t>(sizeof buf));
    return buf;
}

std::string InetAddress::IP_Port() const {
    char buf[64] = "";
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, static_cast<socklen_t>(sizeof buf));
    size_t end = strlen(buf);
    snprintf(buf + end, sizeof buf - end, ":%u", ntohs(addr_.sin_port));
    return buf;
}

uint16_t InetAddress::port() const {
    return ntohs(addr_.sin_port);
}

} // namespace core
