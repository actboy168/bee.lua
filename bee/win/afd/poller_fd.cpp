#include <Windows.h>
#include <bee/nonstd/to_underlying.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/bitmask.h>
#include <bee/win/afd/poller.h>
#include <bee/win/afd/poller_fd.h>

#include <cassert>

namespace bee::net::afd {
    constexpr bpoll_event AllowBpollEvents = bpoll_event::in | bpoll_event::pri | bpoll_event::out | bpoll_event::err | bpoll_event::hup | bpoll_event::rdnorm | bpoll_event::rdband | bpoll_event::wrnorm | bpoll_event::wrand | bpoll_event::msg | bpoll_event::rdhup;

    poller_fd::poller_fd(fd_t socket, fd_t base_socket) noexcept
        : socket(socket)
        , base_socket(base_socket) {
    }

    poller_fd::destroy_status poller_fd::destroy(const afd_context& ctx) noexcept {
        if (!delete_pending) {
            delete_pending = true;
            switch (poll_status) {
            case status::pending:
                if (afd_cancel_poll(ctx, afd)) {
                    poll_status    = status::cancelled;
                    pending_events = bpoll_event::null;
                }
                return destroy_status::unregister;
            case status::cancelled:
                return destroy_status::unregister;
            case status::idle:
                return destroy_status::unregister_then_destroy;
            default:
                std::unreachable();
            }
        } else {
            switch (poll_status) {
            case status::pending:
            case status::cancelled:
                return destroy_status::nothing;
            case status::idle:
                return destroy_status::destroy;
            default:
                std::unreachable();
            }
        }
    }

    poller_fd::update_status poller_fd::update(const afd_context& ctx) noexcept {
        assert(!delete_pending);
        switch (poll_status) {
        case status::pending:
            if ((user_events & AllowBpollEvents & ~pending_events) != bpoll_event::null) {
                if (afd_cancel_poll(ctx, afd)) {
                    poll_status    = status::cancelled;
                    pending_events = bpoll_event::null;
                    return update_status::succee;
                } else {
                    return update_status::failed;
                }
            }
            return update_status::succee;
        case status::cancelled:
            return update_status::succee;
        case status::idle:
            afd.afd_poll_info.Exclusive       = FALSE;
            afd.afd_poll_info.NumberOfHandles = 1;
            afd.afd_poll_info.Timeout         = (std::numeric_limits<int64_t>::max)();
            afd.afd_poll_info.Handle          = (HANDLE)base_socket;
            afd.afd_poll_info.Status          = 0;
            afd.afd_poll_info.Events          = std::to_underlying(user_events);
            if (!afd_poll(ctx, afd)) {
                switch (GetLastError()) {
                case ERROR_IO_PENDING:
                    break;
                case ERROR_INVALID_HANDLE:
                    return update_status::close;
                default:
                    return update_status::failed;
                }
            }
            poll_status    = status::pending;
            pending_events = user_events;
            return update_status::succee;
        default:
            std::unreachable();
        }
    }

    bool poller_fd::set_event(const bpoll_event_t& ev) noexcept {
        user_events = ev.events | bpoll_event::err | bpoll_event::hup;
        user_data   = ev.data;
        if ((user_events & AllowBpollEvents & ~pending_events) != bpoll_event::null) {
            if (!delete_pending) {
                return true;
            }
        }
        return false;
    }

    afd_poll_event poller_fd::feed_event(bpoll_event_t& ev) noexcept {
        poll_status    = status::idle;
        pending_events = bpoll_event::null;
        if (delete_pending) {
            return { afd_poll_status::closed };
        }
        auto event = afd_query_event(afd);
        switch (event.status) {
        case afd_poll_status::succeeded:
        case afd_poll_status::failed:
            event.bpoll_events &= user_events;
            if (event.bpoll_events != bpoll_event::null) {
                if (bitmask_has(user_events, bpoll_event::oneshot)) {
                    user_events = bpoll_event::null;
                }
                ev.data   = user_data;
                ev.events = event.bpoll_events;
            }
            break;
        case afd_poll_status::cancelled:
        case afd_poll_status::nothing:
        case afd_poll_status::closed:
            break;
        default:
            std::unreachable();
        }
        return event;
    }
    fd_t poller_fd::get_socket() noexcept {
        return socket;
    }
}
