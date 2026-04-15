#include <bee/async/async.h>
#include <bee/async/async_uring_linux.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/utility/dynarray.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <poll.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <memory>
#include <unordered_map>

// ---- io_uring ABI definitions (no dependency on liburing or <linux/io_uring.h>) ----
//
// All constants and struct layouts are taken directly from the Linux UAPI headers
// (linux/io_uring.h) and verified against the kernel source.  Static assertions
// below guard the struct layouts so a mismatch is caught at compile time.

#ifndef __NR_io_uring_setup
#    define __NR_io_uring_setup 425
#endif
#ifndef __NR_io_uring_enter
#    define __NR_io_uring_enter 426
#endif

// io_uring_setup flags
enum {
    BEE__IORING_SETUP_NO_SQARRAY = 0x10000u,  // kernel 6.6+: sq_array is implicit
};

// io_uring feature flags (returned in io_uring_params.features)
enum {
    BEE__IORING_FEAT_SINGLE_MMAP = 1u,  // SQ+CQ share a single mmap region
    BEE__IORING_FEAT_NODROP      = 2u,  // CQ overflow is never silently dropped
};

// io_uring_enter flags
enum {
    BEE__IORING_ENTER_GETEVENTS = 1u,
    BEE__IORING_ENTER_EXT_ARG   = 8u,  // arg is io_uring_getevents_arg (kernel 5.11+)
};

// sq_ring flags (iou->sqflags)
enum {
    BEE__IORING_SQ_CQ_OVERFLOW = 2u,
};

// Opcodes we use
enum {
    BEE__IORING_OP_ACCEPT  = 13,
    BEE__IORING_OP_CONNECT = 16,
    BEE__IORING_OP_READ    = 22,
    BEE__IORING_OP_WRITE   = 23,
    BEE__IORING_OP_SEND    = 26,
    BEE__IORING_OP_RECV    = 27,
    BEE__IORING_OP_SENDMSG  = 9,
    BEE__IORING_OP_RECVMSG  = 10,
    BEE__IORING_OP_POLL_ADD = 6,
};

struct bee__io_sqring_offsets {
    uint32_t head;
    uint32_t tail;
    uint32_t ring_mask;
    uint32_t ring_entries;
    uint32_t flags;
    uint32_t dropped;
    uint32_t array;
    uint32_t reserved0;
    uint64_t reserved1;
};
static_assert(40 == sizeof(bee__io_sqring_offsets), "sqring_offsets size");

struct bee__io_cqring_offsets {
    uint32_t head;
    uint32_t tail;
    uint32_t ring_mask;
    uint32_t ring_entries;
    uint32_t overflow;
    uint32_t cqes;
    uint64_t reserved0;
    uint64_t reserved1;
};
static_assert(40 == sizeof(bee__io_cqring_offsets), "cqring_offsets size");

struct bee__io_uring_sqe {
    uint8_t opcode;
    uint8_t flags;
    uint16_t ioprio;
    int32_t fd;
    union {
        uint64_t off;
        uint64_t addr2;
    };
    union {
        uint64_t addr;
    };
    uint32_t len;
    union {
        uint32_t rw_flags;
        uint32_t fsync_flags;
        uint32_t open_flags;
        uint32_t statx_flags;
        uint32_t accept_flags;  // used by IORING_OP_ACCEPT
        uint32_t msg_flags;     // used by IORING_OP_SEND / RECV
    };
    uint64_t user_data;
    union {
        uint16_t buf_index;
        uint64_t pad[3];
    };
};
static_assert(64 == sizeof(bee__io_uring_sqe), "sqe size");
static_assert(0 == __builtin_offsetof(bee__io_uring_sqe, opcode), "sqe.opcode");
static_assert(4 == __builtin_offsetof(bee__io_uring_sqe, fd), "sqe.fd");
static_assert(8 == __builtin_offsetof(bee__io_uring_sqe, off), "sqe.off");
static_assert(16 == __builtin_offsetof(bee__io_uring_sqe, addr), "sqe.addr");
static_assert(24 == __builtin_offsetof(bee__io_uring_sqe, len), "sqe.len");
static_assert(28 == __builtin_offsetof(bee__io_uring_sqe, rw_flags), "sqe.rw_flags");
static_assert(32 == __builtin_offsetof(bee__io_uring_sqe, user_data), "sqe.user_data");
static_assert(40 == __builtin_offsetof(bee__io_uring_sqe, buf_index), "sqe.buf_index");

struct bee__io_uring_cqe {
    uint64_t user_data;
    int32_t res;
    uint32_t flags;
};
static_assert(16 == sizeof(bee__io_uring_cqe), "cqe size");

struct bee__io_uring_params {
    uint32_t sq_entries;
    uint32_t cq_entries;
    uint32_t flags;
    uint32_t sq_thread_cpu;
    uint32_t sq_thread_idle;
    uint32_t features;
    uint32_t reserved[4];
    bee__io_sqring_offsets sq_off;  // 40 bytes
    bee__io_cqring_offsets cq_off;  // 40 bytes
};
static_assert(40 + 40 + 40 == sizeof(bee__io_uring_params), "params size");
static_assert(40 == __builtin_offsetof(bee__io_uring_params, sq_off), "params.sq_off");
static_assert(80 == __builtin_offsetof(bee__io_uring_params, cq_off), "params.cq_off");

// Used with IORING_ENTER_EXT_ARG to pass a timeout directly to io_uring_enter.
struct bee__io_uring_getevents_arg {
    uint64_t sigmask;
    uint32_t sigmask_sz;
    uint32_t pad;
    uint64_t ts;  // pointer to __kernel_timespec
};

struct bee__kernel_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

// ---- raw syscall wrappers ----

static inline int sys_io_uring_setup(unsigned entries, bee__io_uring_params* p) noexcept {
    return static_cast<int>(syscall(__NR_io_uring_setup, entries, p));
}

static inline int sys_io_uring_enter(int fd, unsigned to_submit, unsigned min_complete, unsigned flags, const void* arg) noexcept {
    const unsigned arg_size = (flags & BEE__IORING_ENTER_EXT_ARG)
        ? static_cast<unsigned>(sizeof(bee__io_uring_getevents_arg))
        : 0u;
    return static_cast<int>(syscall(__NR_io_uring_enter, fd, to_submit, min_complete, flags, arg, arg_size));
}

// ---- io_uring ring state (kept behind the forward-declared pointer in the header) ----

// Context kept alive on the heap for the duration of a SENDMSG operation.
// msghdr.msg_iov points into bufs[], so both must outlive the CQE.
struct writev_ctx {
    struct msghdr msg = {};
    bee::dynarray<bee::net::socket::iobuf> bufs;
    explicit writev_ctx(bee::span<const bee::net::socket::iobuf> src)
        : bufs(src.size()) {
        for (size_t i = 0; i < src.size(); ++i) bufs[i] = src[i];
        msg.msg_iov    = reinterpret_cast<struct iovec*>(bufs.data());
        msg.msg_iovlen = static_cast<int>(src.size());
    }
};

struct readv_ctx {
    struct msghdr msg = {};
    bee::dynarray<bee::net::socket::iobuf> bufs;
    explicit readv_ctx(bee::span<const bee::net::socket::iobuf> src)
        : bufs(src.size()) {
        for (size_t i = 0; i < src.size(); ++i) bufs[i] = src[i];
        msg.msg_iov    = reinterpret_cast<struct iovec*>(bufs.data());
        msg.msg_iovlen = static_cast<int>(src.size());
    }
};

struct io_uring {
    int ringfd            = -1;
    char* sq              = nullptr;  // base of the shared SQ+CQ mmap
    size_t maxlen         = 0;
    bee__io_uring_sqe* sqe = nullptr;
    size_t sqelen         = 0;

    // SQ ring pointers into sq mmap
    uint32_t* sqhead  = nullptr;  // kernel consumer
    uint32_t* sqtail  = nullptr;  // we publish here
    uint32_t* sqflags = nullptr;  // SQ_NEED_WAKEUP / SQ_CQ_OVERFLOW flags
    uint32_t sqmask   = 0;

    // CQ ring pointers into sq mmap
    uint32_t* cqhead       = nullptr;  // we advance (consumer)
    uint32_t* cqtail       = nullptr;  // kernel publishes here
    uint32_t cqmask        = 0;
    bee__io_uring_cqe* cqes = nullptr;

    // Pending writev contexts keyed by request_id, freed when CQE arrives.
    std::unordered_map<uint64_t, std::unique_ptr<writev_ctx>> writev_pending;
    // Pending readv contexts keyed by request_id, freed when CQE arrives.
    std::unordered_map<uint64_t, std::unique_ptr<readv_ctx>> readv_pending;
};

namespace bee::async {

    static constexpr uint32_t kEntries = 256;

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

    // ---- atomic helpers (matching libuv's acquire/release ordering) ----

    static inline uint32_t load_acquire(const uint32_t* p) noexcept {
        return __atomic_load_n(p, __ATOMIC_ACQUIRE);
    }

    static inline void store_release(uint32_t* p, uint32_t v) noexcept {
        __atomic_store_n(p, v, __ATOMIC_RELEASE);
    }

    // ---- ring init / exit ----

    static bool uring_init(uint32_t entries, io_uring* ring) noexcept {
        bee__io_uring_params params;
        memset(&params, 0, sizeof(params));

        // On kernel 6.6+ the kernel can omit the sq_array indirection via
        // IORING_SETUP_NO_SQARRAY.  We intentionally do not request that flag here:
        // unknown setup flags may be rejected on older kernels, and the existing
        // sq_array initialisation path already works for both layouts.
        int ringfd = sys_io_uring_setup(entries, &params);
        if (ringfd < 0) return false;

        // Require only the features that are actually used below:
        // SINGLE_MMAP (Linux 5.4+) and NODROP (Linux 5.5+).
        if (!(params.features & BEE__IORING_FEAT_SINGLE_MMAP)) {
            close(ringfd);
            return false;
        }
        if (!(params.features & BEE__IORING_FEAT_NODROP)) {
            close(ringfd);
            return false;
        }

        // SQ+CQ share one mmap (SINGLE_MMAP): use the larger of the two regions.
        size_t sqlen  = params.sq_off.array + params.sq_entries * sizeof(uint32_t);
        size_t cqlen  = params.cq_off.cqes + params.cq_entries * sizeof(bee__io_uring_cqe);
        size_t maxlen = sqlen < cqlen ? cqlen : sqlen;
        size_t sqelen = params.sq_entries * sizeof(bee__io_uring_sqe);

        char* sq = static_cast<char*>(
            mmap(nullptr, maxlen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ringfd, 0 /* IORING_OFF_SQ_RING */)
        );
        if (sq == MAP_FAILED) {
            close(ringfd);
            return false;
        }

        bee__io_uring_sqe* sqe_ptr = static_cast<bee__io_uring_sqe*>(
            mmap(nullptr, sqelen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ringfd, 0x10000000ull /* IORING_OFF_SQES */)
        );
        if (sqe_ptr == MAP_FAILED) {
            munmap(sq, maxlen);
            close(ringfd);
            return false;
        }

        ring->ringfd = ringfd;
        ring->sq     = sq;
        ring->maxlen = maxlen;
        ring->sqe    = sqe_ptr;
        ring->sqelen = sqelen;

        ring->sqhead  = reinterpret_cast<uint32_t*>(sq + params.sq_off.head);
        ring->sqtail  = reinterpret_cast<uint32_t*>(sq + params.sq_off.tail);
        ring->sqflags = reinterpret_cast<uint32_t*>(sq + params.sq_off.flags);
        ring->sqmask  = *reinterpret_cast<uint32_t*>(sq + params.sq_off.ring_mask);

        ring->cqhead = reinterpret_cast<uint32_t*>(sq + params.cq_off.head);
        ring->cqtail = reinterpret_cast<uint32_t*>(sq + params.cq_off.tail);
        ring->cqmask = *reinterpret_cast<uint32_t*>(sq + params.cq_off.ring_mask);
        ring->cqes   = reinterpret_cast<bee__io_uring_cqe*>(sq + params.cq_off.cqes);

        // Pre-fill sq_array with the identity mapping (slot i -> SQE i).
        // On kernels that set NO_SQARRAY the kernel ignores this array, but
        // populating it is harmless and keeps a single code path.
        if (!(params.flags & BEE__IORING_SETUP_NO_SQARRAY)) {
            uint32_t* sqarray = reinterpret_cast<uint32_t*>(sq + params.sq_off.array);
            for (uint32_t i = 0; i <= ring->sqmask; i++)
                sqarray[i] = i;
        }

        return true;
    }

    static void uring_exit(io_uring* ring) noexcept {
        if (ring->ringfd < 0) return;
        munmap(ring->sqe, ring->sqelen);
        munmap(ring->sq, ring->maxlen);
        close(ring->ringfd);
        ring->ringfd = -1;
    }

    // ---- SQE helpers ----

    // Returns the next free SQE slot, or nullptr if the SQ is full.
    // The caller fills the SQE and then calls uring_submit().
    static inline bee__io_uring_sqe* uring_get_sqe(io_uring* ring) noexcept {
        uint32_t head = load_acquire(ring->sqhead);
        uint32_t tail = *ring->sqtail;
        uint32_t mask = ring->sqmask;

        // Ring is full only when the number of in-flight SQEs reaches capacity.
        if ((tail - head) >= (mask + 1))
            return nullptr;

        uint32_t slot         = tail & mask;
        bee__io_uring_sqe* sqe = &ring->sqe[slot];
        memset(sqe, 0, sizeof(*sqe));
        return sqe;
    }

    // Publish one new SQE to the kernel by advancing sqtail (release ordering).
    // If SQPOLL is not in use this is sufficient; io_uring_enter drives submission.
    static inline void uring_submit(io_uring* ring) noexcept {
        store_release(ring->sqtail, *ring->sqtail + 1);
    }

    // Return the number of SQEs published but not yet consumed by the kernel.
    static inline uint32_t uring_pending(const io_uring* ring) noexcept {
        return *ring->sqtail - load_acquire(ring->sqhead);
    }

    // ---- CQE harvesting ----

    int async_uring::harvest_cqes(const span<io_completion>& completions) noexcept {
        io_uring* ring = m_ring;
        uint32_t head  = *ring->cqhead;
        uint32_t tail  = load_acquire(ring->cqtail);
        uint32_t mask  = ring->cqmask;
        uint32_t count = 0;

        while (head != tail && count < static_cast<uint32_t>(completions.size())) {
            const bee__io_uring_cqe& cqe = ring->cqes[head & mask];
            io_completion& c            = completions[count++];
            c.op                        = unpack_op(cqe.user_data);
            c.request_id                = unpack_id(cqe.user_data);
            // Free the writev context (msghdr + iobuf array) once the CQE arrives.
            if (c.op == async_op::writev) {
                ring->writev_pending.erase(c.request_id);
            }
            // Free the readv context once the CQE arrives.
            if (c.op == async_op::readv) {
                ring->readv_pending.erase(c.request_id);
            }
            // For connect/file_write/accept/fd_poll, res==0 means success (not EOF).
            // For read/write (recv/send), res==0 means the peer closed the connection.
            bool zero_is_success = (c.op == async_op::connect || c.op == async_op::writev || c.op == async_op::file_write || c.op == async_op::accept || c.op == async_op::fd_poll);
            if (cqe.res > 0) {
                c.status            = async_status::success;
                // For fd_poll, cqe.res is the revents mask (e.g. POLLIN=1), not a byte count.
                c.bytes_transferred = (c.op == async_op::fd_poll) ? 0 : static_cast<size_t>(cqe.res);
                c.error_code        = 0;
            } else if (cqe.res == 0) {
                if (zero_is_success) {
                    c.status            = async_status::success;
                    c.bytes_transferred = 0;
                    c.error_code        = 0;
                } else {
                    c.status            = async_status::close;
                    c.bytes_transferred = 0;
                    c.error_code        = 0;
                }
            } else {
                c.status            = async_status::error;
                c.bytes_transferred = 0;
                c.error_code        = -cqe.res;
            }
            head++;
        }

        if (count > 0)
            store_release(ring->cqhead, head);

        // If the CQ overflowed, poke the kernel to flush the overflow list.
        // We don't grab the new entries here — they'll appear in the next poll/wait.
        if (load_acquire(ring->sqflags) & BEE__IORING_SQ_CQ_OVERFLOW) {
            int rc;
            do {
                rc = sys_io_uring_enter(ring->ringfd, 0, 0, BEE__IORING_ENTER_GETEVENTS, nullptr);
            } while (rc == -1 && errno == EINTR);
        }

        return static_cast<int>(count);
    }

    // ---- async_uring public interface ----

    async_uring::async_uring()
        : m_ring(new io_uring {}) {
        if (!uring_init(kEntries, m_ring)) {
            delete m_ring;
            m_ring = nullptr;
        }
    }

    async_uring::~async_uring() {
        stop();
    }

    bool async_uring::submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id) {
        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        sqe->opcode    = BEE__IORING_OP_RECV;
        sqe->fd        = fd;
        sqe->addr      = reinterpret_cast<uintptr_t>(buffer);
        sqe->len       = static_cast<uint32_t>(len);
        sqe->msg_flags = 0;
        sqe->user_data = pack_user_data(async_op::read, request_id);
        uring_submit(m_ring);
        return true;  // SQE queued; will be submitted on next poll/wait
    }

    bool async_uring::submit_readv(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) {
        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        auto ctx       = std::make_unique<readv_ctx>(bufs);
        sqe->opcode    = BEE__IORING_OP_RECVMSG;
        sqe->fd        = fd;
        sqe->addr      = reinterpret_cast<uintptr_t>(&ctx->msg);
        sqe->len       = 1;
        sqe->msg_flags = 0;
        sqe->user_data = pack_user_data(async_op::readv, request_id);
        m_ring->readv_pending.emplace(request_id, std::move(ctx));
        uring_submit(m_ring);
        return true;
    }

    bool async_uring::submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id) {        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        sqe->opcode    = BEE__IORING_OP_SEND;
        sqe->fd        = fd;
        sqe->addr      = reinterpret_cast<uintptr_t>(buffer);
        sqe->len       = static_cast<uint32_t>(len);
        sqe->msg_flags = 0;
        sqe->user_data = pack_user_data(async_op::write, request_id);
        uring_submit(m_ring);
        return true;  // SQE queued; will be submitted on next poll/wait
    }

    bool async_uring::submit_writev(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) {
        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        auto ctx       = std::make_unique<writev_ctx>(bufs);
        sqe->opcode    = BEE__IORING_OP_SENDMSG;
        sqe->fd        = fd;
        sqe->addr      = reinterpret_cast<uintptr_t>(&ctx->msg);
        sqe->len       = 1;
        sqe->msg_flags = 0;
        sqe->user_data = pack_user_data(async_op::writev, request_id);
        m_ring->writev_pending.emplace(request_id, std::move(ctx));
        uring_submit(m_ring);
        return true;
    }

    bool async_uring::submit_accept(net::fd_t listen_fd, uint64_t request_id) {
        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        sqe->opcode       = BEE__IORING_OP_ACCEPT;
        sqe->fd           = listen_fd;
        sqe->addr         = 0;  // don't capture peer address
        sqe->addr2        = 0;  // no socklen_t output
        sqe->accept_flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
        sqe->user_data    = pack_user_data(async_op::accept, request_id);
        uring_submit(m_ring);
        return true;  // SQE queued; will be submitted on next poll/wait
    }

    bool async_uring::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        // The caller (Lua binding) pins the endpoint in the buf table, guaranteeing
        // ep.addr() remains valid until the CQE is harvested.
        sqe->opcode    = BEE__IORING_OP_CONNECT;
        sqe->fd        = fd;
        sqe->addr      = reinterpret_cast<uintptr_t>(ep.addr());
        sqe->off       = ep.addrlen();  // CONNECT stores addrlen in the off field
        sqe->user_data = pack_user_data(async_op::connect, request_id);
        uring_submit(m_ring);
        return true;  // SQE queued; will be submitted on next poll/wait
    }

    bool async_uring::submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) {
        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        sqe->opcode    = BEE__IORING_OP_READ;
        sqe->fd        = fd;
        sqe->addr      = reinterpret_cast<uintptr_t>(buffer);
        sqe->len       = static_cast<uint32_t>(len);
        sqe->off       = static_cast<uint64_t>(offset);
        sqe->user_data = pack_user_data(async_op::file_read, request_id);
        uring_submit(m_ring);
        return true;  // SQE queued; will be submitted on next poll/wait
    }

    bool async_uring::submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) {
        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        sqe->opcode    = BEE__IORING_OP_WRITE;
        sqe->fd        = fd;
        sqe->addr      = reinterpret_cast<uintptr_t>(buffer);
        sqe->len       = static_cast<uint32_t>(len);
        sqe->off       = static_cast<uint64_t>(offset);
        sqe->user_data = pack_user_data(async_op::file_write, request_id);
        uring_submit(m_ring);
        return true;  // SQE queued; will be submitted on next poll/wait
    }

    bool async_uring::submit_poll(net::fd_t fd, uint64_t request_id) {
        if (!m_ring) return false;
        bee__io_uring_sqe* sqe = uring_get_sqe(m_ring);
        if (!sqe) return false;
        sqe->opcode    = BEE__IORING_OP_POLL_ADD;
        sqe->fd        = fd;
        sqe->rw_flags  = POLLIN;  // 监听可读事件
        sqe->user_data = pack_user_data(async_op::fd_poll, request_id);
        uring_submit(m_ring);
        return true;
    }

    int async_uring::poll(const span<io_completion>& completions) {
        if (!m_ring) return 0;
        return wait(completions, 0);
    }

    int async_uring::wait(const span<io_completion>& completions, int timeout) {
        if (!m_ring) return 0;
        // Submit any pending SQEs and wait for at least one CQE in a single syscall.
        uint32_t pending = uring_pending(m_ring);

        if (timeout == 0) {
            // Non-blocking: flush pending SQEs then harvest whatever is already done.
            if (pending > 0) {
                int ret;
                do {
                    ret = sys_io_uring_enter(m_ring->ringfd, pending, 0, 0, nullptr);
                } while (ret == -1 && errno == EINTR);
            }
            return harvest_cqes(completions);
        } else if (timeout > 0) {
            // Use IORING_ENTER_EXT_ARG (kernel 5.11+) to pass the timeout inline.
            bee__kernel_timespec ts;
            ts.tv_sec  = timeout / 1000;
            ts.tv_nsec = static_cast<int64_t>(timeout % 1000) * 1000000L;
            bee__io_uring_getevents_arg arg;
            memset(&arg, 0, sizeof(arg));
            arg.ts = reinterpret_cast<uintptr_t>(&ts);
            int ret;
            do {
                ret = sys_io_uring_enter(m_ring->ringfd, pending, 1, BEE__IORING_ENTER_GETEVENTS | BEE__IORING_ENTER_EXT_ARG, &arg);
            } while (ret == -1 && errno == EINTR);
            // errno == ETIME: timeout expired with 0 completions; harvest anyway.
        } else {
            // Block until at least one CQE is available, submitting pending SQEs atomically.
            int ret;
            do {
                ret = sys_io_uring_enter(m_ring->ringfd, pending, 1, BEE__IORING_ENTER_GETEVENTS, nullptr);
            } while (ret == -1 && errno == EINTR);
        }

        return harvest_cqes(completions);
    }

    void async_uring::stop() {
        if (m_ring) {
            uring_exit(m_ring);
            delete m_ring;
            m_ring = nullptr;
        }
    }

    void async_uring::cancel(net::fd_t /*fd*/) {
        // 这是一个空操作。对于 io_uring，这个调用不会取消挂起的操作。
        // 当文件描述符关闭时，这些操作将被内核取消，并最终以 ECANCELED 错误完成。
        // 要实现立即取消，需要使用 IORING_OP_ASYNC_CANCEL。
    }

}  // namespace bee::async
