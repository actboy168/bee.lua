#pragma once

#include <bee/win/afd/afd.h>
#include <bee/net/bpoll.h>

namespace bee::net::afd {
    class poller_fd {
    public:
        enum class status : uint8_t {
            idle,
            pending,
            cancelled,
        };
        enum class destroy_status : uint8_t {
            nothing,
            unregister,
            destroy,
            unregister_then_destroy,
        };
        enum class update_status : uint8_t {
            succee,
            close,
            failed,
        };
        poller_fd(bpoll_socket socket, bpoll_socket base_socket) noexcept;
        [[nodiscard]] destroy_status destroy(const afd_context& ctx) noexcept;
        [[nodiscard]] update_status update(const afd_context& ctx) noexcept;
        [[nodiscard]] bool set_event(const bpoll_event_t& ev) noexcept;
        [[nodiscard]] afd_poll_event feed_event(bpoll_event_t& ev) noexcept;
        [[nodiscard]] bpoll_socket get_socket() noexcept;

    private:
        afd_poll_context afd;
        bpoll_socket socket;
        bpoll_socket base_socket;
        bpoll_data_t user_data;
        bpoll_event user_events    = bpoll_event::null;
        bpoll_event pending_events = bpoll_event::null;
        status poll_status         = status::idle;
        bool delete_pending        = false;
    };
}
