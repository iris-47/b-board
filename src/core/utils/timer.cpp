#include "core/utils/timer.h"
#include "core/utils/logger.h"

namespace core {

Timer::Timer(std::function<void()> cb, std::chrono::steady_clock::time_point when, 
            std::chrono::milliseconds interval)
    : callback_(std::move(cb)),
      expiration_(when),
      interval_(interval) {
}

void Timer::restart() {
    // 更新超时时间
    if (repeat()) {
        expiration_ = std::chrono::steady_clock::now() + interval_;
    } else {
        expiration_ = std::chrono::steady_clock::time_point();
    }
}

TimerManager::TimerManager(EventLoop* loop)
    : loop_(loop),
      next_timer_id_(0) {
}

TimerManager::~TimerManager() {
    // 删除所有定时器
    for (const Entry& timer : timers_) {
        delete timer.second;
    }
}

TimerId TimerManager::addTimer(std::function<void()> cb, 
                              std::chrono::steady_clock::time_point when,
                              std::chrono::milliseconds interval) {
    auto timer = new Timer(std::move(cb), when, interval);
    
    // 添加到定时器列表
    timers_.insert(Entry(when, timer));
    
    // 添加到活跃定时器集合
    uint64_t timer_id = next_timer_id_++;
    active_timers_.insert(ActiveTimer(timer, timer_id));
    
    return TimerId(timer, timer_id);
}

void TimerManager::cancel(TimerId timerId) {
    // 从活跃定时器集合中删除
    ActiveTimerSet::iterator it = active_timers_.find(ActiveTimer(timerId.timer_, timerId.timer_id_));
    if (it != active_timers_.end()) {
        // 从定时器列表中删除
        timers_.erase(Entry(it->first->expiration(), it->first));
        
        // 删除定时器对象
        delete it->first;
        
        // 从活跃定时器集合中删除
        active_timers_.erase(it);
    }
}

// 执行所有超时触发的定时器并移除/重启定时器
void TimerManager::processTimers() {
    // 获取当前时间
    auto now = std::chrono::steady_clock::now();
    
    // 找到已经超时的定时器
    std::vector<Entry> expired;
    auto end = timers_.lower_bound(Entry(now, nullptr));
    std::copy(timers_.begin(), end, std::back_inserter(expired));
    
    // 从定时器列表中删除已超时的定时器
    timers_.erase(timers_.begin(), end);
    
    // 从活跃定时器集合中删除已超时的定时器
    for (const Entry& it : expired) {
        ActiveTimer timer(it.second, 0);
        active_timers_.erase(timer);
    }
    
    // 执行已超时的定时器回调
    for (const Entry& it : expired) {
        it.second->run();
    }
    
    // 重启需要重复的定时器
    for (const Entry& it : expired) {
        if (it.second->repeat()) {
            it.second->restart();
            timers_.insert(Entry(it.second->expiration(), it.second));
            active_timers_.insert(ActiveTimer(it.second, 0));
        } else {
            // 删除不需要重复的定时器
            delete it.second;
        }
    }
}

} // namespace core
