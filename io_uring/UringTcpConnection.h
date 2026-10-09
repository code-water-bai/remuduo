#pragma once

#include "UringAwaiter.h"
#include "../coro/Task.h"
#include "../net/InetAddress.h"
#include "../base/noncopyable.h"
#include <memory>
#include <string>

namespace new_muduo {
namespace io_uring {

class UringEventLoop;

class UringTcpConnection : noncopyable,
    public std::enable_shared_from_this<UringTcpConnection> {
public:
    using UringTcpConnectionPtr = std::shared_ptr<UringTcpConnection>;
    using Handler = std::function<Task<void>(UringTcpConnectionPtr)>;
    using CloseCallback = std::function<void(UringTcpConnectionPtr)>;

    UringTcpConnection(UringEventLoop* loop, int fd,
                       const sockaddr_in* peer_addr,
                       const std::string& name);
    ~UringTcpConnection();

    void start();
    void setConnectionHandle(Handler h) { handle_ = std::move(h); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

    Task<ssize_t> read(char* buf, signed len);
    Task<ssize_t> send(const std::string& message);
    Task<ssize_t> send(const char* data, int len);

    int fd() const { return fd_; }
    const std::string& name() const { return name_; }
    UringEventLoop* loop() const { return loop_; }
    const InetAddress& peerAddress() const { return peer_addr_; }

    void shutdown();
    void forceClose();

private:
    Task<void> connectionLoop();
    void shutdownInLoop();
    void forceCloseInLoop();
    void handleClose();

    UringEventLoop* loop_;
    int fd_;
    std::string name_;
    InetAddress peer_addr_;

    enum State { kDisconnected, kConnecting, kConnected, kDisconnecting };
    State state_ = kDisconnected;

    Handler handle_;
    CloseCallback closeCallback_;

    Task<void> connectionTask_;
};

} // namespace io_uring
} // namespace new_muduo
