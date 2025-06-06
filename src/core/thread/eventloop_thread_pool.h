#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace core {

class EventLoop;
class EventLoopThread;

// 事件循环线程池
class EventLoopThreadPool {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    
    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();
    
    // 禁止拷贝
    EventLoopThreadPool(const EventLoopThreadPool&) = delete;
    EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;
    
    // 设置线程数量
    void setThreadNum(int numThreads) { num_threads_ = numThreads; }
    
    // 启动线程池
    void start(const ThreadInitCallback& cb = ThreadInitCallback());
    
    // 获取下一个事件循环
    EventLoop* getNextLoop();
    
    // 获取所有事件循环
    std::vector<EventLoop*> getAllLoops();
    
    // 是否已启动
    bool started() const { return started_; }
    
    // 获取名称
    const std::string& name() const { return name_; }
    
private:
    EventLoop* base_loop_;    // 初始loop, Acceptor运行在该loop上
    std::string name_;        // 名称
    bool started_;            // 是否已启动
    int num_threads_;         // 线程数量
    int next_;                // 下一个要分配的线程索引
    std::vector<std::unique_ptr<EventLoopThread>> threads_;  // 线程列表
    std::vector<EventLoop*> loops_;                          // EventLoop列表
};

} // namespace core
