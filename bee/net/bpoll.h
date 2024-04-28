#pragma once

#include <bee/net/fd.h>
#include <bee/utility/bitmask.h>
#include <bee/utility/span.h>

#include <cstdint>

#if defined(__linux__)
#    include <sys/epoll.h>
#endif

namespace bee::net {
    enum class bpoll_event : uint32_t {
        null    = 0,
        in      = (1U << 0),   // EPOLLIN
        pri     = (1U << 1),   // EPOLLPRI
        out     = (1U << 2),   // EPOLLOUT
        err     = (1U << 3),   // EPOLLERR
        hup     = (1U << 4),   // EPOLLHUP
        rdnorm  = (1U << 6),   // EPOLLRDNORM
        rdband  = (1U << 7),   // EPOLLRDBAND
        wrnorm  = (1U << 8),   // EPOLLWRNORM
        wrand   = (1U << 9),   // EPOLLWRBAND
        msg     = (1U << 10),  // EPOLLMSG
        rdhup   = (1U << 13),  // EPOLLRDHUP
        oneshot = (1U << 30),  // EPOLLONESHOT
#if !defined(_WIN32)
        et = (1U << 31),  // EPOLLET
#endif
    };
    BEE_BITMASK_OPERATORS(bpoll_event)

#if defined(__linux__)
    using bpoll_data_t  = epoll_data_t;
    using bpoll_event_t = epoll_event;
    using bpoll_handle  = fd_t;
#else
    union bpoll_data_t {
        void* ptr;
        int fd;
        uint32_t u32;
        uint64_t u64;
    };
    struct bpoll_event_t {
        bpoll_event events;
        bpoll_data_t data;
    };
    using bpoll_handle = uintptr_t;
#endif
    constexpr bpoll_handle invalid_bpoll_handle = (bpoll_handle)-1;

    bpoll_handle bpoll_create() noexcept;
    bool bpoll_close(bpoll_handle handle) noexcept;
    bool bpoll_ctl_add(bpoll_handle handle, fd_t socket, const bpoll_event_t& event) noexcept;
    bool bpoll_ctl_mod(bpoll_handle handle, fd_t socket, const bpoll_event_t& event) noexcept;
    bool bpoll_ctl_del(bpoll_handle handle, fd_t socket) noexcept;
    int bpoll_wait(bpoll_handle handle, const span<bpoll_event_t>& events, int timeout) noexcept;
}
