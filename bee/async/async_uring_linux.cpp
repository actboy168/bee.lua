#include <bee/async/async_uring_linux.h>

#include <bee/async/async.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>

#include <liburing.h>
#include <cstring>
#include <unistd.h>

namespace bee::async {

static constexpr unsigned kEntries = 256;

// Pack op type into the high 8 bits of user_data; request_id uses the low 56 bits.
static constexpr uint64_t kOpShift = 56;
static constexpr uint64_t kIdMask  = (uint64_t(1) << kOpShift) - 1;

static inline uint64_t pack_user_data(async_op op, uint64_t request_id) noexcept {
    return (static_cast<uint64_t>(op) << kOpShift) | (request_id & kIdMask);
}

static inline async_op unpack_op(uint64_t user_data) noexcept {
    return static_cast<async_op>(user_data >> kOpShift);
}

static inline uint64_t unpack_id(uint64_t user_data) noexcept {
    return user_data & kIdMask;
}

async_uring::async_uring()
    : m_ring(new io_uring{}) {
    if (io_uring_queue_init(kEntries, m_ring, 0) != 0) {
        delete m_ring;
        m_ring = nullptr;
    }
}

async_uring::~async_uring() {
    stop();
}

bool async_uring::submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id) {
    if (!m_ring) return false;
    struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
    if (!sqe) return false;
    io_uring_prep_recv(sqe, fd, static_cast<char*>(buffer), len, 0);
    sqe->user_data = pack_user_data(async_op::read, request_id);
    return true;  // caller drives submit via poll()/wait()
}

bool async_uring::submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id) {
    if (!m_ring) return false;
    struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
    if (!sqe) return false;
    io_uring_prep_send(sqe, fd, static_cast<const char*>(buffer), len, 0);
    sqe->user_data = pack_user_data(async_op::write, request_id);
    return true;
}

bool async_uring::submit_accept(net::fd_t listen_fd, uint64_t request_id) {
    if (!m_ring) return false;
    struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
    if (!sqe) return false;
    io_uring_prep_accept(sqe, listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    sqe->user_data = pack_user_data(async_op::accept, request_id);
    return true;
}

bool async_uring::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
    if (!m_ring) return false;
    struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
    if (!sqe) return false;
    io_uring_prep_connect(sqe, fd, ep.addr(), ep.addrlen());
    sqe->user_data = pack_user_data(async_op::connect, request_id);
    return true;
}

bool async_uring::submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) {
    if (!m_ring) return false;
    struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
    if (!sqe) return false;
    io_uring_prep_read(sqe, fd, buffer, len, offset);
    sqe->user_data = pack_user_data(async_op::file_read, request_id);
    return true;
}

bool async_uring::submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) {
    if (!m_ring) return false;
    struct io_uring_sqe* sqe = io_uring_get_sqe(m_ring);
    if (!sqe) return false;
    io_uring_prep_write(sqe, fd, buffer, len, offset);
    sqe->user_data = pack_user_data(async_op::file_write, request_id);
    return true;
}

static io_completion make_completion(struct io_uring_cqe* cqe) {
    io_completion c;
    c.op         = unpack_op(cqe->user_data);
    c.request_id = unpack_id(cqe->user_data);
    if (cqe->res > 0) {
        c.status            = async_status::success;
        c.bytes_transferred = static_cast<size_t>(cqe->res);
        c.error_code        = 0;
    } else if (cqe->res == 0) {
        // recv returned 0: peer closed the connection (EOF)
        c.status            = async_status::close;
        c.bytes_transferred = 0;
        c.error_code        = 0;
    } else {
        c.status            = async_status::error;
        c.bytes_transferred = 0;
        c.error_code        = -cqe->res;
    }
    return c;
}

int async_uring::poll(const span<io_completion>& completions) {
    if (!m_ring) return 0;
    // Submit any pending SQEs before harvesting completions.
    io_uring_submit(m_ring);
    struct io_uring_cqe* cqe = nullptr;
    unsigned head;
    unsigned count = 0;
    io_uring_for_each_cqe(m_ring, head, cqe) {
        if (count >= completions.size()) break;
        completions[count++] = make_completion(cqe);
    }
    if (count > 0) {
        io_uring_cq_advance(m_ring, count);
    }
    return static_cast<int>(count);
}

int async_uring::wait(const span<io_completion>& completions, int timeout) {
    if (!m_ring) return 0;
    // Submit any pending SQEs before waiting.
    io_uring_submit(m_ring);
    struct __kernel_timespec ts;
    struct __kernel_timespec* tsp = nullptr;
    if (timeout >= 0) {
        ts.tv_sec  = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000L;
        tsp        = &ts;
    }
    struct io_uring_cqe* cqe = nullptr;
    // wait_nr=1: block until at least one completion is available.
    int ret = io_uring_wait_cqes(m_ring, &cqe, 1, tsp, nullptr);
    if (ret < 0) {
        return 0;
    }
    unsigned count = 0;
    unsigned head;
    io_uring_for_each_cqe(m_ring, head, cqe) {
        if (count >= completions.size()) break;
        completions[count++] = make_completion(cqe);
    }
    if (count > 0) {
        io_uring_cq_advance(m_ring, count);
    }
    return static_cast<int>(count);
}

void async_uring::stop() {
    if (m_ring) {
        io_uring_queue_exit(m_ring);
        delete m_ring;
        m_ring = nullptr;
    }
}

}  // namespace bee::async
