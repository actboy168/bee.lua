#include <bee/net/bpoll.h>
#include <bee/nonstd/to_underlying.h>
#include <sys/epoll.h>
#include <unistd.h>

namespace bee::net {
    static_assert(sizeof(bpoll_event) == sizeof(uint32_t));
    static_assert(sizeof(bpoll_event_t) == sizeof(epoll_event));
    static_assert(sizeof(bpoll_data_t) == sizeof(epoll_data_t));

    static_assert(std::to_underlying(bpoll_event::null) == 0);
    static_assert(std::to_underlying(bpoll_event::in) == EPOLLIN);
    static_assert(std::to_underlying(bpoll_event::pri) == EPOLLPRI);
    static_assert(std::to_underlying(bpoll_event::out) == EPOLLOUT);
    static_assert(std::to_underlying(bpoll_event::err) == EPOLLERR);
    static_assert(std::to_underlying(bpoll_event::hup) == EPOLLHUP);
    static_assert(std::to_underlying(bpoll_event::rdnorm) == EPOLLRDNORM);
    static_assert(std::to_underlying(bpoll_event::rdband) == EPOLLRDBAND);
    static_assert(std::to_underlying(bpoll_event::wrnorm) == EPOLLWRNORM);
    static_assert(std::to_underlying(bpoll_event::wrand) == EPOLLWRBAND);
    static_assert(std::to_underlying(bpoll_event::msg) == EPOLLMSG);
    static_assert(std::to_underlying(bpoll_event::rdhup) == EPOLLRDHUP);
    static_assert(std::to_underlying(bpoll_event::oneshot) == EPOLLONESHOT);

    bpoll_handle bpoll_create() {
        return (bpoll_handle)::epoll_create1(EPOLL_CLOEXEC);
    }

    bool bpoll_close(bpoll_handle hnd) {
        return ::close(hnd) == 0;
    }

    bool bpoll_ctl_add(bpoll_handle hnd, bpoll_socket socket, const bpoll_event_t& event) {
        return ::epoll_ctl(hnd, EPOLL_CTL_ADD, socket, (struct epoll_event*)&event) != -1;
    }

    bool bpoll_ctl_mod(bpoll_handle hnd, bpoll_socket socket, const bpoll_event_t& event) {
        return ::epoll_ctl(hnd, EPOLL_CTL_MOD, socket, (struct epoll_event*)&event) != -1;
    }

    bool bpoll_ctl_del(bpoll_handle hnd, bpoll_socket socket) {
        return ::epoll_ctl(hnd, EPOLL_CTL_DEL, socket, NULL) != -1;
    }

    int bpoll_wait(bpoll_handle hnd, const span<bpoll_event_t>& events, int timeout) {
        return ::epoll_wait(hnd, (struct epoll_event*)events.data(), (int)events.size(), timeout);
    }
}
