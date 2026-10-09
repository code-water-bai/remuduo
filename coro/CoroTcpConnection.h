#pragma once

#include "Task.h"
#include "CoroChannel.h"
#include "../net/Buffer.h"
#include "../net/InetAddress.h"
#include "../net/Socket.h"
#include "../base/noncopyable.h"
#include <memory>

namespace new_muduo {
namespace coro {

class CoroEventLoop;

class CoroTcpConnection : noncopyable,
    public std::enable_shared_from_this<CoroTcpConnection> {
public:
    using CoroTcpConnectionPtr = std::shared_ptr<CoroTcpConnection>;
    using Handler = std::function<Task<void>(CoroTcpConnectionPtr)>;
    using CloseCallback = std::function<void(CoroTcpConnectionPtr)>;

    CoroTcpConnection(CoroEventLoop* loop, int sockfd,
                      const InetAddress& localAddr,
                      const InetAddress& peerAddr,
                      const std::string& name);
    ~CoroTcpConnection();

    void start();
    void setConnectionHandler(Handler h) { handler_ = std::move(h); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

    // 协程读：从连接读取 len 字节到 buf
    Task<ssize_t> read(void* buf, size_t len);

    // 同步发送（非阻塞，数据入 output buffer）
    void send(const void* data, size_t len);
    void send(const std::string& s);

    int fd() const { return sockfd_; }
    const std::string& name() const { return name_; }
    CoroEventLoop* getLoop() const { return loop_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    const InetAddress& localAddress() const { return localAddr_; }
    bool connected() const { return connected_; }

    void shutdown();
    void forceClose();

private:
    Task<void> connectionLoop();
    void sendInLoop(const void* data, size_t len);
    void onWrite();
    void shutdownInLoop();
    void forceCloseInLoop();

    CoroEventLoop* loop_;
    int sockfd_;
    InetAddress localAddr_;
    InetAddress peerAddr_;
    std::string name_;
    bool connected_ = false;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<CoroChannel> channel_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    bool writing_ = false;

    Handler handler_;
    CloseCallback closeCallback_;
    Task<void> connectionTask_;
};

} // namespace coro
} // namespace new_muduo
