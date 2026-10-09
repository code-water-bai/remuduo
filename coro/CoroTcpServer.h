#pragma once

#include "CoroTcpConnection.h"
#include "../net/Acceptor.h"
#include "../net/InetAddress.h"
#include "../net/EventLoopThreadPool.h"
#include "../base/noncopyable.h"
#include <map>
#include <memory>
#include <atomic>

namespace new_muduo {
namespace coro {

class CoroEventLoop;

class CoroTcpServer : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    CoroTcpServer(EventLoop* loop, const InetAddress& listenAddr,
                  const std::string& name);
    ~CoroTcpServer();

    void setThreadNum(int numThreads);
    void start();
    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
    void setConnectionHandler(CoroTcpConnection::Handler h) { handler_ = std::move(h); }

private:
    void onAccept(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const CoroTcpConnection::CoroTcpConnectionPtr& conn);

    EventLoop* loop_;
    InetAddress listenAddr_;
    std::string name_;
    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    std::atomic<int> started_{0};
    int nextConnId_ = 1;

    std::map<std::string, CoroTcpConnection::CoroTcpConnectionPtr> connections_;
    CoroTcpConnection::Handler handler_;
    ThreadInitCallback threadInitCallback_;
};

} // namespace coro
} // namespace new_muduo
