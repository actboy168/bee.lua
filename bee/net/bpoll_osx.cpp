#include <bee/net/bpoll.h>
#include <bee/nonstd/to_underlying.h>
#include <bee/utility/bitmask.h>
#include <bee/utility/flatmap.h>
#include <bee/utility/hybrid_array.h>
#include <errno.h>
#include <sys/event.h>
#include <unistd.h>

#include <cassert>
#include <limits>

namespace bee::net {
    constexpr uint8_t KQUEUE_STATE_REGISTERED = 0x01;
    constexpr uint8_t KQUEUE_STATE_EPOLLRDHUP = 0x02;
    constexpr bpoll_event AllowBpollEvents    = bpoll_event::in | bpoll_event::out | bpoll_event::hup | bpoll_event::rdhup | bpoll_event::err;

    struct poller {
        poller(int kq)
            : kq(kq) {
        }
        ~poller() {
            ::close(kq);
        }

        void set_state(fd_t fd, uint8_t val) noexcept {
            if (val == 0) {
                state.erase(fd);
            } else {
                state.insert_or_assign(fd, val);
            }
        }

        uint8_t get_state(fd_t fd) const noexcept {
            auto ptr = state.find(fd);
            return ptr ? *ptr : 0;
        }

        bool set_kevent(int fd, int read_flags, int write_flags, uint8_t kqflags, void* udata) noexcept {
            struct kevent ev[2];
            EV_SET(&ev[0], fd, EVFILT_READ, read_flags | EV_RECEIPT, 0, 0, udata);
            EV_SET(&ev[1], fd, EVFILT_WRITE, write_flags | EV_RECEIPT, 0, 0, udata);
            int r = ::kevent(kq, ev, 2, ev, 2, NULL);
            if (r < 0) {
                return false;
            }
            for (int i = 0; i < r; ++i) {
                assert((ev[i].flags & EV_ERROR) != 0);
                if (ev[i].data != 0) {
                    errno = ev[i].data;
                    return false;
                }
            }
            set_state(fd, kqflags);
            return true;
        }

        bool bpoll_ctl(fd_t fd, const bpoll_event_t& ev, bool add) noexcept {
            if (bitmask_has(ev.events, ~AllowBpollEvents)) {
                errno = EINVAL;
                return false;
            }
            if (fd < 0) {
                errno = EBADF;
                return false;
            }
            uint8_t kqflags = get_state(fd);
            int flags       = 0;
            if (add) {
                if (kqflags & KQUEUE_STATE_REGISTERED) {
                    errno = EEXIST;
                    return false;
                }
                kqflags = KQUEUE_STATE_REGISTERED;
                flags |= EV_ADD;
            }
            if (bitmask_has(ev.events, bpoll_event::et)) {
                flags |= EV_CLEAR;
            }
            if (bitmask_has(ev.events, bpoll_event::oneshot)) {
                flags |= EV_ONESHOT;
            }
            if (bitmask_has(ev.events, bpoll_event::rdhup)) {
                kqflags |= KQUEUE_STATE_EPOLLRDHUP;
            }
            int read_flags  = flags | EV_DISABLE;
            int write_flags = flags | EV_DISABLE;
            if (bitmask_has(ev.events, bpoll_event::in)) {
                read_flags &= ~EV_DISABLE;
                read_flags |= EV_ENABLE;
            }
            if (bitmask_has(ev.events, bpoll_event::out)) {
                write_flags &= ~EV_DISABLE;
                write_flags |= EV_ENABLE;
            }
            return set_kevent(fd, read_flags, write_flags, kqflags, ev.data.ptr);
        }

        int bpoll_wait(const span<bpoll_event_t>& events, int timeout) noexcept {
            hybrid_array<struct kevent, 256> kev(events.size());
            struct timespec t, *timeop = &t;
            if (timeout < 0) {
                timeop = NULL;
            } else {
                t.tv_sec  = timeout / 1000l;
                t.tv_nsec = timeout % 1000l * 1000000l;
            }
            int n = ::kevent(kq, NULL, 0, kev.data(), kev.size(), timeop);
            if (n == -1) {
                return -1;
            }
            size_t event_n = 0;
            for (int i = 0; i < n; ++i) {
                bpoll_event e = bpoll_event::null;
                if (kev[i].filter == EVFILT_READ) {
                    e |= bpoll_event::in;
                } else if (kev[i].filter == EVFILT_WRITE) {
                    e |= bpoll_event::out;
                }
                if (kev[i].flags & EV_ERROR) {
                    e |= bpoll_event::err;
                }
                if (kev[i].flags & EV_EOF) {
                    if (kev[i].fflags) {
                        e |= bpoll_event::err;
                    }
                    if (kev[i].filter == EVFILT_READ) {
                        uint16_t kqflags = get_state(kev[i].ident);
                        if (kqflags & KQUEUE_STATE_EPOLLRDHUP) {
                            e |= bpoll_event::rdhup;
                        }
                    }
                }
                auto& ev    = events[event_n++];
                ev.events   = e;
                ev.data.ptr = kev[i].udata;
            }
            return n;
        }

        int kq;
        flatmap<fd_t, uint8_t> state;
    };

    bpoll_handle bpoll_create() noexcept {
        int kq = ::kqueue();
        if (kq == -1) {
            return invalid_bpoll_handle;
        }
        poller* ep = new (std::nothrow) poller(kq);
        if (ep == NULL) {
            ::close(kq);
            errno = ENOMEM;
            return invalid_bpoll_handle;
        }
        return (bpoll_handle)ep;
    }

    bool bpoll_close(bpoll_handle handle) noexcept {
        if (handle == invalid_bpoll_handle) {
            errno = EBADF;
            return false;
        }
        auto ep = (poller*)handle;
        delete ep;
        return true;
    }

    bool bpoll_ctl_add(bpoll_handle handle, fd_t fd, const bpoll_event_t& event) noexcept {
        if (handle == invalid_bpoll_handle) {
            errno = EBADF;
            return false;
        }
        auto ep = (poller*)handle;
        return ep->bpoll_ctl(fd, event, true);
    }

    bool bpoll_ctl_mod(bpoll_handle handle, fd_t fd, const bpoll_event_t& event) noexcept {
        if (handle == invalid_bpoll_handle) {
            errno = EBADF;
            return false;
        }
        auto ep = (poller*)handle;
        return ep->bpoll_ctl(fd, event, false);
    }

    bool bpoll_ctl_del(bpoll_handle handle, fd_t fd) noexcept {
        if (handle == invalid_bpoll_handle) {
            errno = EBADF;
            return false;
        }
        auto ep = (poller*)handle;
        if (fd < 0) {
            errno = EBADF;
            return false;
        }
        return ep->set_kevent(fd, EV_DELETE, EV_DELETE, 0, NULL);
    }

    int bpoll_wait(bpoll_handle handle, const span<bpoll_event_t>& events, int timeout) noexcept {
        if (handle == invalid_bpoll_handle) {
            errno = EBADF;
            return -1;
        }
        auto ep = (poller*)handle;
        return ep->bpoll_wait(events, timeout);
    }
}
