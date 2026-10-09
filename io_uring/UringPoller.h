#pragma once

#include "IoUring.h"
#include "../base/noncopyable.h"
#include "../base/Timestamp.h"
#include <vector>
#include <coroutine>

namespace new_muduo {
namespace io_uring {

class UringEventLoop;

struct UringContext {
    std::coroutine_handle<> handle_;
    int res = 0;
    int buf_index_ = -1;
    UringEventLoop* loop_ = nullptr;

    explicit UringContext(UringEventLoop* loop) : loop_(loop) {}
};

// ------------------------------------------------------------------
// Single-threaded fixed buffer pool (all operations on loop thread)
// ------------------------------------------------------------------
class FixedBufferPool : noncopyable {
public:
    static constexpr size_t kNumBuffers = 1024;
    static constexpr size_t kBufferSize = 4096;

    FixedBufferPool();

    int acquire();
    void release(int idx);
    void* ptr(int idx) { return buffers_[idx]; }
    const struct iovec* iovecs() const { return iovecs_; }

private:
    alignas(64) char buffers_[kNumBuffers][kBufferSize];
    struct iovec iovecs_[kNumBuffers];
    int free_stack_[kNumBuffers];
    int free_top_ = -1;
};

// ------------------------------------------------------------------
// O(1) fd -> registered file index mapping
// ------------------------------------------------------------------
class FixedFileTable : noncopyable {
public:
    static constexpr int kMaxFiles = 4096;
    static constexpr int kMaxFd = 65536;
    static constexpr int kInvalidIdx = -1;

    FixedFileTable();

    int registerFd(int raw_fd, IoUring* ring);
    void unregisterFd(int raw_fd, IoUring* ring);
    int getIndex(int raw_fd) const;

private:
    int fd_to_idx_[kMaxFd];
    int idx_to_fd_[kMaxFiles];
    int count_ = 0;
};

// ------------------------------------------------------------------
// UringPoller — batch submit + batch reap + direct resume
// ------------------------------------------------------------------
class UringPoller : noncopyable {
public:
    using ChannelList = std::vector<class Channel*>;

    explicit UringPoller(UringEventLoop* loop);
    ~UringPoller();

    Timestamp poll(int timeoutMs, ChannelList* activeChannels);

    // Prepare SQE only (do NOT submit)
    bool prepareRead(int fd, void* buf, size_t count, UringContext* ctx, off_t offset = 0);
    bool prepareWrite(int fd, const void* buf, size_t count, UringContext* ctx, off_t offset = 0);
    bool prepareAccept(int fd, sockaddr* addr, socklen_t* addrlen, UringContext* ctx);

    int registerFd(int raw_fd);
    void unregisterFd(int raw_fd);
    void releaseBuffer(int idx);
    void* bufferPtr(int idx);

    static UringPoller* newPoller(UringEventLoop* loop);

private:
    void handleCompletions();
    void armWakeupPoll();

    UringEventLoop* loop_;
    IoUring ring_;

    FixedBufferPool buffer_pool_;
    bool buffers_registered_ = false;

    FixedFileTable file_table_;
    bool files_registered_ = false;

    bool wakeup_armed_ = false;

    static constexpr unsigned kMaxCqes = 256;
    struct io_uring_cqe* cqes_[kMaxCqes];
};

} // namespace io_uring
} // namespace new_muduo
