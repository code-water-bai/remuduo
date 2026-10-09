#pragma once

#include "../base/noncopyable.h"
#include <coroutine>
#include <vector>
#include <mutex>

namespace new_muduo {
class EventLoop;
}

namespace new_muduo::coro {

class CoroScheduler : noncopyable {
public:
    explicit CoroScheduler(EventLoop* loop);
    ~CoroScheduler();

    // 如果当前在 loop 线程，直接 resume；否则缓存后批量处理
    void scheduleResume(std::coroutine_handle<> h);
    void processPendingResumes();

private:
    EventLoop* loop_;
    std::vector<std::coroutine_handle<>> pending_;
    std::mutex mutex_;
};

} // namespace new_muduo::coro
