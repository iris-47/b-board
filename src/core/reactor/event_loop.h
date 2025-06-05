#pragma once

#include<vector>
#include<atomic>
#include<thread>
#include<memory>
#include<mutex>

#include "core/reactor/poller.h"
#include "core/utils/timer.h"

namespace core{
class EventLoop{
    using Functor = std::function<void()>;
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void loop(); // 主循环
    void quit(); // 退出主循环

    void runInLoop(Functor cb);   // 在当前循环中执行函数cb
    void queueInLoop(Functor cb); // cb放入pending_functors_队列

    void wakeup(); 

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    
    TimerManager* getTimerManager() { return &timer_manager_; }

    bool isInLoopThread() const; // 通过thread_id_判断是否在所属线程中
    void assertInLoopThread();

private:  
    void handleWakeup();  // 处理唤醒

    void doPendingFunctors(); // 执行待处理函数

    std::atomic<bool> looping_;        // 是否正在循环，仅用于保证loop()函数不会被重复调用
    std::atomic<bool> quit_;           // 是否退出循环，Opt:添加mutex,否则EventLoop析构的时候可能正在loop循环中
    std::atomic<bool> calling_pending_functors_; // 是否正在处理functor

    const std::thread::id thread_id_;  // 所属线程id
    std::unique_ptr<Poller> poller_;   
    int wakeup_fd_; // 用于唤醒
    std::unique_ptr<Channel> wakeup_channel_; // 唤醒的通道(即对应的socket，对应的文件描述符)
    std::vector<Channel*> active_channels_;   // 唤醒的通道，每次循环清空，并调用poller.poll()填充，随后执行读取

    std::mutex mutex_; // 对pending_functors的互斥锁
    std::vector<Functor> pending_functors_; // 待处理的函数

    TimerManager timer_manager_; // 定时器管理器，包含一组定时器
}; // class EventLoop
} // namespace core
