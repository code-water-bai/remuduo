#pragma once

#include "UringEventLoop.h"
#include "../base/noncopyable.h"
#include "../base/Thread.h"
#include <functional>
#include <mutex>
#include <condition_variable>

namespace new_muduo {
namespace io_uring {

class UringEventLoopThread : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    explicit UringEventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback())
        : loop_(nullptr),
          exiting_(false),
          thread_(std::bind(&UringEventLoopThread::threadFunc, this)),
          callback_(cb) {}

    ~UringEventLoopThread() {
        exiting_ = true;
        if (loop_ != nullptr) {
            loop_->quit();
            thread_.join();
        }
    }

    EventLoop* startLoop() {
        assert(!thread_.started());
        thread_.start();

        EventLoop* loop = nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this] { return loop_ != nullptr; });
            loop = loop_;
        }
        return loop;
    }

private:
    void threadFunc() {
        UringEventLoop loop;
        if (callback_) {
            callback_(&loop);
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            loop_ = &loop;
            cond_.notify_one();
        }
        loop.loop();
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }

    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};

} // namespace io_uring
} // namespace new_muduo
