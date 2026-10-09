#pragma once

#include <liburing.h>
#include <cstddef>

namespace new_muduo {
namespace io_uring {

class IoUring {
public:
    explicit IoUring(unsigned entries = 4096, unsigned flags = 0);
    ~IoUring();

    bool valid() const { return valid_; }

    struct io_uring_sqe* getSqe();
    int submit();
    int submitAndWait(unsigned waitNr);

    int peekBatchCqes(struct io_uring_cqe** cqes, unsigned count);
    void cqAdvance(unsigned count);

    bool registerBuffers(const struct iovec* iovecs, unsigned nr);
    bool registerFilesSparse(unsigned nr);
    bool registerFilesUpdate(unsigned off, const int* fds, unsigned nr);

    struct io_uring* ring() { return &ring_; }

private:
    struct io_uring ring_;
    bool valid_ = false;
};

} // namespace io_uring
} // namespace new_muduo
