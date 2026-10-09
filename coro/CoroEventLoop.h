#pragma once

#include "../base/noncopyable.h"

#include <coroutine>
#include <memory>
#include"../net/Callbacks.h"
#define neo new_muduo

namespace new_muduo {
    class EventLoop;
    class TimerQueue;
} 

namespace new_muduo::coro{

    class CoroScheduler;

    
    class CoroEventLoop : noncopyable {
    public:
        explicit CoroEventLoop(EventLoop* loop);
        ~CoroEventLoop();

       
        static CoroEventLoop* current();

        EventLoop* getRawLoop() const { return loop_; }

        
        void loop();

        void quit();

      
        void postTask(std::coroutine_handle<> h);

       
        void runInLoop(const std::function<void()>& cb);
        void queueInLoop(const std::function<void()>& cb);

      
        void scheduleResume(std::coroutine_handle<> h);

        bool isInLoopThread() const;

        

    private:
        void processPendingResumes();

        EventLoop* loop_;
        std::unique_ptr<CoroScheduler> scheduler_;
       
    };

}