#pragma once

#include <bee/async/async_types.h>
#include <bee/net/fd.h>
#include <bee/net/socket.h>
#include <bee/sys/file_handle.h>
#include <bee/utility/dynarray.h>
#include <bee/utility/span.h>
#include <dispatch/dispatch.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace bee::net {
    struct endpoint;
}

namespace bee::async {

    class async {
    public:
        async();
        ~async();

        bool submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id);
        bool submit_readv(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id);
        bool submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id);
        bool submit_writev(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id);
        bool submit_accept(net::fd_t listen_fd, uint64_t request_id);
        bool submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id);
        bool submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id);
        bool submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id);
        bool associate(net::fd_t fd);
        bool submit_poll(net::fd_t fd, uint64_t request_id);
        int poll(const span<io_completion>& completions);
        int wait(const span<io_completion>& completions, int timeout);
        void cancel(net::fd_t fd);
        void stop();

    private:
        dispatch_queue_t m_queue;
        dispatch_semaphore_t m_signal;
        std::deque<io_completion> m_completions;  // only accessed on m_queue
        bool m_stopped;

        // Per-fd persistent read/write sources (reused via suspend/resume)
        struct fd_sources {
            dispatch_source_t read_src  = nullptr;
            dispatch_source_t write_src = nullptr;
            // Current pending op for each direction (updated before resume)
            struct read_op {
                void*    buffer     = nullptr;
                size_t   len        = 0;
                uint64_t request_id = 0;
                bool     pending    = false;
                bool     is_readv   = false;
                dynarray<net::socket::iobuf> iov;
            } r;
            struct write_op {
                const void* buffer     = nullptr;
                size_t      len        = 0;
                uint64_t    request_id = 0;
                bool        pending    = false;
                bool        is_writev  = false;
                dynarray<net::socket::iobuf> iov;
            } w;
        };

        std::unordered_map<net::fd_t, fd_sources*> m_fd_map;

        fd_sources* get_or_create(net::fd_t fd);
        void release_fd(net::fd_t fd);

        // Enqueue a completion from the GCD queue thread.
        void enqueue_completion(const io_completion& c);

        // Drain completions into output span.
        int drain(const span<io_completion>& completions);
    };

}  // namespace bee::async
