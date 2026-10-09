#include "UringTcpServer.h"
#include "UringEventLoopThreadPool.h"
#include "../net/Socket.h"
#include "../base/Logging.h"
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <cstring>

namespace new_muduo {
namespace io_uring {

UringTcpServer::UringTcpServer(UringEventLoop* loop, const InetAddress& addr,
                               const std::string& name)
    : loop_(loop),
      name_(name),
      ipPort_(addr.toIpPort()),
      threadPool_(std::make_shared<UringEventLoopThreadPool>(loop, name)),
      started_(0),
      nextConnId_(1) {}

UringTcpServer::~UringTcpServer() {
    for (auto& [name, conn] : connections_) {
        auto guard = conn;
        conn.reset();
        guard->loop()->queueInLoop([guard]() {});
    }
}

void UringTcpServer::setThreadNums(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

Task<void> UringTcpServer::start(uint16_t port) {
    listen_fd_ = createNonblockingOrDie();

    int opt = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    ::setsockopt(listen_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int ret = ::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret < 0) {
        LOG_SYSFATAL << "bind failed";
    }
    ret = ::listen(listen_fd_, SOMAXCONN);
    if (ret < 0) {
        LOG_SYSFATAL << "listen failed";
    }

    started_.store(1);
    threadPool_->start(threadInitCallback_);

    while (true) {
        UringContext ctx(loop_);
        sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        int conn_fd = co_await AcceptAwaiter{listen_fd_, peer_addr, peer_len, ctx};

        if (conn_fd < 0) {
            continue;
        }

        EventLoop* ioLoop = threadPool_->getNextLoop();
        onNewConnection(ioLoop, conn_fd, peer_addr);
    }

    co_return;
}

void UringTcpServer::onNewConnection(EventLoop* loop, int conn_fd,
                                     const sockaddr_in& peer_addr) {
    char buf[64];
    snprintf(buf, sizeof(buf), "#%d", nextConnId_++);
    std::string connName = name_ + buf;

    auto conn = std::make_shared<UringTcpConnection>(
        static_cast<UringEventLoop*>(loop), conn_fd, &peer_addr, connName);

    conn->setConnectionHandle(handle_);
    conn->setCloseCallback([this](const UringTcpConnection::UringTcpConnectionPtr& c) {
        removeConnection(c);
    });

    connections_[connName] = conn;
    loop->runInLoop([conn]() { conn->start(); });
}

void UringTcpServer::removeConnection(const UringTcpConnection::UringTcpConnectionPtr& conn) {
    loop_->runInLoop(std::bind(&UringTcpServer::removeConnectionInLoop, this, conn));
}

void UringTcpServer::removeConnectionInLoop(const UringTcpConnection::UringTcpConnectionPtr& conn) {
    connections_.erase(conn->name());
}

} // namespace io_uring
} // namespace new_muduo
