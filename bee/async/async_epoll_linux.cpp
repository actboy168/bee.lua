#include <bee/async/async_epoll_linux.h>

#include <bee/async/async.h>
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

bool async_epoll::epoll_register(net::fd_t fd, uint32_t events, pending_op* op) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events   = events | EPOLLONESHOT;
    ev.data.ptr = op;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        if (errno == EEXIST) {
            if (epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &ev) != 0) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

bool async_epoll::submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id) {
    auto* op       = new pending_op();
    op->request_id = request_id;
    op->fd         = fd;
    op->type       = pending_op::read;
    op->r.buffer   = buffer;
    op->r.len      = len;
    if (!epoll_register(fd, EPOLLIN, op)) {
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
    if (!epoll_register(fd, EPOLLOUT, op)) {
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
    if (!epoll_register(listen_fd, EPOLLIN, op)) {
        delete op;
        return false;
    }
    return true;
}

bool async_epoll::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
    auto status = net::socket::connect(fd, ep);
    if (status == net::socket::status::success) {
        // Immediate success: enqueue a synthetic completion.
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
        if (!epoll_register(fd, EPOLLOUT, op)) {
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

static io_completion handle_event(struct epoll_event& ev) {
    auto* op = static_cast<async_epoll::pending_op*>(ev.data.ptr);
    io_completion c;
    c.request_id        = op->request_id;
    c.status            = async_status::error;
    c.bytes_transferred = 0;
    c.error_code        = 0;

    net::fd_t fd = op->fd;

    switch (op->type) {
    case async_epoll::pending_op::read: {
        c.op = async_op::read;
        int rc = 0;
        auto rs = net::socket::recv(fd, rc, static_cast<char*>(op->r.buffer), static_cast<int>(op->r.len));
        switch (rs) {
        case net::socket::recv_status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(rc);
            break;
        case net::socket::recv_status::close:
            c.status = async_status::close;
            break;
        case net::socket::recv_status::failed:
            c.status     = async_status::error;
            c.error_code = errno;
            break;
        case net::socket::recv_status::wait:
            c.status     = async_status::error;
            c.error_code = EAGAIN;
            break;
        }
        break;
    }
    case async_epoll::pending_op::write: {
        c.op = async_op::write;
        int rc = 0;
        auto ss = net::socket::send(fd, rc, static_cast<const char*>(op->w.buffer), static_cast<int>(op->w.len));
        switch (ss) {
        case net::socket::status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(rc);
            break;
        case net::socket::status::wait:
            c.status     = async_status::error;
            c.error_code = EAGAIN;
            break;
        case net::socket::status::failed:
            c.status     = async_status::error;
            c.error_code = errno;
            break;
        }
        break;
    }
    case async_epoll::pending_op::accept: {
        c.op = async_op::accept;
        net::fd_t newfd = net::retired_fd;
        auto as    = net::socket::accept(fd, newfd);
        switch (as) {
        case net::socket::status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(newfd);
            break;
        case net::socket::status::wait:
            c.status     = async_status::error;
            c.error_code = EAGAIN;
            break;
        case net::socket::status::failed:
            c.status     = async_status::error;
            c.error_code = errno;
            break;
        }
        break;
    }
    case async_epoll::pending_op::connect: {
        c.op = async_op::connect;
        int err = 0;
        if (net::socket::errcode(fd, err) && err == 0) {
            c.status = async_status::success;
        } else {
            c.status     = async_status::error;
            c.error_code = (err != 0) ? err : errno;
        }
        break;
    }
    }

    delete op;
    return c;
}

static int drain_epoll(int epfd, const span<io_completion>& completions, int timeout_ms) {
    constexpr int kMaxEvents = 64;
    struct epoll_event events[kMaxEvents];
    int nfds = epoll_wait(epfd, events, kMaxEvents, timeout_ms);
    if (nfds <= 0) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < nfds && count < static_cast<int>(completions.size()); ++i) {
        completions[count++] = handle_event(events[i]);
    }
    return count;
}

int async_epoll::poll(const span<io_completion>& completions) {
    int count = 0;

    // Drain synchronous completions first (file I/O, immediate connect).
    while (!m_sync_completions.empty() && count < static_cast<int>(completions.size())) {
        completions[count++] = m_sync_completions.front();
        m_sync_completions.erase(m_sync_completions.begin());
    }
    if (count >= static_cast<int>(completions.size())) {
        return count;
    }

    count += drain_epoll(m_epfd, span<io_completion>(completions.data() + count, completions.size() - count), 0);
    return count;
}

int async_epoll::wait(const span<io_completion>& completions, int timeout) {
    int count = 0;

    // Drain synchronous completions first.
    while (!m_sync_completions.empty() && count < static_cast<int>(completions.size())) {
        completions[count++] = m_sync_completions.front();
        m_sync_completions.erase(m_sync_completions.begin());
    }
    if (count > 0 || completions.size() == 0) {
        return count;
    }

    count += drain_epoll(m_epfd, completions, timeout);
    return count;
}

void async_epoll::stop() {
    if (m_epfd >= 0) {
        close(m_epfd);
        m_epfd = -1;
    }
}

}  // namespace bee::async
