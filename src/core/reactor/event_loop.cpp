#include "core/reactor/event_loop.h"

#include <signal.h>
#include <assert.h>
#include <sys/eventfd.h>
#include "core/utils/logger.h"
namespace core{

// 忽略程序中的 SIGPIPE 信号, 防止因向已关闭的管道或套接字写入数据而导致程序意外终止。
class IgnoreSigPipe {
public:
    IgnoreSigPipe() {
        ::signal(SIGPIPE, SIG_IGN);
    }
};

// 笔记： 利用RAII实现类似go中init()的效果，即在构造函数中执行
IgnoreSigPipe ignore_sig_pipe_dummy;

// 笔记： static 用于非类成员函数时候，表示函数具有内部链接，效果为封装工具函数，避免污染全局命名空间。
// 创建用于唤醒的eventfd
static int createEventfd() {
    // 笔记：eventfd 是 Linux 提供的一个轻量级进程间通信（IPC）机制，替代管道（pipe）。主线程通过 eventfd 通知工作线程有新任务。
    // 创建一个 eventfd 文件描述符，并设置其为非阻塞（EFD_NONBLOCK） 和 close-on-exec（EFD_CLOEXEC） 模式。
    int evfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evfd < 0) {
        LOG_ERROR("Failed to create event fd");
        abort();
    }
    return evfd;
}
EventLoop::EventLoop()
    :looping_(false), quit_(false), calling_pending_functors_(false),
    thread_id_(std::this_thread::get_id()),
    poller_(Poller::newDefaultPoller(this)),
    wakeup_fd_(createEventfd()),
    wakeup_channel_(new Channel(this, wakeup_fd_)),
    timer_manager_(this){
    LOG_DEBUG("EventLoop created in thread %d", thread_id_);

    // 事件发生时，通过向evfd write来唤醒EventLoop进行处理
    wakeup_channel_->setReadCallback(std::bind(&EventLoop::handleWakeup, this));
    wakeup_channel_->enableReading();
}

EventLoop::~EventLoop(){
    wakeup_channel_->disableAll();
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
    LOG_DEBUG("EventLoop in thread %d destroyed", std::this_thread::get_id());
}

// 执行循环
void EventLoop::loop(){
    assert(!looping_);
    assertInLoopThread();

    looping_ = true;
    quit_ = false;
    
    LOG_INFO("EventLoop in thread %d start looping", std::this_thread::get_id());

    while(!quit_){
        active_channels_.clear();

        // 获取活跃通道，Poller是纯虚类，poll可以是不同的实现
        poller_->poll(10, active_channels_);
        // 处理活跃通道的事件
        for(Channel* channel : active_channels_){
            channel->handleEvent();
        }
        // 执行所有超时触发的定时器并移除/重启定时器
        timer_manager_.processTimers();
        // 处理来自其它线程的函数
        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    calling_pending_functors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // swap() 的交换是 O(1) 时间复杂度，仅交换内部指针，不涉及元素拷贝。
        functors.swap(pending_functors_);
    }

    for(const Functor& functor: functors){
        functor();
    }

    calling_pending_functors_ = false;
}

void EventLoop::quit(){
    quit_ = true;

    // 线程可能正阻塞在 epoll_wait 中, 因此调用wakeup()唤醒以退出
    if(!isInLoopThread()){
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){
        cb();
    }else{
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push_back(std::move(cb));
    }

    if(!isInLoopThread() || calling_pending_functors_){
        wakeup();
    }
}

// 向wakeup_fd写入内容，以触发对应的channel事件，达到唤醒线程的目的
void EventLoop::wakeup(){
    uint16_t one = 1;
    ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR("EventLoop::wakeup() writes %d bytes instead of 8", n);
    }
}

// 处理 wakeup() 带来的事件，即读取wakeup_fd_即可
void EventLoop::handleWakeup() {
    uint64_t one = 1;
    ssize_t n = ::read(wakeup_fd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::handleWakeup() reads {} bytes instead of 8", n);
    }
}

void EventLoop::updateChannel(Channel* channel){
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

// 停止监听channel，并从Poller中移除该channel（不会析构channel）
void EventLoop::removeChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->removeChannel(channel);
}

bool EventLoop::isInLoopThread() const {
    return thread_id_ == std::this_thread::get_id();
}

// 服务于调试和开发阶段，保障线程不会被误用
void EventLoop::assertInLoopThread() {
    if (!isInLoopThread()) {
        LOG_FATAL("EventLoop::assertInLoopThread - EventLoop was created in thread {}, current thread is {}",
                 thread_id_, std::this_thread::get_id());
    }
}
} // namespace core