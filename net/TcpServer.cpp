#include "TcpServer.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "../base/Logging.h"
#include "Socket.h"

#include <functional>
#include <condition_variable>
#include <assert.h>

namespace new_muduo {
	

    using EventLoopThreadPool = new_muduo::EventLoopThreadPool;
    TcpServer::TcpServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const std::string& name,
        bool reusePort)
        : loop_(loop),
        ipPort_(listenAddr.toIpPort()),
        name_(name),
        acceptor_(std::make_unique<Acceptor>(loop, listenAddr, reusePort)),
        threadPool_(std::make_shared<EventLoopThreadPool>(loop, name)),
        started_(0),
        nextConnId_(1) {
        acceptor_->setNewConnectionCallback([this](int sockfd, const InetAddress& peerAddr) {
            newConnection(sockfd, peerAddr);
            });
    }

    void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
        loop_->assertInLoopThread();
        char buf[64];
        snprintf(buf, sizeof(buf), "#%d", nextConnId_);
        ++nextConnId_;
        std::string connName = name_ + buf;

        LOG_TRACE << "TcpServer::newConnection [" << name_
            << "] - new connection [" << connName
            << "] from " << peerAddr.toIpPort();

        InetAddress localAddr(getLocalAddr(sockfd));
        EventLoop* ioLoop = threadPool_->getNextLoop();
        TcpConnectionPtr conn = std::make_shared<TcpConnection>(
            ioLoop, connName, sockfd, localAddr, peerAddr);

        connections_[connName] = conn;
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setWriteCompleteCallback(writeCompleteCallback_);
        conn->setCloseCallback([this](const TcpConnectionPtr& c) { removeConnection(c); });

        ioLoop->runInLoop([conn]() { conn->connectEstablished(); });
    }

    void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
        loop_->runInLoop([this, conn]() { removeConnectionInLoop(conn); });
    }

    void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
        loop_->assertInLoopThread();
        LOG_TRACE << "TcpServer::removeConnectionInLoop [" << name_
            << "] - connection " << conn->name();
        size_t n = connections_.erase(conn->name());
        assert(n == 1);
        (void)n;

        EventLoop* ioLoop = conn->getLoop();
        ioLoop->queueInLoop([conn]() { conn->connectDestroyed(); });
    }

    TcpServer::~TcpServer() {
        loop_->assertInLoopThread();
        LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

        for (auto& [name, conn] : connections_) {
            TcpConnectionPtr connPtr(conn);
            conn.reset();
            connPtr->getLoop()->runInLoop([connPtr]() {
                connPtr->connectDestroyed();
                });
        }
    }

    void TcpServer::setThreadNum(int numThreads) {
        assert(numThreads >= 0);
        threadPool_->setThreadNum(numThreads);
    }

    void TcpServer::start() {
        if (started_.fetch_add(1) == 0) {
            threadPool_->start(threadInitCallback_);
            assert(!acceptor_->listening());
            loop_->runInLoop([this]() {
                acceptor_->listen();
                });
        }
    }
}