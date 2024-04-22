#pragma once

#include <bee/net/bpoll.h>
#include <bee/utility/flatmap.h>
#include <bee/utility/span.h>
#include <bee/win/afd/afd.h>

namespace bee::net::afd {
    class poller_fd;
    class poller {
    public:
        poller(afd_context&& afd) noexcept;
        ~poller() noexcept;
        int wait(const span<bpoll_event_t>& events, int timeout) noexcept;
        bool ctl_add(fd_t socket, const bpoll_event_t& ev) noexcept;
        bool ctl_mod(fd_t socket, const bpoll_event_t& ev) noexcept;
        bool ctl_del(fd_t socket) noexcept;

    private:
        void destory_fd(poller_fd* fd) noexcept;
        void request_update(poller_fd* fd) noexcept;

    private:
        afd_context afd;
        flatmap<fd_t, poller_fd*> fds;
        flatset<poller_fd*> update_queue;
    };
}
