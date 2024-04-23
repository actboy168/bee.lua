#include <bee/net/bpoll.h>
#include <bee/nonstd/to_underlying.h>
#include <bee/utility/bitmask.h>
#include <errno.h>
#include <poll.h>
#include <sys/event.h>
#include <unistd.h>

#include <cassert>
#include <limits>

namespace bee::net {
    constexpr int VAL_BITS                     = 2;
    constexpr int KEY_BITS                     = 32 - VAL_BITS;
    constexpr uint16_t KQUEUE_STATE_REGISTERED = 0x0001;
    constexpr uint16_t KQUEUE_STATE_EPOLLRDHUP = 0x0002;

    template <typename T>
    static bool invalid_fd(T fd) noexcept {
        static_assert(std::is_integral_v<T>);
        if constexpr (std::numeric_limits<T>::is_signed) {
            if (fd < 0) {
                return true;
            }
            return invalid_fd(static_cast<std::make_unsigned_t<T>>(fd));
        }
        else if constexpr (sizeof(uint32_t) < sizeof(T)) {
            if (fd > (std::numeric_limits<uint32_t>::max)()) {
                return true;
            }
            return invalid_fd(static_cast<uint32_t>(fd));
        }
        else {
            return (uint32_t)fd & ~(((uint32_t)1 << KEY_BITS) - 1);
        }
    }

    static int set_state(int kq, uint32_t key, uint16_t val) noexcept {
        assert(!invalid_fd(key));
        if ((val & ~(((uint16_t)1 << VAL_BITS) - 1))) {
            return EINVAL;
        }
        struct kevent ev[VAL_BITS * 2];
        int n = 0;
        for (int i = 0; i < VAL_BITS; ++i) {
            uint32_t info_bit = (uint32_t)1 << i;
            uint32_t kev_key  = key | (info_bit << KEY_BITS);
            EV_SET(&ev[n], kev_key, EVFILT_USER, EV_ADD, 0, 0, 0);
            ++n;
            if (!(val & info_bit)) {
                EV_SET(&ev[n], kev_key, EVFILT_USER, EV_DELETE, 0, 0, 0);
                ++n;
            }
        }
        int oe = errno;
        if (::kevent(kq, ev, n, NULL, 0, NULL) < 0) {
            int e = errno;
            errno = oe;
            return e;
        }
        return 0;
    }

    static bool get_state(int kq, uint32_t key, uint16_t* val) noexcept {
        assert(!invalid_fd(key));
        struct kevent ev[VAL_BITS];
        for (int i = 0; i < VAL_BITS; ++i) {
            uint32_t info_bit = (uint32_t)1 << i;
            uint32_t kev_key  = key | (info_bit << KEY_BITS);
            EV_SET(&ev[i], kev_key, EVFILT_USER, EV_RECEIPT, 0, 0, 0);
        }
        int n = ::kevent(kq, ev, VAL_BITS, ev, VAL_BITS, NULL);
        if (n < 0) {
            return false;
        }
        uint16_t nval = 0;
        for (int i = 0; i < n; ++i) {
            if (!(ev[i].flags & EV_ERROR)) {
                errno = EINVAL;
                return false;
            }
            if (ev[i].data == 0) {
                nval |= (uint32_t)1 << i;
            }
            else if (ev[i].data != ENOENT) {
                errno = EINVAL;
                return false;
            }
        }
        *val = nval;
        return true;
    }

    static bool set_kevent(int kq, int fd, int read_flags, int write_flags, int kqflags, void* udata) noexcept {
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
        set_state(kq, fd, kqflags);
        return true;
    }

    constexpr bpoll_event AllowBpollEvents = bpoll_event::in | bpoll_event::out | bpoll_event::hup | bpoll_event::rdhup | bpoll_event::err;

    static bool bpoll_ctl(int kq, int fd, const bpoll_event_t& ev, bool add) noexcept {
        if (bitmask_has(ev.events, ~AllowBpollEvents)) {
            errno = EINVAL;
            return false;
        }
        if (invalid_fd(fd)) {
            errno = EBADF;
            return false;
        }
        uint16_t kqflags;
        if (!get_state(kq, fd, &kqflags)) {
            return false;
        }
        int flags = 0;
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
        return set_kevent(kq, fd, read_flags, write_flags, kqflags, ev.data.ptr);
    }

    bpoll_handle bpoll_create() noexcept {
        return kqueue();
    }

    bool bpoll_close(bpoll_handle kq) noexcept {
        return ::close(kq) == 0;
    }

    bool bpoll_ctl_add(bpoll_handle kq, fd_t fd, const bpoll_event_t& event) noexcept {
        return bpoll_ctl(kq, fd, event, true);
    }

    bool bpoll_ctl_mod(bpoll_handle kq, fd_t fd, const bpoll_event_t& event) noexcept {
        return bpoll_ctl(kq, fd, event, false);
    }

    bool bpoll_ctl_del(bpoll_handle kq, fd_t fd) noexcept {
        if (invalid_fd(fd)) {
            errno = EBADF;
            return false;
        }
        return set_kevent(kq, fd, EV_DELETE, EV_DELETE, 0, NULL);
    }

    int bpoll_wait(bpoll_handle kq, const span<bpoll_event_t>& events, int timeout) noexcept {
        struct kevent kev[events.size()];
        struct timespec t, *timeop = &t;
        if (timeout < 0) {
            timeop = NULL;
        }
        else {
            t.tv_sec  = timeout / 1000l;
            t.tv_nsec = timeout % 1000l * 1000000l;
        }
        int n = ::kevent(kq, NULL, 0, kev, events.size(), timeop);
        if (n == -1) {
            return -1;
        }
        size_t event_n = 0;
        for (int i = 0; i < n; ++i) {
            bpoll_event e = bpoll_event::null;
            if (kev[i].filter == EVFILT_READ) {
                e |= bpoll_event::in;
            }
            else if (kev[i].filter == EVFILT_WRITE) {
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
                    uint16_t kqflags = 0;
                    if (!invalid_fd(kev[i].ident)) {
                        get_state(kq, kev[i].ident, &kqflags);
                    }
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
}
