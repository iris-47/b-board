#include "core/reactor/epoll_poller.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "core/utils/logger.h"

#define NEW_POLLER -1    // 该Channel不属于Poller，未监听
#define ADDED_POLLER 1   // 该Channel属于Poller，启动监听
#define DELETED_POLLER 2 // 该Channel属于Poller，未监听

namespace core{
EPollPoller::EPollPoller(EventLoop* loop)
    // 笔记：EPOLL_CLOEXEC创建的 epoll 文件描述符，会在 exec() 时自动关闭。
    :Poller(loop), epoll_fd_(epoll_create1(EPOLL_CLOEXEC)),
     ep_events_(kInitEventListSize){
    if(epoll_fd_ < 0){
        LOG_FATAL("epoll_create error: %s", strerror(errno));
    }
}

EPollPoller::~EPollPoller(){
    ::close(epoll_fd_);
}

// 调用epoll_wait获取就绪io，返回就绪io数量
int EPollPoller::poll(int timeout_ms, std::vector<Channel*>& active_channels){
    LOG_TRACE("poll fd total count: %d", channels_.size());

    // 笔记：.data()返回指向vector内部数组的裸指针T*。适用于需要传递原始指针的 C 接口或高性能场景（如 memcpy、epoll_wait 等）
    // 笔记：static_cast编译时完成类型转换，不进行检查；dynamic_cast运行时检查。向下转型推荐dynamic_cast
    int num_events = epoll_wait(epoll_fd_, ep_events_.data(), ep_events_.size(), timeout_ms);
    int saved_errno = errno;

    if(num_events > 0){
        LOG_TRACE("poll %d events happened", num_events);
        for (int i = 0; i < num_events; ++i) {
            Channel* channel = static_cast<Channel*>(ep_events_[i].data.ptr);
            channel->set_revents(ep_events_[i].events);
            active_channels.push_back(channel);
        }
        
        // 事件列表会被充满，进行扩容
        if(ep_events_.size() == num_events){
            ep_events_.resize(num_events*2);
        }
    }else if(num_events == 0){
        LOG_TRACE("poll nothing happend");
    }else{
        // 笔记：EINTR	调用被信号中断（如 SIGALRM、SIGCHLD），需重新调用 epoll_wait，这是正常现象，不需要输出错误信息。
        if (saved_errno != EINTR) {
            errno = saved_errno;
            LOG_ERROR("EPollPoller::poll error: {}", strerror(errno));
        }
    }

    return num_events;
}

// 更新channel或添加channel到Poller
void EPollPoller::updateChannel(Channel* channel){
    const int status = channel->status();
    LOG_TRACE("update channel{fd = %d, events = %d, status = %d}", channel->fd(), channel->events(), status);
    
    if(status == NEW_POLLER || status == DELETED_POLLER){
        int fd = channel->fd();
        if(status == NEW_POLLER){
            channels_[fd] = channel;
        }

        channel->set_status(ADDED_POLLER);
        update(EPOLL_CTL_ADD, channel);
    }else{ // status == ADDED_POLLER
        // 移除没有监听事件的channel
        if(channel->isNoneEvent()){
            update(EPOLL_CTL_DEL, channel);
            channel->set_status(DELETED_POLLER);
        }else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 停止监听channel，并从Poller中移除该channel
void EPollPoller::removeChannel(Channel* channel){
    int fd = channel->fd();
    LOG_TRACE("delete channel{fd = %d", fd);

    int status = channel->status();
    size_t n = channels_.erase(fd);

    if(status == ADDED_POLLER){
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_status(NEW_POLLER);
}

// 调用epoll_ctl更新epoll监听io
void EPollPoller::update(int operation, Channel* channel){
    int fd = channel->fd();

    struct epoll_event ep_event;
    memset(&ep_event, 0, sizeof(ep_event));

    ep_event.events = channel->events();
    ep_event.data.ptr = channel;

    LOG_TRACE("EPollPoller::update operation = %s, fd = %s, events = %d",
              operation == EPOLL_CTL_ADD ? "ADD" : 
              operation == EPOLL_CTL_DEL ? "DEL" : "MOD",
              fd, channel->events());
    
    if (epoll_ctl(epoll_fd_, operation, fd, &ep_event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error: fd = %d, %s", fd, strerror(errno));
        } else {
            LOG_FATAL("epoll_ctl add/mod error: fd = %d, %s", fd, strerror(errno));
        }
    }
}
} // namespace core
