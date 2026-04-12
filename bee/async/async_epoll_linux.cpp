#include <bee/async/async.h>
#include <bee/async/async_epoll_linux.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace bee::async {

    async_epoll::async_epoll()
        : m_epfd(-1) {
        m_epfd = epoll_create1(EPOLL_CLOEXEC);
    }

    async_epoll::~async_epoll() {
        stop();
    }

    // Build combined events mask from fd_state and call epoll_ctl ADD or MOD.
    bool async_epoll::fd_arm(net::fd_t fd, fd_state& state) {
        uint32_t events = 0;
        if (state.read_op) events |= EPOLLIN;
        if (state.write_op) events |= EPOLLOUT;

        if (events == state.events) {
            return true;
        }

        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events  = events;
        ev.data.fd = fd;

        int op = (state.events == 0) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
        if (epoll_ctl(m_epfd, op, fd, &ev) != 0) {
            return false;
        }
        state.events = events;
        return true;
    }

    // Remove one direction from fd_state; update or DEL epoll registration.
    void async_epoll::fd_disarm(net::fd_t fd, fd_state& state, bool is_write) {
        if (is_write) {
            state.write_op = nullptr;
        } else {
            state.read_op = nullptr;
        }

        uint32_t new_events = 0;
        if (state.read_op) new_events |= EPOLLIN;
        if (state.write_op) new_events |= EPOLLOUT;

        if (new_events == 0) {
            epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
            state.events = 0;
            m_fd_states.erase(fd);
        } else if (new_events != state.events) {
            struct epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.events  = new_events;
            ev.data.fd = fd;
            epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &ev);
            state.events = new_events;
        }
    }

    bool async_epoll::submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id) {
        auto* op       = new pending_op();
        op->request_id = request_id;
        op->fd         = fd;
        op->type       = pending_op::read;
        op->r.buffer   = buffer;
        op->r.len      = len;

        auto& state   = m_fd_states[fd];
        state.read_op = op;
        if (!fd_arm(fd, state)) {
            state.read_op = nullptr;
            if (!state.write_op) m_fd_states.erase(fd);
            delete op;
            return false;
        }
        return true;
    }

    bool async_epoll::submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id) {
        auto* op       = new pending_op();
        op->request_id = request_id;
        op->fd         = fd;
        op->type       = pending_op::write;
        op->w.buffer   = buffer;
        op->w.len      = len;

        auto& state    = m_fd_states[fd];
        state.write_op = op;
        if (!fd_arm(fd, state)) {
            state.write_op = nullptr;
            if (!state.read_op) m_fd_states.erase(fd);
            delete op;
            return false;
        }
        return true;
    }

    bool async_epoll::submit_writev(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) {
        auto* op       = new pending_op();
        op->request_id = request_id;
        op->fd         = fd;
        op->type       = pending_op::writev;
        op->wv         = dynarray<net::socket::iobuf>(bufs.size());
        for (size_t i = 0; i < bufs.size(); ++i) op->wv[i] = bufs[i];

        auto& state    = m_fd_states[fd];
        state.write_op = op;
        if (!fd_arm(fd, state)) {
            state.write_op = nullptr;
            if (!state.read_op) m_fd_states.erase(fd);
            delete op;
            return false;
        }
        return true;
    }

    bool async_epoll::submit_accept(net::fd_t listen_fd, uint64_t request_id) {
        auto* op       = new pending_op();
        op->request_id = request_id;
        op->fd         = listen_fd;
        op->type       = pending_op::accept;

        auto& state   = m_fd_states[listen_fd];
        state.read_op = op;
        if (!fd_arm(listen_fd, state)) {
            state.read_op = nullptr;
            if (!state.write_op) m_fd_states.erase(listen_fd);
            delete op;
            return false;
        }
        return true;
    }

    bool async_epoll::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
        auto status = net::socket::connect(fd, ep);
        if (status == net::socket::status::success) {
            io_completion c;
            c.request_id        = request_id;
            c.op                = async_op::connect;
            c.status            = async_status::success;
            c.bytes_transferred = 0;
            c.error_code        = 0;
            m_sync_completions.push_back(c);
            return true;
        }
        if (status == net::socket::status::wait) {
            auto* op       = new pending_op();
            op->request_id = request_id;
            op->fd         = fd;
            op->type       = pending_op::connect;

            auto& state    = m_fd_states[fd];
            state.write_op = op;
            if (!fd_arm(fd, state)) {
                state.write_op = nullptr;
                if (!state.read_op) m_fd_states.erase(fd);
                delete op;
                return false;
            }
            return true;
        }
        return false;
    }

    bool async_epoll::submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) {
        io_completion c;
        c.request_id = request_id;
        c.op         = async_op::file_read;
        ssize_t n    = pread(fd, buffer, len, offset);
        if (n >= 0) {
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(n);
            c.error_code        = 0;
        } else {
            c.status            = async_status::error;
            c.bytes_transferred = 0;
            c.error_code        = errno;
        }
        m_sync_completions.push_back(c);
        return true;
    }

    bool async_epoll::submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) {
        io_completion c;
        c.request_id = request_id;
        c.op         = async_op::file_write;
        ssize_t n    = pwrite(fd, buffer, len, offset);
        if (n >= 0) {
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(n);
            c.error_code        = 0;
        } else {
            c.status            = async_status::error;
            c.bytes_transferred = 0;
            c.error_code        = errno;
        }
        m_sync_completions.push_back(c);
        return true;
    }

    bool async_epoll::submit_poll(net::fd_t fd, uint64_t request_id) {
        auto* op       = new pending_op();
        op->request_id = request_id;
        op->fd         = fd;
        op->type       = pending_op::fd_poll;

        auto& state   = m_fd_states[fd];
        state.read_op = op;
        if (!fd_arm(fd, state)) {
            state.read_op = nullptr;
            if (!state.write_op) m_fd_states.erase(fd);
            delete op;
            return false;
        }
        return true;
    }

    // Process one read-direction event. Returns true if a completion was produced.
    // If the syscall returns EAGAIN (spurious wakeup), returns false and leaves op intact.
    static bool process_read_op(
        int epfd,
        net::fd_t fd,
        async_epoll::fd_state& state,
        std::unordered_map<net::fd_t, async_epoll::fd_state>& fd_states,
        io_completion& out
    ) {
        async_epoll::pending_op* op = state.read_op;
        out.request_id              = op->request_id;
        out.bytes_transferred       = 0;
        out.error_code              = 0;

        bool produced = true;

        switch (op->type) {
        case async_epoll::pending_op::read: {
            out.op  = async_op::read;
            int rc  = 0;
            auto rs = net::socket::recv(fd, rc, static_cast<char*>(op->r.buffer), static_cast<int>(op->r.len));
            switch (rs) {
            case net::socket::recv_status::success:
                out.status            = async_status::success;
                out.bytes_transferred = static_cast<size_t>(rc);
                break;
            case net::socket::recv_status::close:
                out.status = async_status::close;
                break;
            case net::socket::recv_status::failed:
                out.status     = async_status::error;
                out.error_code = errno;
                break;
            case net::socket::recv_status::wait:
                // Spurious EPOLLIN: no data yet, keep op, do not emit completion.
                produced = false;
                break;
            }
            break;
        }
        case async_epoll::pending_op::accept: {
            out.op          = async_op::accept;
            net::fd_t newfd = net::retired_fd;
            auto as         = net::socket::accept(fd, newfd);
            switch (as) {
            case net::socket::status::success:
                out.status            = async_status::success;
                out.bytes_transferred = static_cast<size_t>(newfd);
                break;
            case net::socket::status::wait:
                produced = false;
                break;
            case net::socket::status::failed:
                out.status     = async_status::error;
                out.error_code = errno;
                break;
            }
            break;
        }
        case async_epoll::pending_op::fd_poll: {
            // fd_poll: 只通知 fd 可读，不消费任何数据
            out.op                = async_op::fd_poll;
            out.status            = async_status::success;
            out.bytes_transferred = 0;
            out.error_code        = 0;
            break;
        }
        default:
            produced = false;
            break;
        }

        if (produced) {
            // Completion ready: clear read slot and update epoll mask.
            delete op;
            state.read_op       = nullptr;
            uint32_t new_events = state.write_op ? EPOLLOUT : 0;
            if (new_events == 0) {
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                state.events = 0;
                fd_states.erase(fd);
            } else if (new_events != state.events) {
                struct epoll_event ev;
                memset(&ev, 0, sizeof(ev));
                ev.events  = new_events;
                ev.data.fd = fd;
                epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                state.events = new_events;
            }
        }
        return produced;
    }

    // Process one write-direction event. Returns true if a completion was produced.
    static bool process_write_op(
        int epfd,
        net::fd_t fd,
        async_epoll::fd_state& state,
        std::unordered_map<net::fd_t, async_epoll::fd_state>& fd_states,
        io_completion& out
    ) {
        async_epoll::pending_op* op = state.write_op;
        out.request_id              = op->request_id;
        out.bytes_transferred       = 0;
        out.error_code              = 0;

        bool produced = true;

        switch (op->type) {
        case async_epoll::pending_op::write: {
            out.op  = async_op::write;
            int rc  = 0;
            auto ss = net::socket::send(fd, rc, static_cast<const char*>(op->w.buffer), static_cast<int>(op->w.len));
            switch (ss) {
            case net::socket::status::success:
                out.status            = async_status::success;
                out.bytes_transferred = static_cast<size_t>(rc);
                break;
            case net::socket::status::wait:
                produced = false;
                break;
            case net::socket::status::failed:
                out.status     = async_status::error;
                out.error_code = errno;
                break;
            }
            break;
        }
        case async_epoll::pending_op::writev: {
            out.op  = async_op::writev;
            int rc  = 0;
            auto ss = net::socket::sendv(fd, rc, span<const net::socket::iobuf>(op->wv.data(), op->wv.size()));
            switch (ss) {
            case net::socket::status::success:
                out.status            = async_status::success;
                out.bytes_transferred = static_cast<size_t>(rc);
                break;
            case net::socket::status::wait:
                produced = false;
                break;
            case net::socket::status::failed:
                out.status     = async_status::error;
                out.error_code = errno;
                break;
            }
            break;
        }
        case async_epoll::pending_op::connect: {
            out.op  = async_op::connect;
            int err = 0;
            if (net::socket::errcode(fd, err) && err == 0) {
                out.status = async_status::success;
            } else {
                out.status     = async_status::error;
                out.error_code = (err != 0) ? err : errno;
            }
            break;
        }
        default:
            produced = false;
            break;
        }

        if (produced) {
            delete op;
            state.write_op      = nullptr;
            uint32_t new_events = state.read_op ? EPOLLIN : 0;
            if (new_events == 0) {
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                state.events = 0;
                fd_states.erase(fd);
            } else if (new_events != state.events) {
                struct epoll_event ev;
                memset(&ev, 0, sizeof(ev));
                ev.events  = new_events;
                ev.data.fd = fd;
                epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                state.events = new_events;
            }
        }
        return produced;
    }

    static int drain_epoll(
        int epfd,
        const span<io_completion>& completions,
        int timeout_ms,
        std::unordered_map<net::fd_t, async_epoll::fd_state>& fd_states
    ) {
        constexpr int kMaxEvents = 64;
        struct epoll_event events[kMaxEvents];
        int nfds = epoll_wait(epfd, events, kMaxEvents, timeout_ms);
        if (nfds <= 0) {
            return 0;
        }

        int count = 0;
        for (int i = 0; i < nfds && count < static_cast<int>(completions.size()); ++i) {
            uint32_t ev  = events[i].events;
            net::fd_t fd = static_cast<net::fd_t>(events[i].data.fd);

            auto it = fd_states.find(fd);
            if (it == fd_states.end()) continue;
            auto& state = it->second;

            // Propagate errors to both directions.
            if (ev & (EPOLLERR | EPOLLHUP)) {
                ev |= EPOLLIN | EPOLLOUT;
            }

            // Process read direction.
            if ((ev & EPOLLIN) && state.read_op) {
                io_completion c;
                if (process_read_op(epfd, fd, state, fd_states, c)) {
                    completions[count++] = c;
                }
            }

            // Process write direction (re-lookup in case erase happened above).
            if (count < static_cast<int>(completions.size()) && (ev & EPOLLOUT)) {
                auto it2 = fd_states.find(fd);
                if (it2 != fd_states.end() && it2->second.write_op) {
                    io_completion c;
                    if (process_write_op(epfd, fd, it2->second, fd_states, c)) {
                        completions[count++] = c;
                    }
                }
            }
        }
        return count;
    }

    int async_epoll::poll(const span<io_completion>& completions) {
        int count = 0;

        while (!m_sync_completions.empty() && count < static_cast<int>(completions.size())) {
            completions[count++] = m_sync_completions.front();
            m_sync_completions.pop_front();
        }
        if (count >= static_cast<int>(completions.size())) {
            return count;
        }

        count += drain_epoll(m_epfd, span<io_completion>(completions.data() + count, completions.size() - count), 0, m_fd_states);
        return count;
    }

    int async_epoll::wait(const span<io_completion>& completions, int timeout) {
        int count = 0;

        while (!m_sync_completions.empty() && count < static_cast<int>(completions.size())) {
            completions[count++] = m_sync_completions.front();
            m_sync_completions.pop_front();
        }
        if (count > 0 || completions.size() == 0) {
            return count;
        }

        count += drain_epoll(m_epfd, completions, timeout, m_fd_states);
        return count;
    }

    void async_epoll::stop() {
        if (m_epfd >= 0) {
            close(m_epfd);
            m_epfd = -1;
        }
        for (auto& [fd, state] : m_fd_states) {
            delete state.read_op;
            delete state.write_op;
        }
        m_fd_states.clear();
    }

    void async_epoll::cancel(net::fd_t fd) {
        auto it = m_fd_states.find(fd);
        if (it == m_fd_states.end()) return;
        auto& state = it->second;
        epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
        delete state.read_op;
        delete state.write_op;
        m_fd_states.erase(it);
    }

}  // namespace bee::async
