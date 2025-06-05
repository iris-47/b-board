#pragma once

#include <netinet/in.h>
#include <string>

namespace core
{
// IPv4地址类
class InetAddress{
private:
    struct sockaddr_in addr_;


public:
    // 笔记：explicit 禁止编译器进行隐式类型转换，只允许显式调用。避免意外的类型转换导致代码行为不清晰或潜在错误。别和volatile搞混了
    explicit InetAddress(uint16_t port = 0, bool loopback = false);
    InetAddress(const std::string& ip, uint16_t port);
    explicit InetAddress(const struct sockaddr_in& addr);

    std::string IP() const;       // 获取IP
    std::string IP_Port() const;  // 获取IP:port
    uint16_t port() const;        // 获取port

    // 将类内部的地址结构体sockaddr_in转换为通用的 const sockaddr* 类型指针
    const struct sockaddr* getSockAddr() const{
        // 笔记： const 用于函数名后表示该函数不会修改成员变量，该函数也只能调用同样有const修饰的成员函数
        // 笔记： reinterpret_cast进行低级别的二进制重新解释（如指针 → 整数、不同类型指针互转），即不检查，类似强制类型转换
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }

    void setSockAddr(const struct sockaddr_in& addr) { addr_ = addr; }
};
} // namespace core
