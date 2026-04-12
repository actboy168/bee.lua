#pragma once

#include <bee/async/async.h>
#include <bee/net/fd.h>
#include <bee/net/socket.h>
#include <bee/sys/file_handle.h>
#include <bee/utility/dynarray.h>
#include <bee/utility/span.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace bee::net {
    struct endpoint;
}

namespace bee::async {

    class async_epoll : public async {
    public:
        async_epoll();
        ~async_epoll() override;

        bool submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id) override;
        bool submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id) override;
        bool submit_writev(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) override;
        bool submit_accept(net::fd_t listen_fd, uint64_t request_id) override;
        bool submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) override;
        bool submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) override;
        bool submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) override;
        bool submit_poll(net::fd_t fd, uint64_t request_id) override;
        int poll(const span<io_completion>& completions) override;
        int wait(const span<io_completion>& completions, int timeout) override;
        void stop() override;
        void cancel(net::fd_t fd) override;

        struct pending_op {
            uint64_t request_id = 0;
            net::fd_t fd        = net::retired_fd;
            enum type_t : uint8_t {
                read,
                write,
                writev,
                accept,
                connect,
                fd_poll,
            } type = read;
            union {
                struct {
                    void* buffer;
                    size_t len;
                } r;
                struct {
                    const void* buffer;
                    size_t len;
                } w;
            };
            dynarray<net::socket::iobuf> wv;  // used when type == writev
        };

        // Per-fd state: tracks up to one read-direction and one write-direction pending op,
        // plus the currently registered epoll events mask.
        struct fd_state {
            pending_op* read_op  = nullptr;  // EPOLLIN direction (read/accept/connect)
            pending_op* write_op = nullptr;  // EPOLLOUT direction (write/writev)
            uint32_t events      = 0;        // currently registered events mask
        };

    private:
        int m_epfd;
        std::deque<io_completion> m_sync_completions;
        std::unordered_map<net::fd_t, fd_state> m_fd_states;

        static constexpr int kMaxEvents = 64;

        // Register or update epoll for fd, merging read_op/write_op into a combined event mask.
        // Returns false on epoll_ctl failure.
        bool fd_arm(net::fd_t fd, fd_state& state);

        // Remove one direction from fd_state; DEL if both directions gone.
        void fd_disarm(net::fd_t fd, fd_state& state, bool is_write);
    };

}  // namespace bee::async
