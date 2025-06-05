#pragma once

#include <map>

#include "core/net/channel.h"

namespace core {
class EventLoop;

class Poller{
public:
    Poller(EventLoop* loop):loop_(loop){}
    virtual ~Poller() = default;

    Poller(const Poller&) = delete; 
    Poller& operator=(const Poller&) = delete;

    virtual int poll(int timeout_ms, std::vector<Channel*>& active_channels) = 0; // 轮询IO事件, 返回就绪的IO数量

    // TODO：updateChannel存在的意义是啥
    virtual void updateChannel(Channel* channel) = 0; // 更新Channel
    virtual void removeChannel(Channel* channel) = 0; // 移除Channel

    virtual bool hasChannel(Channel* channel);
    static Poller* newDefaultPoller(EventLoop* loop); // 创建默认的Poller

protected:
    std::map<int, Channel*> channels_; // 存储所有的Channel对象, fd -> Channel的映射

private:
    EventLoop* loop_; // 所属的EventLoop
}; // class Poller
}  // namespace core
