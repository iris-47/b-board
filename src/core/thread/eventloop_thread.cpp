#include "core/thread/eventloop_thread.h"

#include "core/reactor/event_loop.h"
#include "core/utils/logger.h"

namespace core {

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string & name)
    :loop_(nullptr), exiting_(false), thread_(),
    init_callback_(cb), name_(name)
{}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop(){
    thread_ = std::thread(&EventLoopThread::threadFunc, this);

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 笔记：使用while, 因为 wait() 可能出现“虚假唤醒”，因此需要在唤醒后检查条件是否满足
        // 等待threadFunc创建并初始化loop_
        while(loop_ == nullptr){
            cond_.wait(lock);
        }
        loop = loop_;
    }

    return loop;
}

void EventLoopThread::threadFunc(){
    EventLoop loop;

    if(init_callback_){
        init_callback_(&loop);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop_->loop();

    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = nullptr;
}

} //namespace core