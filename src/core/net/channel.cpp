#include "core/net/channel.h"

#include <assert.h>
#include "core/reactor/event_loop.h"
#include "core/utils/logger.h"
namespace core{
Channel::Channel(EventLoop* loop, int fd)
    :loop_(loop), fd_(fd),
     events_(0), revents_(0), status_(-1), // -1表示新建channel，不属于任何poller
     event_handling_(false), tied_(false){}

Channel::~Channel(){
    // 笔记：assert通常在调试版本中生效，而在发布版本中会被编译器优化掉（如果定义了 NDEBUG 宏）。 assert 是开发者对代码正确性的声明，用于捕获程序中的逻辑错误，而非处理预期的运行时异常。
    assert(!event_handling_);
}

// 绑定当前处理的对象，防止obj被其它线程析构
void Channel::tie(const std::shared_ptr<void>& obj){
    tie_ = obj;
    tied_ = true;
}

void Channel::update(){
    loop_->updateChannel(this);
}

void Channel::remove() {
    loop_->removeChannel(this);
}

void Channel::handleEvent(){
    // 如果 tied_ 且 guard 为 nullptr，说明绑定的对象已销毁，跳过处理
    std::shared_ptr<void> guard = tied_ ? tie_.lock() : std::shared_ptr<void>();
    if (!tied_ || guard) {
        event_handling_ = true;
    
        LOG_TRACE("fd = %d, revents = %d", fd_, revents_);
        
        // 对端关闭连接（半关闭或全关闭），但本地可能仍有数据可读。
        if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
            LOG_WARN("fd = %d POLLHUP", fd_);
            if (close_callback_) close_callback_();
        }
        
        // 错误事件|无效文件描述符
        if (revents_ & (POLLERR | POLLNVAL)) {
            if (error_callback_) error_callback_();
        }
        
        // 读事件|紧急数据|对端关闭写
        if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
            if (read_callback_) read_callback_();
        }
        
        // 写事件
        if (revents_ & POLLOUT) {
            if (write_callback_) write_callback_();
        }
        
        event_handling_ = false;
    }
}

} // namespace core