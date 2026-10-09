#pragma once
#include "../base/noncopyable.h"
#include "Callbacks.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "../net/Socket.h"
#include "../net/InetAddress.h"
#include "../base/Logging.h"
#include "../net/TcpServer.h"
#include  "EventLoop.h"
#include "../base/Thread.h"
#include <assert.h>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


#include <map>
#include <memory>
#include <string>
#include <atomic>

namespace new_muduo {
    class EventLoopThread : new_muduo::noncopyable {
    public:
        using ThreadInitCallback = std::function<void(EventLoop*)>;

        EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback())
            : loop_(nullptr),
            exiting_(false),
            thread_(std::bind(&EventLoopThread::threadFunc, this)),
            mutex_(),
            cond_(),
            callback_(cb) {
        }

        ~EventLoopThread() {
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
            EventLoop loop;
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
}