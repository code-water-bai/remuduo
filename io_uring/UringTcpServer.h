#pragma once

#include "UringTcpConnection.h"
#include "UringAwaiter.h"
#include "../coro/Task.h"
#include "../net/InetAddress.h"
#include "../base/noncopyable.h"
#include <map>
#include <memory>
#include <atomic>

namespace new_muduo {
namespace io_uring {

class UringEventLoop;
class UringEventLoopThreadPool;

class UringTcpServer : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    UringTcpServer(UringEventLoop* loop, const InetAddress& addr,
                   const std::string& name);
    ~UringTcpServer();

    Task<void> start(uint16_t port);
    void setThreadNums(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }

    void setConnectionHandle(UringTcpConnection::Handler h) { handle_ = std::move(h); }

private:
    void onNewConnection(EventLoop* loop, int conn_fd, const sockaddr_in& peer_addr);
    void removeConnection(const UringTcpConnection::UringTcpConnectionPtr& conn);
    void removeConnectionInLoop(const UringTcpConnection::UringTcpConnectionPtr& conn);

    UringEventLoop* loop_;
    std::string name_;
    std::string ipPort_;

    int listen_fd_ = -1;
    std::atomic<int> started_{0};
    int nextConnId_ = 1;

    std::shared_ptr<UringEventLoopThreadPool> threadPool_;
    ThreadInitCallback threadInitCallback_;

    std::map<std::string, std::shared_ptr<UringTcpConnection>> connections_;
    UringTcpConnection::Handler handle_;
};

} // namespace io_uring
} // namespace new_muduo
