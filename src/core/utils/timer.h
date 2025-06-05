#pragma once

#include <functional>
#include <chrono>
#include <set>

namespace core{
class EventLoop;

class Timer{
public:
    Timer(std::function<void()> callback, std::chrono::steady_clock::time_point when, 
        std::chrono::milliseconds interval = std::chrono::milliseconds(0));

    void run() const {callback_();}
    void restart();

    // 是否为重复Timer
    bool repeat() const { return interval_.count() > 0; } 

    // 获取expiration_
    std::chrono::steady_clock::time_point expiration() const { return expiration_; }
    
    
private:
    const std::function<void()> callback_;
    std::chrono::steady_clock::time_point expiration_; // 超时时间
    const std::chrono::microseconds interval_; // interval_ = 0表示不重复
}; // class Timer

class TimerId{
public:
    TimerId(): timer_(nullptr), timer_id_(0){}
    TimerId(Timer* timer, uint64_t seq): timer_(timer), timer_id_(seq){}

    friend class TimerManager;
private:
    Timer* timer_;
    uint64_t timer_id_;
};

class TimerManager{
public:
    TimerManager(EventLoop* loop);
    ~TimerManager();
    
    // 添加定时器
    TimerId addTimer(std::function<void()> cb, std::chrono::steady_clock::time_point when,
                     std::chrono::milliseconds interval = std::chrono::milliseconds(0));
    
    void cancel(TimerId timerId); // 取消定时器
    void processTimers();         // 处理定时器

private:
    using Entry = std::pair<std::chrono::steady_clock::time_point, Timer*>;
    using ActiveTimer = std::pair<Timer*, uint64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;
    
    // 删除指定的定时器
    void deleteTimer(Timer* timer);
    
    EventLoop* loop_;               // 所属的事件循环
    std::set<Entry> timers_;        // 按超时时间排序的定时器列表
    ActiveTimerSet active_timers_;  // 活跃的定时器集合
    uint64_t next_timer_id_;        // 下一个定时器ID
};

} // namespace core