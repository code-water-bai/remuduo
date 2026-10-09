#include "net/TcpServer.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpConnection.h"
#include "net/Buffer.h"
#include "base/Logging.h"

using namespace new_muduo;

class EchoServer {
public:
    EchoServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
        : server_(loop, addr, name) {
        server_.setConnectionCallback(
            [this](const TcpConnectionPtr& conn) { onConnection(conn); });
        server_.setMessageCallback(
            [this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
                onMessage(conn, buf);
            });
    }

    void setThreadNum(int n) { server_.setThreadNum(n); }
    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        // 连接/断开日志保留，非热路径
        LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort()
                 << " -> " << conn->localAddress().toIpPort()
                 << (conn->connected() ? " : online" : " : offline");
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
        // 热路径：零日志，直接回显
        conn->send(buf);
    }

    TcpServer server_;
};

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    EventLoop loop;
    InetAddress addr(port);
    EchoServer server(&loop, addr, "EchoServer");
    server.setThreadNum(4);
    server.start();

    LOG_INFO << "EchoServer listening on port " << port;
    loop.loop();
    return 0;
}
