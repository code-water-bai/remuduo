#include "io_uring/UringTcpServer.h"
#include "io_uring/UringTcpConnection.h"
#include "io_uring/UringEventLoop.h"
#include "coro/Task.h"
#include "net/InetAddress.h"
#include "base/Logging.h"

using namespace new_muduo;
using namespace new_muduo::io_uring;
using namespace new_muduo::coro;

int main(int argc, char* argv[]) {
    uint16_t port = 8082;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    UringEventLoop baseLoop;
    InetAddress addr(port);
    UringTcpServer server(&baseLoop, addr, "UringEchoServer");
    server.setThreadNums(4);

    server.setConnectionHandle(
        [](UringTcpConnection::UringTcpConnectionPtr conn) -> Task<void> {
            LOG_INFO << "UringEchoServer - " << conn->peerAddress().toIpPort()
                     << " connected (fd=" << conn->fd() << ")";

            char buf[65536];
            while (true) {
                ssize_t n = co_await conn->read(buf, sizeof(buf));
                if (n <= 0) break;
                ssize_t res = co_await conn->send(buf, static_cast<int>(n));
                if (res < 0) break;
            }
            co_return;
        });

    auto startTask = server.start(port);
    startTask.handle().resume();

    LOG_INFO << "UringEchoServer listening on port " << port;
    baseLoop.loop();
    return 0;
}
