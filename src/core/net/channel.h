#pragma once

#include <functional>
#include <memory>
#include <poll.h>


namespace core {

class EventLoop;

// Channel负责一个文件描述符的IO事件分发，一个Channel对象对应一个文件描述符
// 封装了向Poller中注册的文件描述符fd，感兴趣的事件events、Poller返回的发生的事件revents，和一组能够根据fd发生的事件revents进行回调的回调函数callbacks
// 共有两种Channel，一种是listenfd - acceptorChannel，一种是connfd - connectionChannel
class Channel {
using EventCallback = std::function<void()>;
public:
    Channel(EventLoop* loop, int fd);
    ~Channel();

    Channel(const Channel&) = delete; // 禁止拷贝构造
    Channel& operator=(const Channel&) = delete; // 禁止拷贝赋值

    void handleEvent(); // 处理事件

    // 设置事件回调
    // 笔记：不使用void setReadCallback(const EventCallback& cb) { read_callback_ = cb; }的原因：避免拷贝
    // 笔记：std::move 将一个左值转换为右值引用，并清空原值，从而允许调用移动构造函数或移动赋值运算符，而不是拷贝构造函数或拷贝赋值运算符。
    void setReadCallback(EventCallback cb) { read_callback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { write_callback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { close_callback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { error_callback_ = std::move(cb); }

    // 关注/取消关注事件
    void enableReading() {events_ |= (POLLIN | POLLPRI); update();}
    void disableReading() {events_ &= ~(POLLIN | POLLPRI); update();}
    void enableWriting() {events_ |= POLLOUT; update();}
    void disableWriting() {events_ &= ~POLLOUT; update();}
    void disableAll() {events_ = 0; update();}

    // 是否关注事件
    bool isReading() const { return events_ & (POLLIN | POLLPRI); }
    bool isWriting() const { return events_ & POLLOUT; }
    bool isNoneEvent() const { return events_ == 0; } // 当前Channel没有关注任何事件

    int events() const { return events_; } // 获取关注的事件
    void set_revents(int revents) { revents_ = revents; } // 设置发生的事件

    int fd() const { return fd_; }                    // 获取文件描述符
    int status() const { return status_; }            // 获取状态
    void set_status(int status) { status_ = status; } // 设置状态
    EventLoop* ownerLoop() { return loop_; }          // 获取所属的EventLoop

    void tie(const std::shared_ptr<void>& obj); // 绑定当前处理的对象，防止obj被其它线程析构

    void remove(); // 从EventLoop中移除自己

private:
    void update(); // 更新Channel的事件

    EventLoop* loop_; // 所属的EventLoop
    const int fd_;    // 绑定的文件描述符
    int events_;      // 关注的事件
    int revents_;     // 发生的事件
    int status_;      // 状态，包括kNew, kAdded, kDeleted

    bool event_handling_;     // 是否正在处理事件
    bool tied_;               // 是否绑定了一个对象
    std::weak_ptr<void> tie_; // 绑定的对象, 用于防止多线程中的意外析构

    EventCallback read_callback_;  // 可读事件回调
    EventCallback write_callback_; // 可写事件回调
    EventCallback close_callback_; // 关闭事件回调
    EventCallback error_callback_; // 错误事件回调
}; // class Channel
}  // namespace core