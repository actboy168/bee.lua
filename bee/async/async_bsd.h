#pragma once

#include <bee/async/async_types.h>
#include <bee/net/fd.h>
#include <bee/sys/file_handle.h>
#include <bee/utility/span.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bee::net {
    struct endpoint;
}

namespace bee::async {

class async {
public:
    async();
    ~async();

    bool submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id);
    bool submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id);
    bool submit_accept(net::fd_t listen_fd, uint64_t request_id);
    bool submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id);
    bool submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id);
    bool submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id);
    int  poll(const span<io_completion>& completions);
    int  wait(const span<io_completion>& completions, int timeout);
    void stop();

    struct pending_op {
        uint64_t  request_id = 0;
        net::fd_t fd         = net::retired_fd;
        enum type_t : uint8_t {
            read,
            write,
            accept,
            connect,
        } type = read;
        union {
            struct { void*       buffer; size_t len; } r;
            struct { const void* buffer; size_t len; } w;
        };
    };

    static constexpr int kMaxEvents = 64;

private:
    int m_kqfd;
    std::vector<io_completion> m_sync_completions;

    bool kqueue_register(net::fd_t fd, int filter, pending_op* op);
};

}  // namespace bee::async
