#include "UringEventLoop.h"
#include "UringPoller.h"
#include "../base/Logging.h"
#include <assert>

namespace new_muduo::io_uring {

UringEventLoop::UringEventLoop()
    : poller_(UringPoller::newPoller(this)) {}

UringEventLoop::~UringEventLoop() = default;

void UringEventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;
    LOG_TRACE << "UringEventLoop " << this << " start looping";

    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(100, &activeChannels_);
        ++iteration_;

        // UringPoller 直接在 handleCompletions 中 resume 协程，
        // 不需要像 epoll 那样遍历 activeChannels_

        if (coroScheduler_) {
            coroScheduler_->processPendingResumes();
        }

        doPendingFunctors();
    }

    LOG_TRACE << "UringEventLoop " << this << " stop looping";
    looping_ = false;
}

void UringEventLoop::wait_read(int fd, void* buf, signed len, UringContext* ctx) {
    poller_->prepareRead(fd, buf, static_cast<size_t>(len), ctx);
    // 不立即提交 — 由 poll() 统一批量提交
}

void UringEventLoop::wait_write(int fd, const void* buf, signed len, UringContext* ctx) {
    poller_->prepareWrite(fd, buf, static_cast<size_t>(len), ctx);
}

void UringEventLoop::wait_accept(int fd, sockaddr* peeraddr, UringContext* ctx, socklen_t* len) {
    poller_->prepareAccept(fd, peeraddr, len, ctx);
}

int UringEventLoop::registerFd(int raw_fd) {
    return poller_->registerFd(raw_fd);
}

void UringEventLoop::unregisterFd(int raw_fd) {
    poller_->unregisterFd(raw_fd);
}

void UringEventLoop::releaseBuffer(int idx) {
    poller_->releaseBuffer(idx);
}

void* UringEventLoop::bufferPtr(int idx) {
    return poller_->bufferPtr(idx);
}

} // namespace new_muduo::io_uring
