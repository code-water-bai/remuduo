#include "coro/CoroTcpServer.h"
#include "coro/CoroTcpConnection.h"
#include "coro/CoroEventLoop.h"
#include "coro/Task.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "base/Logging.h"

using namespace new_muduo;
using namespace new_muduo::coro;

int main(int argc, char* argv[]) {
    uint16_t port = 8081;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    EventLoop rawLoop;
    CoroEventLoop loop(&rawLoop);
    InetAddress addr(port);
    CoroTcpServer server(&loop, addr, "CoroEchoServer");

    server.setConnectionHandler(
        [](CoroTcpConnectionPtr conn) -> Task<void> {
            LOG_INFO << "CoroEchoServer - " << conn->peerAddress().toIpPort()
                     << " connected (fd=" << conn->fd() << ")";

            char buf[65536];
            while (true) {
                ssize_t n = co_await conn->read(buf, sizeof(buf));
                if (n <= 0) break;
                conn->send(buf, static_cast<size_t>(n));
            }
            co_return;
        });

    server.setThreadNum(4);
    server.start();

    LOG_INFO << "CoroEchoServer listening on port " << port;
    loop.loop();
    return 0;
}
