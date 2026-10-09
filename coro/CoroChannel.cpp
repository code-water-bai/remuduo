#include "CoroChannel.h"
#include "CoroEventLoop.h"
#include "../net/EventLoop.h"
#include "../base/Logging.h"

#include <utility>

namespace neo::coro {

    CoroChannel::CoroChannel(CoroEventLoop* loop, int fd)
        : loop_(loop), fd_(fd) {}

    CoroChannel::~CoroChannel() {
        remove();
    }

    

    void CoroChannel::enableReading() {
        if (!attached_) {
            auto* rawLoop = loop_->getRawLoop();
            channel_ = std::make_unique<neo::Channel>(rawLoop, fd_);
            channel_->setReadCallback([this](neo::Timestamp t) { handleRead(t); });
            channel_->setWriteCallback([this]() { handleWrite();  });
            channel_->setCloseCallback([this]() { handleClose();  });
            attached_ = true;
        }
        reading_ = true;
        channel_->enableReading();
    }

    void CoroChannel::disableReading() {
        reading_ = false;
        if (channel_)
            channel_->disableReading();
    }

    void CoroChannel::enableWriting() {
        if (!attached_) {
            auto* rawLoop = loop_->getRawLoop();
            channel_ = std::make_unique<neo::Channel>(rawLoop, fd_);
            channel_->setReadCallback([this](neo::Timestamp t) { handleRead(t); });
            channel_->setWriteCallback([this]() { handleWrite();  });
            channel_->setCloseCallback([this]() { handleClose();  });
            attached_ = true;
        }
        writing_ = true;
        channel_->enableWriting();
    }

    void CoroChannel::disableWriting() {
        writing_ = false;
        if (channel_)
            channel_->disableWriting();
    }

    void CoroChannel::disableAll() {
        reading_ = false;
        writing_ = false;
        if (channel_)
            channel_->disableAll();
    }

    void CoroChannel::remove() {
        disableAll();
        if (channel_) {
            channel_->remove();
            channel_.reset();
            attached_ = false;
        }
        readWaiter_ = nullptr;
        writeWaiter_ = nullptr;
    }

    

    void CoroChannel::handleRead(neo::Timestamp ) {
        if (readWaiter_) {
            auto h = std::exchange(readWaiter_, nullptr);
            // 不 disableReading — 读事件常驻 epoll，和原版 Channel 行为一致
            loop_->scheduleResume(h);
            return;
        }
       /* if (readCallback_) {
            neo::Buffer buf;
            readCallback_(&buf);
        }*/
    }

    void CoroChannel::handleWrite() {
        if (writeWaiter_) {
            auto h = std::exchange(writeWaiter_, nullptr);
            disableWriting();
            loop_->scheduleResume(h);
            return;
        }
      /*  if (writeCallback_) {
            writeCallback_();
        }*/
    }

    void CoroChannel::handleClose() {
        
        if (readWaiter_) {
            auto h = std::exchange(readWaiter_, nullptr);
            h.resume();
        }
        if (writeWaiter_) {
            auto h = std::exchange(writeWaiter_, nullptr);
            h.resume();
        }

        if (closeCallback_)
            closeCallback_();
        remove();
    }

} 
