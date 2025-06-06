#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace core {

class EventLoop;

// 事件循环线程类
class EventLoopThread {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    
    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const std::string& name = std::string());
    ~EventLoopThread();
    
    // 禁止拷贝
    EventLoopThread(const EventLoopThread&) = delete;
    EventLoopThread& operator=(const EventLoopThread&) = delete;
    
    // 启动线程，返回事件循环
    EventLoop* startLoop();
    
private:
    // 线程函数
    void threadFunc();
    
    EventLoop* loop_;             // 事件循环
    bool exiting_;                // 是否退出
    std::thread thread_;          // 线程对象
    std::mutex mutex_;            // 互斥锁
    std::condition_variable cond_;  // 条件变量
    ThreadInitCallback init_callback_;   // 线程初始化回调
    std::string name_;              // 线程名
};

} // namespace core
