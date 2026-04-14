#pragma once

#if defined(_WIN32)
// clang-format off
#    include <WinSock2.h>
// clang-format on
#    include <WS2tcpip.h>
#endif

#include <bee/async/async_types.h>
#include <bee/net/fd.h>
#include <bee/net/socket.h>
#include <bee/sys/file_handle.h>
#include <bee/utility/span.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>

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
        bool submit_poll(net::fd_t fd, uint64_t request_id);
        int poll(const span<io_completion>& completions);
        int wait(const span<io_completion>& completions, int timeout);
        void stop();
        bool associate(net::fd_t fd);
        std::pair<file_handle, bool> associate_file(file_handle::value_type fd);
        void cancel(net::fd_t fd);

        struct overlapped_ext {
            // OVERLAPPED must be first so we can cast between the two.
            unsigned char overlapped[32];  // sizeof(OVERLAPPED) == 32 on x64 (static_assert below)
            uint64_t request_id;
            uintptr_t accept_sock;  // used only for op_accept
            enum op_type : uint8_t {
                op_read,
                op_readv,
                op_write,
                op_writev,
                op_accept,
                op_connect,
                op_file_read,
                op_file_write,
                op_poll,
            } type;
            // Output buffer for AcceptEx: (sizeof(sockaddr_storage)+16) * 2 bytes
            // sizeof(sockaddr_storage)==128, so 144*2 = 288 bytes
            static constexpr DWORD kAddrSlotSize = sizeof(sockaddr_storage) + 16;
            char addr_buf[kAddrSlotSize * 2];
        };

    private:
        void* m_iocp;       // HANDLE
        void* m_connectex;  // LPFN_CONNECTEX
        void* m_acceptex;   // LPFN_ACCEPTEX
        std::deque<io_completion> m_sync_completions;
    };

}  // namespace bee::async
