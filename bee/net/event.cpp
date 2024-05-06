#include <bee/net/event.h>
#include <bee/net/socket.h>
#include <bee/nonstd/unreachable.h>

namespace bee::net {
    event::~event() noexcept {
        if (pipe[0] != retired_fd) {
            socket::close(pipe[0]);
            pipe[0] = retired_fd;
        }
        if (pipe[1] != retired_fd) {
            socket::close(pipe[1]);
            pipe[1] = retired_fd;
        }
    }

    bool event::open() noexcept {
        if (pipe[0] != retired_fd)
            return false;
        return socket::pair(pipe, socket::fd_flags::nonblock);
    }

    void event::set() noexcept {
        if (pipe[0] == retired_fd)
            return;
        if (e.test_and_set(std::memory_order_seq_cst))
            return;
        char tmp[1] = { 0 };
        int rc      = 0;
        socket::send(pipe[1], rc, tmp, sizeof(tmp));
    }

    void event::clear() noexcept {
        char tmp[128];
        int rc = 0;
        for (;;) {
            switch (socket::recv(pipe[0], rc, tmp, sizeof(tmp))) {
            case socket::recv_status::wait:
            case socket::recv_status::close:
            case socket::recv_status::failed:
                e.clear(std::memory_order_seq_cst);
                return;
            case socket::recv_status::success:
                break;
            default:
                std::unreachable();
            }
        }
    }

    fd_t event::fd() const noexcept {
        return pipe[0];
    }
}
