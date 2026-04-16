#pragma once

#include <bee/async/async.h>
#include <bee/net/fd.h>
#include <bee/sys/file_handle.h>
#include <bee/utility/span.h>

#include <cstddef>
#include <cstdint>

// io_uring is defined internally in the .cpp; forward-declare the ring type here.
struct io_uring;

namespace bee::async {

    class async_uring : public async {
    public:
        async_uring();
        ~async_uring() override;

        bool submit_read(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) override;
        bool submit_write(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) override;
        bool submit_accept(net::fd_t listen_fd, uint64_t request_id) override;
        bool submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) override;
        bool submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) override;
        bool submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) override;
        bool submit_poll(net::fd_t fd, uint64_t request_id) override;
        int poll(const span<io_completion>& completions) override;
        int wait(const span<io_completion>& completions, int timeout) override;
        void stop() override;
        void cancel(net::fd_t fd) override;

        bool valid() const noexcept { return m_ring != nullptr; }

    private:
        io_uring* m_ring;  // nullptr if ring initialisation failed

        int harvest_cqes(const span<io_completion>& completions) noexcept;
    };

}  // namespace bee::async
