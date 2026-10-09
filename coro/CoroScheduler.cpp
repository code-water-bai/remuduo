#include "CoroScheduler.h"
#include "../net/EventLoop.h"

namespace new_muduo::coro {

CoroScheduler::CoroScheduler(EventLoop* loop) : loop_(loop) {}
CoroScheduler::~CoroScheduler() = default;

void CoroScheduler::scheduleResume(std::coroutine_handle<> h) {
    if (!h || h.done()) return;

    // 热点路径：loop 线程直接 resume，零队列开销
    if (loop_->isInLoopThread()) {
        h.resume();
        return;
    }

    // 跨线程：缓存 + 唤醒
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.push_back(h);
    }
    loop_->wakeup();
}

void CoroScheduler::processPendingResumes() {
    std::vector<std::coroutine_handle<>> tmp;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tmp.swap(pending_);
    }
    for (auto& h : tmp) {
        if (h && !h.done()) {
            h.resume();
        }
    }
}

} // namespace new_muduo::coro
