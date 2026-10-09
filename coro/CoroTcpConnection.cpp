#include "CoroTcpConnection.h"
#include "CoroEventLoop.h"
#include "../base/Logging.h"

namespace new_muduo::coro {

CoroTcpConnection::CoroTcpConnection(CoroEventLoop* loop, int sockfd,
                                     const InetAddress& localAddr,
                                     const InetAddress& peerAddr,
                                     const std::string& name)
    : loop_(loop),
      sockfd_(sockfd),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      name_(name),
      socket_(std::make_unique<Socket>(sockfd)),
      channel_(std::make_unique<CoroChannel>(loop, sockfd)) {
    socket_->setTcpNoDelay(true);
    socket_->setKeepAlive(true);
}

CoroTcpConnection::~CoroTcpConnection() {
    if (channel_) {
        channel_->remove();
    }
}

void CoroTcpConnection::start() {
    connected_ = true;
    if (!handler_) {
        LOG_ERROR << "CoroTcpConnection::start - no handler set";
        forceClose();
        return;
    }
    // 读事件常驻 epoll，和原版 TcpConnection 行为一致
    channel_->enableReading();

    connectionTask_ = connectionLoop();
    if (connectionTask_) {
        connectionTask_.handle().resume();
    }
}

Task<ssize_t> CoroTcpConnection::read(void* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        // 1. 先尝试从 inputBuffer 拷贝（已读到的数据）
        size_t avail = inputBuffer_.readableBytes();
        if (avail > 0) {
            size_t take = std::min(avail, len - total);
            std::memcpy(static_cast<char*>(buf) + total, inputBuffer_.peek(), take);
            inputBuffer_.retrieve(take);
            total += take;
            continue;
        }

        // 2. 再尝试直接 readFd（避免不必要的协程挂起）
        int savedErrno = 0;
        ssize_t n = inputBuffer_.readFd(sockfd_, &savedErrno);
        if (n > 0) {
            continue;
        } else if (n == 0) {
            handleClose();
            co_return static_cast<ssize_t>(total);
        } else {
            errno = savedErrno;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 3. 确实没数据了，才挂起等待可读事件
                co_await channel_->read();
                continue;
            }
            handleClose();
            co_return (total > 0) ? static_cast<ssize_t>(total) : -1;
        }
    }
    co_return static_cast<ssize_t>(total);
}

void CoroTcpConnection::send(const void* data, size_t len) {
    if (!connected_) return;
    if (loop_->getRawLoop()->isInLoopThread()) {
        sendInLoop(data, len);
    } else {
        std::string msg(static_cast<const char*>(data), len);
        loop_->runInLoop([this, msg = std::move(msg)]() mutable {
            sendInLoop(msg.data(), msg.size());
        });
    }
}

void CoroTcpConnection::send(const std::string& s) {
    send(s.data(), s.size());
}

void CoroTcpConnection::sendInLoop(const void* data, size_t len) {
    outputBuffer_.append(data, len);
    if (!writing_) {
        writing_ = true;
        channel_->setWriteCallback([this]() { onWrite(); });
        channel_->enableWriting();
    }
}

void CoroTcpConnection::onWrite() {
    ssize_t n = ::write(sockfd_, outputBuffer_.peek(), outputBuffer_.readableBytes());
    if (n > 0) {
        outputBuffer_.retrieve(n);
        if (outputBuffer_.readableBytes() == 0) {
            channel_->disableWriting();
            writing_ = false;
        }
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        LOG_ERROR << "CoroTcpConnection::onWrite error";
        handleClose();
    }
}

Task<void> CoroTcpConnection::connectionLoop() {
    co_await handler_(shared_from_this());
    if (connected_) {
        forceClose();
    }
    co_return;
}

void CoroTcpConnection::shutdown() {
    if (connected_) {
        loop_->runInLoop([this]() { shutdownInLoop(); });
    }
}

void CoroTcpConnection::forceClose() {
    if (!connected_) return;
    loop_->runInLoop([this]() { forceCloseInLoop(); });
}

void CoroTcpConnection::shutdownInLoop() {
    socket_->shutdownWrite();
}

void CoroTcpConnection::forceCloseInLoop() {
    if (!connected_) return;
    connected_ = false;
    channel_->disableAll();
    if (closeCallback_) {
        auto guard = shared_from_this();
        closeCallback_(guard);
    }
}

} // namespace new_muduo::coro
