#include "core/thread/eventloop_thread_pool.h"

#include "core/thread/eventloop_thread.h"
#include "core/reactor/event_loop.h"
#include "core/utils/logger.h"

namespace core {

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : base_loop_(baseLoop),
      name_(nameArg),
      started_(false),
      num_threads_(0),
      next_(0) {
}

EventLoopThreadPool::~EventLoopThreadPool() {
    // 不需要特别的清理工作，unique_ptr会自动销毁
    // 所有loop都是栈变量(线程函数或main，生命周期与程序一致)，不需要特别的析构操作。
}

// 启动所有线程，并执行线程初始化回调函数
void EventLoopThreadPool::start(const ThreadInitCallback & cb){
    base_loop_->assertInLoopThread();

    started_ = true;

    // 创建线程，num_threads == 0则只有base_loop_一个线程
    for(int i = 0;i < num_threads_;i++){
        // 笔记："Variable Length Array(VLA)"特性为c特性，但gcc/clang也支持c++拥有该特性
        char thread_name[name_.size() + 32];
        snprintf(thread_name, sizeof(thread_name), "%s%d", name_.c_str(), i);

        EventLoopThread* t = new EventLoopThread(cb, thread_name);
        EventLoop* loop = t->startLoop();
        // 笔记：std::make_unique和std::unique_ptr的区别类似于push和emplace的区别
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(loop);
    }

    // 如果没有线程，使用基础事件循环
    if (num_threads_ == 0 && cb) {
        cb(base_loop_);
    }
}

// 让所有loops(所有线程)轮流处理连接，负载均衡
EventLoop* EventLoopThreadPool::getNextLoop() {
    base_loop_->assertInLoopThread();
    
    // 默认使用基础事件循环
    EventLoop* loop = base_loop_;
    
    // 如果有其他事件循环，轮流选择
    if (!loops_.empty()) {
        loop = loops_[next_];
        ++next_;
        if (static_cast<size_t>(next_) >= loops_.size()) {
            next_ = 0;
        }
    }
    
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    base_loop_->assertInLoopThread();
    
    // 如果没有线程，返回基础事件循环
    if (loops_.empty()) {
        return { base_loop_ };
    } else {
        return loops_;
    }
}
} // namespace core