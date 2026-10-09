#include "UringPoller.h"
#include "UringEventLoop.h"
#include "../base/Logging.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>

namespace new_muduo {
namespace io_uring {

// ===================================================================
// FixedBufferPool
// ===================================================================
FixedBufferPool::FixedBufferPool() : free_top_(-1) {
    for (size_t i = 0; i < kNumBuffers; ++i) {
        iovecs_[i].iov_base = buffers_[i];
        iovecs_[i].iov_len  = kBufferSize;
    }
    for (int i = static_cast<int>(kNumBuffers) - 1; i >= 0; --i) {
        release(i);
    }
}

int FixedBufferPool::acquire() {
    if (free_top_ < 0) return -1;
    return free_stack_[free_top_--];
}

void FixedBufferPool::release(int idx) {
    free_stack_[++free_top_] = idx;
}

// ===================================================================
// FixedFileTable
// ===================================================================
FixedFileTable::FixedFileTable() : count_(0) {
    for (int i = 0; i < kMaxFd; ++i) fd_to_idx_[i] = kInvalidIdx;
    for (int i = 0; i < kMaxFiles; ++i) idx_to_fd_[i] = -1;
}

int FixedFileTable::registerFd(int raw_fd, IoUring* ring) {
    if (raw_fd < 0 || raw_fd >= kMaxFd || count_ >= kMaxFiles) return kInvalidIdx;
    if (fd_to_idx_[raw_fd] != kInvalidIdx) return fd_to_idx_[raw_fd];

    int idx = count_++;
    idx_to_fd_[idx] = raw_fd;
    fd_to_idx_[raw_fd] = idx;

    if (ring) {
        ring->registerFilesUpdate(idx, &raw_fd, 1);
    }
    return idx;
}

void FixedFileTable::unregisterFd(int raw_fd, IoUring* ring) {
    if (raw_fd < 0 || raw_fd >= kMaxFd) return;
    int idx = fd_to_idx_[raw_fd];
    if (idx == kInvalidIdx) return;

    int placeholder = -1;
    if (ring) {
        ring->registerFilesUpdate(idx, &placeholder, 1);
    }
    fd_to_idx_[raw_fd] = kInvalidIdx;
    idx_to_fd_[idx] = -1;
}

int FixedFileTable::getIndex(int raw_fd) const {
    if (raw_fd < 0 || raw_fd >= kMaxFd) return kInvalidIdx;
    return fd_to_idx_[raw_fd];
}

// ===================================================================
// UringPoller
// ===================================================================
UringPoller::UringPoller(UringEventLoop* loop)
    : loop_(loop),
      ring_(4096, IORING_SETUP_SQPOLL)  // 启用 SQPOLL，内核批量提交
{
    if (!ring_.valid()) {
        LOG_SYSFATAL << "Failed to initialize io_uring";
    }

    buffers_registered_ = ring_.registerBuffers(buffer_pool_.iovecs(), FixedBufferPool::kNumBuffers);
    files_registered_   = ring_.registerFilesSparse(FixedFileTable::kMaxFiles);

    armWakeupPoll();
}

UringPoller::~UringPoller() = default;

Timestamp UringPoller::poll(int timeoutMs, ChannelList* /*activeChannels*/) {
    // 批量提交所有挂起的 SQE，并等待至少 1 个 CQE
    ring_.submitAndWait(1);

    // 收割所有就绪 CQE（批量）
    int count = ring_.peekBatchCqes(cqes_, kMaxCqes);
    if (count > 0) {
        handleCompletions();
        ring_.cqAdvance(count);
    }

    return Timestamp::now();
}

bool UringPoller::prepareRead(int fd, void* buf, size_t count,
                              UringContext* ctx, off_t offset) {
    struct io_uring_sqe* sqe = ring_.getSqe();
    if (!sqe) return false;

    int fidx = files_registered_ ? file_table_.getIndex(fd) : FixedFileTable::kInvalidIdx;

    if (buffers_registered_ && count <= FixedBufferPool::kBufferSize) {
        int bidx = buffer_pool_.acquire();
        if (bidx >= 0) {
            ctx->buf_index_ = bidx;
            void* rbuf = buffer_pool_.ptr(bidx);
            if (fidx >= 0) {
                io_uring_prep_read_fixed(sqe, fidx, rbuf, count, offset, bidx);
                sqe->flags |= IOSQE_FIXED_FILE;
            } else {
                io_uring_prep_read_fixed(sqe, fd, rbuf, count, offset, bidx);
            }
            io_uring_sqe_set_data(sqe, ctx);
            return true;
        }
    }

    if (fidx >= 0) {
        io_uring_prep_read(sqe, fidx, buf, count, offset);
        sqe->flags |= IOSQE_FIXED_FILE;
    } else {
        io_uring_prep_read(sqe, fd, buf, count, offset);
    }
    io_uring_sqe_set_data(sqe, ctx);
    return true;
}

bool UringPoller::prepareWrite(int fd, const void* buf, size_t count,
                               UringContext* ctx, off_t offset) {
    struct io_uring_sqe* sqe = ring_.getSqe();
    if (!sqe) return false;

    int fidx = files_registered_ ? file_table_.getIndex(fd) : FixedFileTable::kInvalidIdx;

    if (buffers_registered_ && count <= FixedBufferPool::kBufferSize) {
        int bidx = buffer_pool_.acquire();
        if (bidx >= 0) {
            ctx->buf_index_ = bidx;
            void* wbuf = buffer_pool_.ptr(bidx);
            std::memcpy(wbuf, buf, count);
            if (fidx >= 0) {
                io_uring_prep_write_fixed(sqe, fidx, wbuf, count, offset, bidx);
                sqe->flags |= IOSQE_FIXED_FILE;
            } else {
                io_uring_prep_write_fixed(sqe, fd, wbuf, count, offset, bidx);
            }
            io_uring_sqe_set_data(sqe, ctx);
            return true;
        }
    }

    if (fidx >= 0) {
        io_uring_prep_write(sqe, fidx, buf, count, offset);
        sqe->flags |= IOSQE_FIXED_FILE;
    } else {
        io_uring_prep_write(sqe, fd, buf, count, offset);
    }
    io_uring_sqe_set_data(sqe, ctx);
    return true;
}

bool UringPoller::prepareAccept(int fd, sockaddr* addr, socklen_t* addrlen,
                                UringContext* ctx) {
    struct io_uring_sqe* sqe = ring_.getSqe();
    if (!sqe) return false;

    int fidx = files_registered_ ? file_table_.getIndex(fd) : FixedFileTable::kInvalidIdx;

    if (fidx >= 0) {
        io_uring_prep_accept(sqe, fidx, addr, addrlen, 0);
        sqe->flags |= IOSQE_FIXED_FILE;
    } else {
        io_uring_prep_accept(sqe, fd, addr, addrlen, 0);
    }
    io_uring_sqe_set_data(sqe, ctx);
    return true;
}

int UringPoller::registerFd(int raw_fd) {
    if (!files_registered_) return -1;
    return file_table_.registerFd(raw_fd, &ring_);
}

void UringPoller::unregisterFd(int raw_fd) {
    if (!files_registered_) return;
    file_table_.unregisterFd(raw_fd, &ring_);
}

void UringPoller::releaseBuffer(int idx) {
    if (idx >= 0) buffer_pool_.release(idx);
}

void* UringPoller::bufferPtr(int idx) {
    return buffer_pool_.ptr(idx);
}

UringPoller* UringPoller::newPoller(UringEventLoop* loop) {
    return new UringPoller(loop);
}

void UringPoller::armWakeupPoll() {
    int fd = loop_->wakeupFd();
    struct io_uring_sqe* sqe = ring_.getSqe();
    if (!sqe) {
        LOG_ERROR << "UringPoller: failed to get SQE for wakeup poll";
        return;
    }
#ifdef IORING_POLL_ADD_MULTI
    io_uring_prep_poll_multishot(sqe, fd, POLLIN);
#else
    io_uring_prep_poll_add(sqe, fd, POLLIN);
#endif
    io_uring_sqe_set_data(sqe, nullptr);  // nullptr = wakeup
    wakeup_armed_ = true;
    ring_.submit();
}

void UringPoller::handleCompletions() {
    for (int i = 0; i < kMaxCqes; ++i) {
        struct io_uring_cqe* cqe = cqes_[i];
        if (!cqe) break;

        UringContext* ctx = static_cast<UringContext*>(io_uring_cqe_get_data(cqe));

        if (ctx == nullptr) {
            // Wakeup eventfd — read to clear
            uint64_t dummy;
            ::read(loop_->wakeupFd(), &dummy, sizeof(dummy));

#ifdef IORING_POLL_ADD_MULTI
            if (!(cqe->flags & IORING_CQE_F_MORE)) {
                wakeup_armed_ = false;
                armWakeupPoll();
            }
#else
            wakeup_armed_ = false;
            armWakeupPoll();
#endif
            continue;
        }

        ctx->res = cqe->res;

        // 直接 resume — io_uring completion 始终发生在 loop 线程
        if (ctx->handle_) {
            ctx->handle_.resume();
        }
    }
}

} // namespace io_uring
} // namespace new_muduo
