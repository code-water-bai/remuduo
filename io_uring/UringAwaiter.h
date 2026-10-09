#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <coroutine>
#include "UringPoller.h"

namespace new_muduo {
namespace io_uring {

struct ReadAwaiter {
    int         fd_;
    void*       buf_;
    unsigned    len_;
    UringContext* ctx_;

    ReadAwaiter(int fd, void* buf, unsigned len, UringContext& ctx)
        : fd_(fd), buf_(buf), len_(len), ctx_(&ctx) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        ctx_->handle_ = h;
        ctx_->loop_->wait_read(fd_, buf_, static_cast<signed>(len_), ctx_);
    }

    int await_resume() const noexcept {
        return ctx_->res;
    }
};

struct WriteAwaiter {
    int           fd_;
    const void*   buf_;
    unsigned      len_;
    UringContext* ctx_;

    WriteAwaiter(int fd, const void* buf, unsigned len, UringContext& ctx)
        : fd_(fd), buf_(buf), len_(len), ctx_(&ctx) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        ctx_->handle_ = h;
        ctx_->loop_->wait_write(fd_, buf_, static_cast<signed>(len_), ctx_);
    }

    int await_resume() const noexcept {
        return ctx_->res;
    }
};

struct AcceptAwaiter {
    int            listen_fd_;
    sockaddr_in*   addr_;
    socklen_t*     addrlen_;
    UringContext*  ctx_;

    AcceptAwaiter(int fd, sockaddr_in& addr, socklen_t& len, UringContext& ctx)
        : listen_fd_(fd), addr_(&addr), addrlen_(&len), ctx_(&ctx) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        ctx_->handle_ = h;
        ctx_->loop_->wait_accept(listen_fd_, reinterpret_cast<sockaddr*>(addr_), ctx_, addrlen_);
    }

    int await_resume() const noexcept {
        return ctx_->res;
    }
};

} // namespace io_uring
} // namespace new_muduo
