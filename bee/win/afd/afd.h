#pragma once

#include <bee/net/bpoll.h>
#include <bee/utility/span.h>

struct _OVERLAPPED_ENTRY;

namespace bee::net::afd {
    struct afd_context {
        HANDLE iocp_handle;
        HANDLE afd_handle;
    };

    struct IO_STATUS_BLOCK {
        int32_t Status;
        uint64_t* Information;
    };

    struct AFD_POLL_INFO {
        int64_t Timeout;
        uint32_t NumberOfHandles;
        uint32_t Exclusive;
        HANDLE Handle;
        uint32_t Events;
        int32_t Status;
    };

    struct afd_poll_context {
        IO_STATUS_BLOCK io_status_block;
        AFD_POLL_INFO afd_poll_info;
    };

    enum class afd_poll_status : uint8_t {
        cancelled,
        failed,
        nothing,
        closed,
        succeeded,
    };

    struct afd_poll_event {
        afd_poll_status status;
        bpoll_event bpoll_events = bpoll_event::null;
    };

    bool afd_create(afd_context& ctx) noexcept;
    void afd_destroy(afd_context& ctx) noexcept;
    uint32_t afd_wait(const afd_context& ctx, const span<_OVERLAPPED_ENTRY>& events, uint32_t timeout) noexcept;
    bool afd_poll(const afd_context& ctx, afd_poll_context& poll) noexcept;
    bool afd_cancel_poll(const afd_context& ctx, afd_poll_context& poll) noexcept;
    afd_poll_event afd_query_event(const afd_poll_context& poll) noexcept;
    fd_t afd_get_base_socket(fd_t socket) noexcept;
}
