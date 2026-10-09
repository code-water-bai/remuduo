#pragma once

#include "../net/EventLoop.h"
#include <memory>

namespace new_muduo {
namespace io_uring {

class UringPoller;
class UringContext;

class UringEventLoop : public EventLoop {
public:
    UringEventLoop();
    ~UringEventLoop() override;

    void loop() override;

    // Prepare SQE only (batch submitted in poll())
    void wait_read(int fd, void* buf, signed len, UringContext* ctx);
    void wait_write(int fd, const void* buf, signed len, UringContext* ctx);
    void wait_accept(int fd, sockaddr* peeraddr, UringContext* ctx, socklen_t* len);

    int registerFd(int raw_fd);
    void unregisterFd(int raw_fd);
    void releaseBuffer(int idx);
    void* bufferPtr(int idx);

private:
    std::unique_ptr<UringPoller> poller_;
};

} // namespace io_uring
} // namespace new_muduo
