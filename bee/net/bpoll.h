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
#else
    union bpoll_data_t {
        void* ptr;
        int fd;
        uint32_t u32;
        uint64_t u64;
        fd_t sock;
        fd_t hnd;
    };
    struct bpoll_event_t {
        bpoll_event events;
        bpoll_data_t data;
    };
#endif

    fd_t bpoll_create();
    bool bpoll_close(fd_t fd);
    bool bpoll_ctl_add(fd_t fd, fd_t socket, const bpoll_event_t& event);
    bool bpoll_ctl_mod(fd_t fd, fd_t socket, const bpoll_event_t& event);
    bool bpoll_ctl_del(fd_t fd, fd_t socket);
    int bpoll_wait(fd_t fd, const span<bpoll_event_t>& events, int timeout);
}
