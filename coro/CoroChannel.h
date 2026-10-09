#pragma once

#include "../base/noncopyable.h"
#include "../net/Channel.h"

#include <coroutine>
#include <functional>
#include <memory>
#include <utility>
#define neo new_muduo

namespace new_muduo {
    class EventLoop;
    class Buffer;
    class Timestamp;
}

namespace new_muduo::coro {

    class CoroEventLoop;

   
    class CoroChannel : noncopyable {
    public:
        
        struct ReadAwaiter {
            CoroChannel* channel_;
            explicit ReadAwaiter(CoroChannel* ch) : channel_(ch) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) {
                channel_->readWaiter_ = h;
                // 不调用 enableReading — 连接建立后读事件常驻 epoll
            }
            void await_resume() const noexcept {}
        };

        struct WriteAwaiter {
            CoroChannel* channel_;
            explicit WriteAwaiter(CoroChannel* ch) : channel_(ch) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) {
                channel_->writeWaiter_ = h;
                channel_->enableWriting();
            }
            void await_resume() const noexcept {}
        };

        using ReadCallback = std::function<void(neo::Buffer*)>;
        using WriteCallback = std::function<void()>;
        using CloseCallback = std::function<void()>;

        CoroChannel(CoroEventLoop* loop, int fd);
        ~CoroChannel();

        int fd() const { return fd_; }
        CoroEventLoop* getLoop() const { return loop_; }
        int events()const { return channel_->events(); }

       
        ReadAwaiter  read() { return ReadAwaiter{ this }; }
        WriteAwaiter write() { return WriteAwaiter{ this }; }

       
        void setReadCallback(ReadCallback cb) { readCallback_ = std::move(cb); }
        void setWriteCallback(WriteCallback cb) { writeCallback_ = std::move(cb); }
        void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

        void enableReading();
        void disableReading();
        void enableWriting();
        void disableWriting();
        void disableAll();
        void remove();

        bool isReading() const { return reading_; }
        bool isWriting() const { return writing_; }

    private:
        void handleRead(neo::Timestamp t);
        void handleWrite();
        void handleClose();

        CoroEventLoop* loop_;
        int fd_;
        bool reading_ = false;
        bool writing_ = false;
        bool attached_ = false;

        std::coroutine_handle<> readWaiter_;
        std::coroutine_handle<> writeWaiter_;

        ReadCallback  readCallback_;
        WriteCallback writeCallback_;
        CloseCallback closeCallback_;

        std::unique_ptr<neo::Channel> channel_;
    };

}  // namespace neo::coro
