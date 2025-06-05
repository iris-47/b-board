#pragma once

#include<vector>
#include<sys/epoll.h>

#include "core/reactor/poller.h"

namespace core{
// epoll 多路复用实现
class EPollPoller : public Poller {
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;
    
    int poll(int timeout_ms, std::vector<Channel*>& active_channels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    void fillActivateChannels(int num_events, std::vector<Channel*>& active_channels) const; 
    void update(int operation, Channel* channel);

    int epoll_fd_;
    // 笔记 struct epoll_event包含events和data。epoll_ctl用于添加fd到epfd。epoll_event的效果类似于“回调函数”
    std::vector<epoll_event> ep_events_;
};

} // namespace core