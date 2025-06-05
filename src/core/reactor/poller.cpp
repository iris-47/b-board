#include "core/reactor/poller.h"
#include "core/reactor/epoll_poller.h"

// TODO: add APPLE support
// TODO: add more support like IoUring
namespace core {
bool Poller::hasChannel(Channel* channel){
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

Poller* Poller::newDefaultPoller(EventLoop* loop){
    return new EPollPoller(loop);
}
}