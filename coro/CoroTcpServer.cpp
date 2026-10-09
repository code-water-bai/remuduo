#include "CoroTcpServer.h"
#include "CoroEventLoop.h"
#include "../net/Socket.h"
#include "../base/Logging.h"

namespace new_muduo::coro {

CoroTcpServer::CoroTcpServer(EventLoop* loop, const InetAddress& listenAddr,
                             const std::string& name)
    : loop_(loop),
      listenAddr_(listenAddr),
      name_(name),
      acceptor_(std::make_unique<Acceptor>(loop, listenAddr)),
      threadPool_(std::make_shared<EventLoopThreadPool>(loop, name)),
      started_(0),
      nextConnId_(1) {
    acceptor_->setNewConnectionCallback(
        [this](int sockfd, const InetAddress& peerAddr) { onAccept(sockfd, peerAddr); });
}

CoroTcpServer::~CoroTcpServer() {
    for (auto& [name, conn] : connections_) {
        CoroTcpConnection::CoroTcpConnectionPtr guard(conn);
        conn.reset();
        guard->getLoop()->runInLoop([guard]() {});
    }
}

void CoroTcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

void CoroTcpServer::start() {
    if (started_.fetch_add(1) > 0) return;
    threadPool_->start(threadInitCallback_);
    loop_->runInLoop([this]() {
        acceptor_->listen();
    });
    LOG_TRACE << "CoroTcpServer[" << name_ << "] listening on " << listenAddr_.toIpPort();
}

void CoroTcpServer::onAccept(int sockfd, const InetAddress& peerAddr) {
    char buf[64];
    snprintf(buf, sizeof(buf), "#%d", nextConnId_++);
    std::string connName = name_ + buf;

    InetAddress localAddr(getLocalAddr(sockfd));
    EventLoop* ioLoop = threadPool_->getNextLoop();

    auto coroLoop = std::make_shared<CoroEventLoop>(ioLoop);
    auto conn = std::make_shared<CoroTcpConnection>(
        coroLoop.get(), sockfd, localAddr, peerAddr, connName);

    conn->setConnectionHandler(handler_);
    conn->setCloseCallback([this](const CoroTcpConnection::CoroTcpConnectionPtr& c) {
        connections_.erase(c->name());
    });

    connections_[connName] = conn;
    coroLoop->runInLoop([conn]() { conn->start(); });
}

} // namespace new_muduo::coro
