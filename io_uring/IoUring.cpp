#include "IoUring.h"
#include "../base/Logging.h"
#include <cstring>

namespace new_muduo {
namespace io_uring {

IoUring::IoUring(unsigned entries, unsigned flags) {
    struct io_uring_params p = {};
    p.flags = flags;
    if (flags & IORING_SETUP_SQPOLL) {
        p.sq_thread_idle = 2000;
    }
    int ret = io_uring_queue_init_params(entries, &ring_, &p);
    if (ret < 0) {
        LOG_ERROR << "io_uring_queue_init failed: " << strerror(-ret);
        return;
    }
    valid_ = true;
}

IoUring::~IoUring() {
    if (valid_) {
        io_uring_queue_exit(&ring_);
    }
}

struct io_uring_sqe* IoUring::getSqe() {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        submit();
        sqe = io_uring_get_sqe(&ring_);
    }
    return sqe;
}

int IoUring::submit() {
    int ret = io_uring_submit(&ring_);
    if (ret < 0) {
        LOG_ERROR << "io_uring_submit failed: " << strerror(-ret);
    }
    return ret;
}

int IoUring::submitAndWait(unsigned waitNr) {
    int ret = io_uring_submit_and_wait(&ring_, waitNr);
    if (ret < 0) {
        LOG_ERROR << "io_uring_submit_and_wait failed: " << strerror(-ret);
    }
    return ret;
}

int IoUring::peekBatchCqes(struct io_uring_cqe** cqes, unsigned count) {
    return io_uring_peek_batch_cqe(&ring_, cqes, count);
}

void IoUring::cqAdvance(unsigned count) {
    if (count > 0) {
        io_uring_cq_advance(&ring_, count);
    }
}

bool IoUring::registerBuffers(const struct iovec* iovecs, unsigned nr) {
    int ret = io_uring_register_buffers(&ring_, iovecs, nr);
    if (ret < 0) {
        LOG_WARN << "io_uring_register_buffers failed: " << strerror(-ret);
        return false;
    }
    return true;
}

bool IoUring::registerFilesSparse(unsigned nr) {
    int ret = io_uring_register_files_sparse(&ring_, nr);
    if (ret < 0) {
        LOG_WARN << "io_uring_register_files_sparse failed: " << strerror(-ret);
        return false;
    }
    return true;
}

bool IoUring::registerFilesUpdate(unsigned off, const int* fds, unsigned nr) {
    int ret = io_uring_register_files_update(&ring_, off, fds, nr);
    if (ret < 0) {
        LOG_WARN << "io_uring_register_files_update failed: " << strerror(-ret);
        return false;
    }
    return true;
}

} // namespace io_uring
} // namespace new_muduo
