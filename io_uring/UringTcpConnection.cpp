#include "UringTcpConnection.h"
#include "UringEventLoop.h"
#include "../base/Logging.h"
#include <unistd.h>
#include <cstring>

namespace new_muduo {
namespace io_uring {

UringTcpConnection::UringTcpConnection(UringEventLoop* loop, int fd,
                                       const sockaddr_in* peer_addr,
                                       const std::string& name)
    : loop_(loop),
      fd_(fd),
      name_(name),
      peer_addr_(*peer_addr),
      state_(kDisconnected) {
    int opt = 1;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
}

UringTcpConnection::~UringTcpConnection() {
    if (fd_ > 0) {
        loop_->unregisterFd(fd_);
        ::close(fd_);
    }
}

void UringTcpConnection::start() {
    state_ = kConnecting;
    if (!handle_) {
        LOG_ERROR << "UringTcpConnection::start - no handler set";
        forceClose();
        return;
    }

    int fidx = loop_->registerFd(fd_);
    if (fidx < 0) {
        LOG_WARN << "registerFd(" << fd_ << ") failed, falling back to raw fd";
    }

    connectionTask_ = connectionLoop();
    if (connectionTask_) {
        connectionTask_.handle().resume();
    }
}

Task<ssize_t> UringTcpConnection::read(char* buf, signed len) {
    int remain = len;
    char* pos = buf;
    UringContext ctx_(loop_);

    while (remain > 0) {
        co_await ReadAwaiter{fd_, pos, static_cast<unsigned>(remain), ctx_};
        ssize_t res = ctx_.res;

        if (ctx_.buf_index_ >= 0) {
            if (res > 0) {
                void* rbuf = loop_->bufferPtr(ctx_.buf_index_);
                std::memcpy(pos, rbuf, static_cast<size_t>(res));
            }
            loop_->releaseBuffer(ctx_.buf_index_);
            ctx_.buf_index_ = -1;
        }

        if (res > 0) {
            remain -= static_cast<int>(res);
            pos += res;
        } else if (res == 0) {
            handleClose();
            co_return static_cast<ssize_t>(len - remain);
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            handleClose();
            co_return (len - remain > 0) ? static_cast<ssize_t>(len - remain) : res;
        }
    }
    co_return static_cast<ssize_t>(len);
}

Task<ssize_t> UringTcpConnection::send(const std::string& message) {
    return send(message.data(), static_cast<int>(message.size()));
}

Task<ssize_t> UringTcpConnection::send(const char* data, int len) {
    UringContext ctx_(loop_);
    int remain = len;
    const char* pos = data;

    while (remain > 0) {
        co_await WriteAwaiter{fd_, pos, static_cast<unsigned>(remain), ctx_};
        ssize_t res = ctx_.res;

        if (ctx_.buf_index_ >= 0) {
            loop_->releaseBuffer(ctx_.buf_index_);
            ctx_.buf_index_ = -1;
        }

        if (res > 0) {
            remain -= static_cast<int>(res);
            pos += res;
        } else if (res == 0) {
            continue;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            handleClose();
            co_return (len - remain > 0) ? static_cast<ssize_t>(len - remain) : res;
        }
    }
    co_return static_cast<ssize_t>(len);
}

Task<void> UringTcpConnection::connectionLoop() {
    state_ = kConnected;
    co_await handle_(shared_from_this());
    if (state_ != kDisconnected) {
        forceClose();
    }
    co_return;
}

void UringTcpConnection::shutdown() {
    if (state_ == kConnected) {
        state_ = kDisconnecting;
        loop_->queueInLoop([this]() { shutdownInLoop(); });
    }
}

void UringTcpConnection::forceClose() {
    if (state_ == kDisconnected || state_ == kDisconnecting) return;
    state_ = kDisconnecting;
    loop_->queueInLoop([this]() { forceCloseInLoop(); });
}

void UringTcpConnection::shutdownInLoop() {
    ::shutdown(fd_, SHUT_WR);
}

void UringTcpConnection::forceCloseInLoop() {
    if (state_ == kDisconnected) return;
    loop_->unregisterFd(fd_);
    ::shutdown(fd_, SHUT_RDWR);
    handleClose();
}

void UringTcpConnection::handleClose() {
    if (state_ == kDisconnected) return;
    state_ = kDisconnected;
    UringTcpConnectionPtr guard = shared_from_this();
    if (closeCallback_) {
        closeCallback_(guard);
    }
}

} // namespace io_uring
} // namespace new_muduo
